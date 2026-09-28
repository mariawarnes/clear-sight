#include "TextureCache.h"
#include <d3dcompiler.h>
#include <algorithm>
#include <cmath>

namespace HC {
namespace {
constexpr char shaderSource[] = R"(
Texture2D<float4> source : register(t0);
SamplerState linearSampler : register(s0);
RWTexture2D<float4> outputTexture : register(u0);
cbuffer Settings : register(b0) {
    float3 colour; float detail;
    uint2 dimensions; float2 padding;
};
[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID) {
    if (any(id.xy >= dimensions)) return;
    float2 uv = (float2(id.xy) + 0.5) / float2(dimensions);
    float4 original = source.SampleLevel(linearSampler, uv, 0);
    float luma = dot(original.rgb, float3(0.2126, 0.7152, 0.0722));
    // Detail may darken the palette, never amplify it into clipped highlights.
    float shape = lerp(1.0, 0.65 + 0.35 * sqrt(saturate(luma)), detail);
    outputTexture[id.xy] = float4(saturate(colour * shape), original.a);
})";

// Preserve the exact compute slots touched, even when another mod uses compute.
struct ComputeScope {
    ID3D11DeviceContext* context;
    ComPtr<ID3D11ComputeShader> shader;
    ComPtr<ID3D11ShaderResourceView> srv;
    ComPtr<ID3D11UnorderedAccessView> uav;
    ComPtr<ID3D11Buffer> buffer;
    ComPtr<ID3D11SamplerState> sampler;
    std::array<ID3D11ClassInstance*, 256> instances{};
    UINT count = 256;
    explicit ComputeScope(ID3D11DeviceContext* c) : context(c) {
        c->CSGetShader(&shader, instances.data(), &count);
        c->CSGetShaderResources(0, 1, &srv);
        c->CSGetUnorderedAccessViews(0, 1, &uav);
        c->CSGetConstantBuffers(0, 1, &buffer);
        c->CSGetSamplers(0, 1, &sampler);
    }
    ~ComputeScope() {
        ID3D11UnorderedAccessView* empty = nullptr;
        context->CSSetUnorderedAccessViews(0, 1, &empty, nullptr);
        context->CSSetShaderResources(0, 1, srv.GetAddressOf());
        context->CSSetUnorderedAccessViews(0, 1, uav.GetAddressOf(), nullptr);
        context->CSSetConstantBuffers(0, 1, buffer.GetAddressOf());
        context->CSSetSamplers(0, 1, sampler.GetAddressOf());
        context->CSSetShader(shader.Get(), instances.data(), count);
        for (UINT i = 0; i < count; ++i) if (instances[i]) instances[i]->Release();
    }
};
}

bool TextureCache::Initialize(ID3D11Device* device, std::string& error)
{
    device_ = device;
    ComPtr<ID3DBlob> code, diagnostics;
    auto hr = D3DCompile(shaderSource, sizeof(shaderSource) - 1, "ClearSightDiffuse",
        nullptr, nullptr, "main", "cs_5_0", D3DCOMPILE_ENABLE_STRICTNESS, 0, &code, &diagnostics);
    if (FAILED(hr)) {
        error = diagnostics ? std::string(static_cast<char*>(diagnostics->GetBufferPointer()), diagnostics->GetBufferSize()) : "D3DCompile failed";
        return false;
    }
    if (FAILED(device->CreateComputeShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, &shader_))) {
        error = "CreateComputeShader failed"; return false;
    }
    D3D11_BUFFER_DESC buffer{};
    buffer.ByteWidth = 32;
    buffer.Usage = D3D11_USAGE_DEFAULT;
    buffer.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    if (FAILED(device->CreateBuffer(&buffer, nullptr, &constants_))) {
        error = "CreateBuffer failed"; return false;
    }
    D3D11_SAMPLER_DESC sampler{};
    sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler.MaxLOD = D3D11_FLOAT32_MAX;
    if (FAILED(device->CreateSamplerState(&sampler, &sampler_))) {
        error = "CreateSamplerState failed"; return false;
    }
    return true;
}

ComPtr<ID3D11ShaderResourceView> TextureCache::Get(ID3D11DeviceContext* context,
    ID3D11ShaderResourceView* source, const Treatment& treatment)
{
    if (!source || !shader_) return {};
    ++clock_;
    for (auto& entry : entries_) {
        if (entry.source.Get() == source && entry.treatment == treatment) {
            entry.used = clock_;
            return entry.output;
        }
    }
    D3D11_SHADER_RESOURCE_VIEW_DESC view{};
    source->GetDesc(&view);
    if (view.ViewDimension != D3D11_SRV_DIMENSION_TEXTURE2D) return {};
    ComPtr<ID3D11Resource> resource;
    source->GetResource(&resource);
    ComPtr<ID3D11Texture2D> input;
    if (FAILED(resource.As(&input))) return {};
    D3D11_TEXTURE2D_DESC desc{};
    input->GetDesc(&desc);
    // Never cache mutable render targets, depth buffers, arrays or multisampled resources.
    if (desc.ArraySize != 1 || desc.SampleDesc.Count != 1 ||
        (desc.BindFlags & (D3D11_BIND_RENDER_TARGET | D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_UNORDERED_ACCESS)) ||
        desc.Usage == D3D11_USAGE_DYNAMIC) return {};
    auto width = std::max(1u, desc.Width >> view.Texture2D.MostDetailedMip);
    auto height = std::max(1u, desc.Height >> view.Texture2D.MostDetailedMip);
    while (std::max(width, height) > std::max(8u, maxDimension)) {
        width = std::max(1u, width / 2); height = std::max(1u, height / 2);
    }
    std::size_t bytes = 0;
    for (auto w = width, h = height;; w = std::max(1u, w / 2), h = std::max(1u, h / 2)) {
        bytes += static_cast<std::size_t>(w) * h * 4;
        if (w == 1 && h == 1) break;
    }
    if (bytes > maxBytes) return {};
    // A visible material must receive its treatment on EVERY draw. Returning
    // the original on a budget miss creates flashing, and persistent starvation
    // when the scene's working set exceeds the cache. Build misses synchronously;
    // smaller derived textures keep both conversion cost and residency bounded.
    desc = {};
    desc.Width = width; desc.Height = height; desc.ArraySize = 1; desc.MipLevels = 0;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_RENDER_TARGET;
    desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
    ComPtr<ID3D11Texture2D> output;
    ComPtr<ID3D11ShaderResourceView> srv;
    ComPtr<ID3D11UnorderedAccessView> uav;
    if (FAILED(device_->CreateTexture2D(&desc, nullptr, &output)) ||
        FAILED(device_->CreateShaderResourceView(output.Get(), nullptr, &srv))) return {};
    D3D11_UNORDERED_ACCESS_VIEW_DESC uv{};
    uv.Format = desc.Format; uv.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
    if (FAILED(device_->CreateUnorderedAccessView(output.Get(), &uv, &uav))) return {};
    struct Settings { float colour[3]; float detail; UINT size[2]; float padding[2]; } settings{};
    std::copy(treatment.colour.begin(), treatment.colour.end(), settings.colour);
    settings.detail = treatment.detail; settings.size[0] = width; settings.size[1] = height;
    {
        ComputeScope restore(context);
        context->UpdateSubresource(constants_.Get(), 0, nullptr, &settings, 0, 0);
        context->CSSetShader(shader_.Get(), nullptr, 0);
        context->CSSetConstantBuffers(0, 1, constants_.GetAddressOf());
        context->CSSetSamplers(0, 1, sampler_.GetAddressOf());
        context->CSSetShaderResources(0, 1, &source);
        context->CSSetUnorderedAccessViews(0, 1, uav.GetAddressOf(), nullptr);
        context->Dispatch((width + 7) / 8, (height + 7) / 8, 1);
        ID3D11UnorderedAccessView* empty = nullptr;
        context->CSSetUnorderedAccessViews(0, 1, &empty, nullptr);
        context->GenerateMips(srv.Get());
    }
    while (!entries_.empty() && bytes_ + bytes > maxBytes) {
        auto victim = std::min_element(entries_.begin(), entries_.end(), [](auto& a, auto& b) { return a.used < b.used; });
        bytes_ -= victim->bytes;
        entries_.erase(victim);
    }
    entries_.push_back({source, srv, treatment, bytes, clock_});
    bytes_ += bytes;
    return srv;
}

void TextureCache::Clear() { entries_.clear(); bytes_ = 0; }
}
