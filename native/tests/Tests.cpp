#include "Classification.h"
#include "DoorTarget.h"
#include "LoadWarmup.h"
#include "TextureCache.h"
#include "FaceTreatment.h"
#include "GreyscaleShader.h"
#include "Palette.h"
#include "LightColourScope.h"
#include <d3dcompiler.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace HC;
void Require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }

ComPtr<ID3D11ShaderResourceView> MakeInput(ID3D11Device* device)
{
    // Red, blue, yellow and dark pixels with different opacity. Their original
    // hue must never choose the category; all should become the requested blue.
    const std::array<std::uint32_t, 4> pixels{0xFF0000FF, 0x80FF0000, 0x0000FFFF, 0xFF101010};
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = 2; desc.Height = 2; desc.ArraySize = desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_IMMUTABLE; desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA data{pixels.data(), 8, 0};
    ComPtr<ID3D11Texture2D> texture;
    Require(SUCCEEDED(device->CreateTexture2D(&desc, &data, &texture)), "Create input texture");
    ComPtr<ID3D11ShaderResourceView> view;
    Require(SUCCEEDED(device->CreateShaderResourceView(texture.Get(), nullptr, &view)), "Create input SRV");
    return view;
}

std::array<std::array<unsigned char, 4>, 4> ReadPixels(ID3D11Device* device, ID3D11DeviceContext* context, ID3D11ShaderResourceView* view)
{
    ComPtr<ID3D11Resource> resource; view->GetResource(&resource);
    ComPtr<ID3D11Texture2D> texture; Require(SUCCEEDED(resource.As(&texture)), "Output texture type");
    D3D11_TEXTURE2D_DESC desc{}; texture->GetDesc(&desc);
    desc.Usage = D3D11_USAGE_STAGING; desc.BindFlags = desc.MiscFlags = 0; desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> staging;
    Require(SUCCEEDED(device->CreateTexture2D(&desc, nullptr, &staging)), "Create readback texture");
    context->CopyResource(staging.Get(), texture.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};
    Require(SUCCEEDED(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped)), "Map output");
    std::array<std::array<unsigned char, 4>, 4> pixels{};
    for (UINT y = 0; y < 2; ++y) for (UINT x = 0; x < 2; ++x) {
        auto* p = static_cast<unsigned char*>(mapped.pData) + y * mapped.RowPitch + x * 4;
        std::copy_n(p, 4, pixels[y * 2 + x].data());
    }
    context->Unmap(staging.Get(), 0);
    return pixels;
}

void TestGreyscaleShaders(ID3D11Device* device, ID3D11DeviceContext* context, bool foliage = false, bool shadows = false)
{
    const char* vertex = "float4 main(uint id:SV_VertexID):SV_Position { return float4((id==2?3:-1),(id==1?3:-1),0,1); }";
    const char* pixel = R"(
        float4 main(float4 p:SV_Position):SV_Target0 {
            if (p.x > 1 && p.y > 1) discard;
            if (p.x < 1) return float4(1,0.2,0.05,0.35);
            return float4(0.04,0.2,0.9,0.8);
        })";
    ComPtr<ID3D11ShaderResourceView> leafTexture;
    if (foliage) {
        vertex = R"(struct O { float4 p:SV_Position; float3 tint:COLOR0; };
            O main(uint id:SV_VertexID) { O o; o.p=float4((id==2?3:-1),(id==1?3:-1),0,1); o.tint=float3(0.2,1,0.1); return o; })";
        pixel = R"(Texture2D<float4> leaf:register(t0);
            float4 main(float4 p:SV_Position, float3 tint:COLOR0):SV_Target0 {
                float4 texel = leaf.Load(int3(int2(p.xy),0));
                clip(texel.a - 0.5);
                return float4(texel.rgb*tint + float3(0.02,0.15,0.01),texel.a);
            })";
        leafTexture = MakeInput(device);
        context->PSSetShaderResources(0, 1, leafTexture.GetAddressOf());
    }
    ComPtr<ID3DBlob> vsCode, psCode;
    if (shadows) pixel = R"(
        float4 main(float4 p:SV_Position):SV_Target0 {
            float value = p.y < 1 ? (p.x < 1 ? 0.004 : 0.012) : (p.x < 1 ? 0.03 : 0.12);
            return float4(value.xxx,0.7);
        })";
    Require(SUCCEEDED(D3DCompile(vertex, strlen(vertex), nullptr, nullptr, nullptr, "main", "vs_5_0", 0, 0, &vsCode, nullptr)), "Compile test vertex shader");
    Require(SUCCEEDED(D3DCompile(pixel, strlen(pixel), nullptr, nullptr, nullptr, "main", "ps_5_0", 0, 0, &psCode, nullptr)), "Compile coloured effect shader");
    ComPtr<ID3D11VertexShader> vs;
    Require(SUCCEEDED(device->CreateVertexShader(vsCode->GetBufferPointer(), vsCode->GetBufferSize(), nullptr, &vs)), "Create test VS");
    ComPtr<ID3D11PixelShader> ps;
    Require(SUCCEEDED(device->CreatePixelShader(psCode->GetBufferPointer(), psCode->GetBufferSize(), nullptr, &ps)), "Create original PS");
    ps->SetPrivateData(shaderBytecodeId, static_cast<UINT>(psCode->GetBufferSize()), psCode->GetBufferPointer());
    GreyscaleShaders shaders;
    if (shadows) shaders.ConfigureShadows(0.035f, 0.06f);
    for (const char* variant : {
        "float4 main():SV_Target0 { return float4(1,0,0,0.4); }",
        "float3 main():SV_Target0 { return float3(1,0,0); }",
        "struct O { float4 a:SV_Target0; float4 b:SV_Target1; }; O main() { O o; o.a=float4(1,0,0,1); o.b=float4(0,0,1,1); return o; }"
    }) {
        ComPtr<ID3DBlob> code;
        Require(SUCCEEDED(D3DCompile(variant, strlen(variant), nullptr, nullptr, nullptr, "main", "ps_5_0", 0, 0, &code, nullptr)), "Compile shader output variant");
        auto patched = GreyscaleBytecode({static_cast<const std::uint32_t*>(code->GetBufferPointer()), code->GetBufferSize()/4});
        ComPtr<ID3D11PixelShader> check;
        Require(SUCCEEDED(device->CreatePixelShader(patched.data(), patched.size()*4, nullptr, &check)), "Validate no-temporary, RGB-only and multi-output shader variants");
    }
    auto grey = shaders.Get(device, ps.Get());
    Require(grey != nullptr, "D3D11 validates patched grayscale effect shader and checksum");
    Require(shaders.Get(device, ps.Get()) == grey, "Reuse grayscale shader");
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = desc.Height = 2; desc.MipLevels = desc.ArraySize = desc.SampleDesc.Count = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    ComPtr<ID3D11Texture2D> target; ComPtr<ID3D11RenderTargetView> rtv; ComPtr<ID3D11ShaderResourceView> srv;
    Require(SUCCEEDED(device->CreateTexture2D(&desc, nullptr, &target)), "Create effect test target");
    device->CreateRenderTargetView(target.Get(), nullptr, &rtv);
    device->CreateShaderResourceView(target.Get(), nullptr, &srv);
    context->OMSetRenderTargets(1, rtv.GetAddressOf(), nullptr);
    D3D11_VIEWPORT viewport{0,0,2,2,0,1}; context->RSSetViewports(1, &viewport);
    D3D11_RASTERIZER_DESC raster{}; raster.FillMode = D3D11_FILL_SOLID; raster.CullMode = D3D11_CULL_NONE;
    ComPtr<ID3D11RasterizerState> rs; device->CreateRasterizerState(&raster, &rs); context->RSSetState(rs.Get());
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(vs.Get(), nullptr, 0);
    const float clear[4]{};
    context->ClearRenderTargetView(rtv.Get(), clear);
    context->PSSetShader(ps.Get(), nullptr, 0); context->Draw(3, 0);
    auto original = ReadPixels(device, context, srv.Get());
    context->ClearRenderTargetView(rtv.Get(), clear);
    context->PSSetShader(grey.Get(), nullptr, 0); context->Draw(3, 0);
    auto treated = ReadPixels(device, context, srv.Get());
    if (!shadows) Require(original[0][0] != original[0][1], "Original shader renders coloured pixels");
    for (int i = 0; i < 4; ++i) {
        const float luma = original[i][0] * 0.2126f + original[i][1] * 0.7152f + original[i][2] * 0.0722f;
        Require(treated[i][0] == treated[i][1] && treated[i][1] == treated[i][2], "Final effect output is grey despite shader-generated colours");
        if (shadows) {
            const float inputs[]{0.004f,0.012f,0.03f,0.12f};
            const auto value = inputs[i];
            const float expected = std::max(value, std::min(0.06f, 0.035f+0.025f*std::pow(value/0.06f, 0.25f)));
            Require(std::abs(treated[i][0] - expected*255) <= 1.1f, "GPU shadow lift matches bounded curve");
            if (i < 3) Require(treated[i][0] > original[i][0], "Dark scenery is lifted");
            if (i < 3) {
                const float oldValue = 0.015f+0.045f*std::sqrt(value/0.06f);
                Require(treated[i][0] > std::round(oldValue*255), "Dungeon shadows are brighter than 0.3.9");
                Require(treated[i][0] <= std::ceil(0.06f*255), "Stronger lift keeps existing upper grey limit");
            }
            else Require(treated[i] == original[i], "Bright scenery is unchanged");
            if (i > 0) Require(treated[i][0] > treated[i-1][0], "Shadow gradations remain distinguishable");
        } else Require(std::abs(treated[i][0] - luma) <= 1.5f, "Effect brightness preserved");
        Require(treated[i][3] == original[i][3], "Effect alpha/discard preserved");
    }
    if (!shadows) Require(treated[foliage ? 2 : 3][3] == 0, "Discarded effect/leaf pixel remains invisible");
    // A following actor/quest draw sharing this shader must get its colours back.
    context->ClearRenderTargetView(rtv.Get(), clear);
    context->PSSetShader(ps.Get(), nullptr, 0); context->Draw(3, 0);
    Require(ReadPixels(device, context, srv.Get()) == original, "Restoring shared shader restores original coloured output");
    context->OMSetRenderTargets(0, nullptr, nullptr);
    context->PSSetShader(nullptr, nullptr, 0);
    ID3D11ShaderResourceView* empty = nullptr; context->PSSetShaderResources(0, 1, &empty);
    std::cout << "PASS: final-output shader greyscale, GPU validation, branching, alpha/discard preservation\n";
}

int main()
{
    LoadWarmup warmup;
    Require(!warmup.Poll(1, false, 100), "Loading/failed saves never activate treatment");
    Require(!warmup.Poll(1, true, 200), "Load notification alone does not resume immediately");
    Require(!warmup.Poll(1, true, 1699), "Readiness must remain stable for full window");
    Require(warmup.Poll(1, true, 1700), "Stable ready world resumes");
    Require(!warmup.Poll(2, true, 1800), "Second save load resets readiness");
    Require(!warmup.Poll(2, false, 3000), "Loading menu interruption resets readiness");
    Require(!warmup.Poll(2, true, 4000), "Readiness starts afresh after interruption");
    Require(warmup.Poll(2, true, 5500), "Ready after interrupted load");
    // Distances from the vanilla MQ101 marker/door records in Skyrim.esm.
    NearestQuestDoor hadvarDoor;
    hadvarDoor.Consider(155570, 258.2709f * 258.2709f, true);
    hadvarDoor.Consider(596480, 117.2681f * 117.2681f, true);
    hadvarDoor.Consider(1, 1, false);
    Require(hadvarDoor.id == 596480, "Helgen marker chooses closest door in same space");
    NearestQuestDoor ralofDoor;
    ralofDoor.Consider(155566, 328.9310f * 328.9310f, true);
    ralofDoor.Consider(384769, 313.4675f * 313.4675f, true);
    Require(ralofDoor.id == 384769, "Elevated Ralof marker still finds its door");
    NearestQuestDoor distantDoor;
    distantDoor.Consider(1, 400.0f * 400.0f, true);
    Require(distantDoor.id == 0, "Unrelated distant door remains unmarked");
    try {
        Require(DesaturateScenery(Category::World), "Scenery including foliage receives final greyscale");
        for (auto category : {Category::Neutral, Category::Ally, Category::Enemy, Category::Important})
            Require(!DesaturateScenery(category), "Character and quest palettes bypass scenery greyscale");
        for (Colour source : {Colour{1,0.25f,0.01f}, Colour{0.05f,0.2f,2.0f}, Colour{0.8f,0.02f,1.2f}}) {
            auto light = source;
            const float expected = source[0]*0.2126f + source[1]*0.7152f + source[2]*0.0722f;
            {
                LightColourScope scope;
                scope.Add(light[0], light[1], light[2]);
                scope.Add(light[0], light[1], light[2]);
                Require(light[0] == light[1] && light[1] == light[2], "Fire and magic lights upload neutral RGB");
                Require(std::abs(light[0]-expected) < 0.00001f, "Local light luminance preserved including HDR");
                // Uniform lighting scales a category colour without changing its hue.
                const Colour palette{0.04f,0.2f,0.65f};
                Require(std::abs((palette[2]*light[2])/(palette[0]*light[0]) - palette[2]/palette[0]) < 0.0001f,
                    "Neutral local lighting preserves character palette ratios");
            }
            Require(light == source, "Original light restored even when referenced twice");
        }
        Require(ParseColour(L" blue ") == std::optional<Colour>{defaultAlly}, "Blue preset matches default ally colour");
        Require(ParseColour(L"RED") == std::optional<Colour>{defaultEnemy}, "Colour names ignore case");
        Require(ParseColour(L"Yellow") == std::optional<Colour>{defaultImportant}, "Yellow preset matches quest colour");
        for (auto colour : {defaultAlly, defaultEnemy, defaultImportant}) {
            const auto contrast = ReferenceContrast(colour, defaultWorld);
            Require(contrast >= 3, "Default reference swatches meet 3:1 using sRGB relative luminance");
            // Also check the material's linear RGB under darkest default facial
            // detail (1 - 0.35*0.35), UNORM quantization, and unlit world albedo.
            auto shaded = colour;
            for (auto& c : shaded) c = std::floor(c * 0.8775f * 255) / 255;
            const float materialRatio = (LinearLuminance(shaded)+0.05f)/(LinearLuminance(defaultWorld)+0.05f);
            Require(materialRatio >= 3, "Default material palette retains 3:1 under default detail and quantization");
            std::cout << "Default palette: reference=" << contrast << ":1; material/detail=" << materialRatio << ":1\n";
        }
        Require(ParseColour(L"#FF0080") == std::optional<Colour>{{1,0,128.0f/255}}, "Custom hex RGB");
        for (auto invalid : {L"", L"notacolour", L"#GG0011", L"#FFF", L"1,0,0 junk", L"2,0,0", L"-1,0,0", L"1,0"})
            Require(!ParseColour(invalid), "Invalid colour rejected");
        Require(Classify(true, true, true, true) == Category::Enemy, "Hostility priority");
        Require(Classify(true, false, true, true) == Category::Ally, "Teammate classification");
        Require(Classify(true, false, false, false) == Category::Neutral, "Neutral actor classification");
        Require(Classify(false, false, false, true) == Category::Important, "Quest reference classification");
        Require(Classify(false, true, true, false) == Category::World, "Non-actor flags ignored");
        Require(Classify(true, false, false, false, true) == Category::Ally, "Non-hostile NPC blue option");
        Require(Classify(true, true, false, false, true) == Category::Enemy, "Hostility still overrides non-hostile blue option");
        Require(Classify(true, false, false, true, true) == Category::Ally, "Follow target stays blue");
        Require(Classify(true, false, false, true, false) == Category::Neutral, "Quest target retains neutral colour when blue option is disabled");
        for (int flags = 0; flags < 16; ++flags) {
            Require(Classify(true, flags & 1, flags & 2, flags & 4, flags & 8, true) == Category::Neutral,
                "Death overrides hostility, teammate, quest and non-hostile-blue flags");
        }
        Require(Classify(true, true, false, true, true, false) == Category::Enemy, "Living hostile quest actor stays red");
        Require(Classify(false, false, false, true, true, false) == Category::Important, "Quest objects remain yellow");
        Require(Classify(false, false, false, false, true) == Category::World, "Blue option does not affect scenery");
        ComPtr<ID3D11Device> device; ComPtr<ID3D11DeviceContext> context;
        D3D_FEATURE_LEVEL level{};
        Require(SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0,
            D3D11_SDK_VERSION, &device, &level, &context)), "Create WARP D3D11 device");
        TextureCache cache; std::string error;
        Require(cache.Initialize(device.Get(), error), error.c_str());
        auto source = MakeInput(device.Get());
        // Ensure our compute dispatch restores existing pipeline bindings.
        context->CSSetShaderResources(0, 1, source.GetAddressOf());
        D3D11_BUFFER_DESC oldBufferDesc{};
        oldBufferDesc.ByteWidth = 16; oldBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        ComPtr<ID3D11Buffer> oldBuffer;
        Require(SUCCEEDED(device->CreateBuffer(&oldBufferDesc, nullptr, &oldBuffer)), "Create prior constant buffer");
        context->CSSetConstantBuffers(0, 1, oldBuffer.GetAddressOf());
        D3D11_SAMPLER_DESC oldSamplerDesc{};
        oldSamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
        oldSamplerDesc.AddressU = oldSamplerDesc.AddressV = oldSamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        ComPtr<ID3D11SamplerState> oldSampler;
        Require(SUCCEEDED(device->CreateSamplerState(&oldSamplerDesc, &oldSampler)), "Create prior sampler");
        context->CSSetSamplers(0, 1, oldSampler.GetAddressOf());
        Treatment blue{{0.1f,0.4f,0.9f}, 0};
        auto output = cache.Get(context.Get(), source.Get(), blue);
        Require(output != nullptr, "Generate treated texture");
        ComPtr<ID3D11ShaderResourceView> restored;
        context->CSGetShaderResources(0, 1, &restored);
        Require(restored.Get() == source.Get(), "Compute SRV state restored");
        ComPtr<ID3D11ComputeShader> shader;
        context->CSGetShader(&shader, nullptr, nullptr);
        Require(shader == nullptr, "Compute shader state restored");
        ComPtr<ID3D11Buffer> restoredBuffer;
        context->CSGetConstantBuffers(0, 1, &restoredBuffer);
        Require(restoredBuffer.Get() == oldBuffer.Get(), "Constant buffer restored");
        ComPtr<ID3D11SamplerState> restoredSampler;
        context->CSGetSamplers(0, 1, &restoredSampler);
        Require(restoredSampler.Get() == oldSampler.Get(), "Sampler restored");
        ComPtr<ID3D11UnorderedAccessView> restoredUAV;
        context->CSGetUnorderedAccessViews(0, 1, &restoredUAV);
        Require(restoredUAV == nullptr, "Output UAV unbound after dispatch");
        auto pixels = ReadPixels(device.Get(), context.Get(), output.Get());
        const unsigned char expectedAlpha[]{255,128,0,255};
        for (std::size_t i = 0; i < 4; ++i) {
            Require(std::abs(static_cast<int>(pixels[i][0]) - 26) <= 1 &&
                std::abs(static_cast<int>(pixels[i][1]) - 102) <= 1 &&
                std::abs(static_cast<int>(pixels[i][2]) - 230) <= 1, "Source hue cannot affect category colour");
            Require(pixels[i][3] == expectedAlpha[i], "Preserve alpha for hair/foliage");
        }
        Require(cache.Get(context.Get(), source.Get(), blue).Get() == output.Get(), "Reuse cached transformation");
        Treatment red{{0.9f,0.1f,0.05f}, 0};
        auto hostile = cache.Get(context.Get(), source.Get(), red);
        Require(hostile && hostile.Get() != output.Get(), "Shared source supports different actor categories");
        auto original = ReadPixels(device.Get(), context.Get(), source.Get());
        Require(original[0][0] == 255 && original[0][1] == 0, "Source material remains unchanged");
        blue.detail = 0.6f;
        auto detailed = cache.Get(context.Get(), source.Get(), blue);
        auto detailPixels = ReadPixels(device.Get(), context.Get(), detailed.Get());
        Require(detailPixels[2][2] > detailPixels[3][2], "Facial luminance detail retains brightness differences");
        Require(detailPixels[1][3] == 128, "Detail retains alpha");
        for (const auto& pixel : detailPixels) {
            Require(pixel[0] <= 27 && pixel[1] <= 103 && pixel[2] <= 231,
                "Detail never brightens the selected palette");
        }
        auto tintOutput = cache.Get(context.Get(), source.Get(), faceTint);
        auto neutralDetailOutput = cache.Get(context.Get(), source.Get(), faceDetailNeutral);
        auto tintPixels = ReadPixels(device.Get(), context.Get(), tintOutput.Get());
        auto neutralDetailPixels = ReadPixels(device.Get(), context.Get(), neutralDetailOutput.Get());
        // Check GPU-produced auxiliary maps against Skyrim's facegen albedo
        // equation. A face should match the same base albedo used for hair/body.
        for (float base : {0.04f, 0.20f, 0.40f, 0.65f}) {
            for (int channel = 0; channel < 3; ++channel) {
                const float tint = tintPixels[0][channel] / 255.0f;
                const float detail = neutralDetailPixels[0][channel] / 255.0f;
                const float bias = channel == 1 ? 0.0f : 1.0f / 255;
                const float face = (base * base + 2 * tint * base * (1 - base)) *
                    3.984375f * (bias + detail);
                Require(std::abs(face - base) < 0.005f, "Facegen maps preserve body/hair albedo instead of overexposing skin");
            }
        }
        cache.Clear(); Require(cache.Bytes() == 0, "Cache clears on game reload");
        // A scene with more than the old 256-entry limit must stay treated on
        // every draw, including an initial burst exceeding four cache misses.
        std::vector<ComPtr<ID3D11ShaderResourceView>> sceneSources, sceneOutputs;
        for (int i = 0; i < 300; ++i) {
            sceneSources.push_back(MakeInput(device.Get()));
            auto treated = cache.Get(context.Get(), sceneSources.back().Get(), red);
            Require(treated != nullptr, "Every first-use material is treated without a time-budget fallback");
            sceneOutputs.push_back(treated);
        }
        for (int frame = 0; frame < 3; ++frame) {
            for (std::size_t i = 0; i < sceneSources.size(); ++i) {
                auto treated = cache.Get(context.Get(), sceneSources[i].Get(), red);
                Require(treated.Get() == sceneOutputs[i].Get(), "Scene working set remains resident beyond 256 materials");
            }
        }
        // Force memory eviction: even an over-capacity scene must not flash its
        // original materials. A 2x2 output with mipmaps occupies 20 bytes.
        cache.Clear();
        cache.maxBytes = 40;
        for (int frame = 0; frame < 2; ++frame) {
            for (std::size_t i = 0; i < 12; ++i) {
                auto treated = cache.Get(context.Get(), sceneSources[i].Get(), red);
                Require(treated != nullptr, "Eviction never falls back to the original texture");
                Require(cache.Bytes() <= cache.maxBytes, "Eviction respects byte limit");
                auto pixel = ReadPixels(device.Get(), context.Get(), treated.Get())[0];
                Require(std::abs(static_cast<int>(pixel[0]) - 230) <= 1 &&
                    std::abs(static_cast<int>(pixel[1]) - 26) <= 1, "Evicted material regenerates the correct colour");
            }
        }
        std::cout << "PASS: classification, real HLSL compilation, WARP dispatch, alpha, hue isolation, cache, detail, state restoration\n";
        std::cout << "PASS: 300-material scene residency, repeated draws, synchronous misses, forced memory eviction without original-colour fallback\n";
        std::cout << "PASS: non-hostile blue policy, hostile priority, bounded detail, neutral facegen map composition\n";
        TestGreyscaleShaders(device.Get(), context.Get());
        TestGreyscaleShaders(device.Get(), context.Get(), true);
        TestGreyscaleShaders(device.Get(), context.Get(), false, true);
        return 0;
    } catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
