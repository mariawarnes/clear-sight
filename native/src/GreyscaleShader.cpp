#include "GreyscaleShader.h"
#include "../third_party/dxc/d3d12TokenizedProgramFormat.hpp"
#include "../third_party/dxc/DxilHash.h"
#include <bit>
#include <cstring>
#include <stdexcept>

namespace HC {
namespace {
void Check(bool condition) { if (!condition) throw std::runtime_error("Unsupported or malformed pixel shader"); }
constexpr std::uint32_t FourCC(char a, char b, char c, char d) {
    return a | (b << 8) | (c << 16) | (d << 24);
}
std::vector<std::uint32_t> Patch(std::span<const std::uint32_t> code, float shadowFloor, float shadowRange) {
    Check(code.size() >= 2 && code[1] == code.size() && (code[0] >> 16) == 0);
    std::uint32_t temp = 0;
    bool hasTemps = false;
    std::uint32_t outputMask = 0;
    for (std::size_t p = 2; p < code.size();) {
        const auto op = code[p] & 0x7ff;
        const auto n = op == D3D10_SB_OPCODE_CUSTOMDATA ? (p + 1 < code.size() ? code[p+1] : 0) : (code[p] >> 24) & 0x7f;
        Check(n && p + n <= code.size());
        if (op == D3D10_SB_OPCODE_DCL_TEMPS) { Check(n == 2); temp = code[p+1]; hasTemps = true; }
        if (op == D3D10_SB_OPCODE_DCL_OUTPUT && n == 3 && code[p+2] == 0) outputMask = (code[p+1] >> 4) & 15;
        // Subroutine returns need separate handling; Skyrim effect shaders use main only.
        Check(op != D3D10_SB_OPCODE_CALL && op != D3D10_SB_OPCODE_CALLC && op != D3D10_SB_OPCODE_LABEL);
        p += n;
    }
    const bool lift = shadowFloor > 0;
    const unsigned extraTemps = lift ? 2 : 1;
    Check((outputMask & 7) == 7 && temp + extraTemps <= 4096);
    std::vector<std::uint32_t> result{code[0], 0};
    if (!hasTemps) result.insert(result.end(), {D3D10_SB_OPCODE_DCL_TEMPS | (2u << 24), extraTemps});
    for (std::size_t p = 2; p < code.size();) {
        const auto op = code[p] & 0x7ff;
        const auto n = op == D3D10_SB_OPCODE_CUSTOMDATA ? code[p+1] : (code[p] >> 24) & 0x7f;
        if (op == D3D10_SB_OPCODE_RET || op == D3D10_SB_OPCODE_RETC) {
            // mov o0, rN preserves alpha; dp3 touches RGB only, after every tint,
            // vertex colour, lighting calculation and hardcoded blood colour.
            result.insert(result.end(), {D3D10_SB_OPCODE_MOV | (5u << 24), 0x00102002 | (outputMask << 4), 0, 0x00100e46, temp,
                D3D10_SB_OPCODE_DP3 | (10u << 24), lift ? 0x00100012u : 0x00102072u, lift ? temp+1 : 0, 0x00100e46, temp, 0x00004002,
                std::bit_cast<std::uint32_t>(0.2126f), std::bit_cast<std::uint32_t>(0.7152f),
                std::bit_cast<std::uint32_t>(0.0722f), 0});
            if (lift) {
                const auto scratch = temp + 1;
                auto binary = [&](unsigned op, unsigned dest, unsigned src, float value) {
                    result.insert(result.end(), {op | (7u << 24), dest, scratch, src, scratch,
                        0x00004001, std::bit_cast<std::uint32_t>(value)});
                };
                // y = floor + (range-floor)*pow(max(luma,0)/range, 0.25).
                // Clamp the lift to range and never darken brighter scenery.
                binary(D3D10_SB_OPCODE_MAX, 0x00100022, 0x00100006, 0);
                binary(D3D10_SB_OPCODE_MUL, 0x00100022, 0x00100556, 1/shadowRange);
                result.insert(result.end(), {D3D10_SB_OPCODE_SQRT | (5u << 24), 0x00100022, scratch, 0x00100556, scratch});
                result.insert(result.end(), {D3D10_SB_OPCODE_SQRT | (5u << 24), 0x00100022, scratch, 0x00100556, scratch});
                binary(D3D10_SB_OPCODE_MUL, 0x00100022, 0x00100556, shadowRange-shadowFloor);
                binary(D3D10_SB_OPCODE_ADD, 0x00100022, 0x00100556, shadowFloor);
                binary(D3D10_SB_OPCODE_MIN, 0x00100022, 0x00100556, shadowRange);
                result.insert(result.end(), {D3D10_SB_OPCODE_MAX | (7u << 24), 0x00102072, 0,
                    0x00100006, scratch, 0x00100556, scratch});
            }
        }
        const auto start = result.size();
        result.insert(result.end(), code.begin() + p, code.begin() + p + n);
        if (op == D3D10_SB_OPCODE_DCL_TEMPS) result[start+1] = temp + extraTemps;
        const bool declaration = (op >= D3D10_SB_OPCODE_DCL_RESOURCE && op <= D3D10_SB_OPCODE_DCL_GLOBAL_FLAGS) ||
            op == D3D10_SB_OPCODE_CUSTOMDATA;
        if (!declaration) {
            std::size_t operand = 1;
            while (operand < n && (code[p+operand-1] & 0x80000000u)) ++operand;
            if (operand + 1 < n && ((code[p+operand] >> 12) & 0xff) == D3D10_SB_OPERAND_TYPE_OUTPUT) {
                // Only immediate one-dimensional output register 0 is supported.
                Check((code[p+operand] & 0xfff00000u) == 0x00100000u);
                if (code[p+operand+1] == 0) {
                    result[start+operand] &= ~0x000ff000u;
                    result[start+operand+1] = temp;
                }
            }
        }
        p += n;
    }
    result[1] = static_cast<std::uint32_t>(result.size());
    return result;
}
}
std::vector<std::uint32_t> GreyscaleBytecode(std::span<const std::uint32_t> input, float shadowFloor, float shadowRange) {
    Check(shadowFloor >= 0 && shadowFloor < shadowRange && shadowRange <= 1);
    Check(input.size() >= 8 && input[0] == FourCC('D','X','B','C') && input[6] == input.size_bytes());
    const auto count = input[7];
    Check(count <= input.size() - 8);
    std::vector<std::uint32_t> output(input.begin(), input.begin() + 8 + count);
    bool patched = false;
    for (std::size_t i = 0; i < count; ++i) {
        Check(input[8+i] % 4 == 0);
        const auto at = input[8+i] / 4;
        Check(at + 2 <= input.size() && input[at+1] % 4 == 0);
        const auto length = input[at+1] / 4;
        Check(length <= input.size() - at - 2);
        output[8+i] = static_cast<std::uint32_t>(output.size() * 4);
        if (input[at] == FourCC('S','H','D','R') || input[at] == FourCC('S','H','E','X')) {
            auto code = Patch(input.subspan(at + 2, length), shadowFloor, shadowRange);
            output.push_back(input[at]); output.push_back(static_cast<std::uint32_t>(code.size() * 4));
            output.insert(output.end(), code.begin(), code.end()); patched = true;
        } else output.insert(output.end(), input.begin() + at, input.begin() + at + 2 + length);
    }
    Check(patched);
    output[6] = static_cast<std::uint32_t>(output.size() * 4);
    ComputeHashRetail(reinterpret_cast<const BYTE*>(output.data()) + 20, output[6] - 20,
        reinterpret_cast<BYTE*>(output.data()) + 4);
    return output;
}
ComPtr<ID3D11PixelShader> GreyscaleShaders::Get(ID3D11Device* device, ID3D11PixelShader* source) {
    if (!source) return {};
    if (auto it = entries_.find(source); it != entries_.end()) return it->second.grey;
    Entry entry{source, {}};
    UINT size = 0;
    if (SUCCEEDED(source->GetPrivateData(shaderBytecodeId, &size, nullptr)) && size && size % 4 == 0) {
        std::vector<std::uint32_t> bytes(size / 4);
        if (SUCCEEDED(source->GetPrivateData(shaderBytecodeId, &size, bytes.data()))) {
            try {
                const auto patched = GreyscaleBytecode(bytes, shadowFloor_, shadowRange_);
                device->CreatePixelShader(patched.data(), patched.size() * 4, nullptr, &entry.grey);
            } catch (const std::exception&) {}
        }
    }
    auto result = entry.grey;
    entries_.emplace(source, std::move(entry));
    return result;
}
}
