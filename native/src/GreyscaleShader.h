#pragma once
#include "TextureCache.h"
#include <span>
#include <unordered_map>

namespace HC {
// Retains vanilla shader logic, alpha/discard, depth and all outputs except RGB0.
std::vector<std::uint32_t> GreyscaleBytecode(std::span<const std::uint32_t> input, float shadowFloor = 0, float shadowRange = 0.06f);
inline constexpr GUID shaderBytecodeId{0xb9dd4617,0x3ed9,0x4e0f,{0x97,0x51,0x5b,0x44,0x79,0x17,0x10,0x72}};
class GreyscaleShaders {
public:
    void ConfigureShadows(float floor, float range) { entries_.clear(); shadowFloor_ = floor; shadowRange_ = range; }
    ComPtr<ID3D11PixelShader> Get(ID3D11Device* device, ID3D11PixelShader* source);
private:
    struct Entry { ComPtr<ID3D11PixelShader> original, grey; };
    std::unordered_map<ID3D11PixelShader*, Entry> entries_;
    float shadowFloor_ = 0, shadowRange_ = 0.06f;
};
}
