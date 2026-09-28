#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace HC {
using Microsoft::WRL::ComPtr;
struct Treatment {
    std::array<float, 3> colour;
    float detail;
    bool operator==(const Treatment&) const = default;
};

// Transforms only diffuse RGB. Preserves source alpha and leaves normal maps alone.
// Render-thread owned; no writes to source textures or serialized game materials.
class TextureCache {
public:
    bool Initialize(ID3D11Device* device, std::string& error);
    ComPtr<ID3D11ShaderResourceView> Get(ID3D11DeviceContext* context,
        ID3D11ShaderResourceView* source, const Treatment& treatment);
    void Clear();
    std::uint32_t maxDimension = 256;
    std::size_t maxBytes = 256ull * 1024 * 1024;
    std::size_t Bytes() const { return bytes_; }
private:
    struct Entry {
        ComPtr<ID3D11ShaderResourceView> source, output;
        Treatment treatment;
        std::size_t bytes;
        std::uint64_t used;
    };
    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11ComputeShader> shader_;
    ComPtr<ID3D11Buffer> constants_;
    ComPtr<ID3D11SamplerState> sampler_;
    std::vector<Entry> entries_;
    std::size_t bytes_ = 0;
    std::uint64_t clock_ = 0;
};
}
