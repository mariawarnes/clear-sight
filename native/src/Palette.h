#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cwctype>
#include <optional>
#include <sstream>
#include <string>

namespace HC {
using Colour = std::array<float, 3>;
inline constexpr Colour defaultWorld{0.06f, 0.06f, 0.06f};
inline constexpr Colour defaultAlly{0.15f, 0.55f, 1.0f};
inline constexpr Colour defaultEnemy{1.0f, 0.20f, 0.10f};
inline constexpr Colour defaultImportant{1.0f, 0.85f, 0.05f};
inline float LinearLuminance(const Colour& rgb) {
    return rgb[0]*0.2126f + rgb[1]*0.7152f + rgb[2]*0.0722f;
}
inline float ReferenceContrast(const Colour& a, const Colour& b) {
    auto luminance = [](Colour rgb) {
        for (auto& c : rgb) c = c <= 0.04045f ? c/12.92f : std::pow((c+0.055f)/1.055f, 2.4f);
        return LinearLuminance(rgb);
    };
    const float x = luminance(a), y = luminance(b);
    return (std::max(x,y)+0.05f)/(std::min(x,y)+0.05f);
}
inline std::optional<Colour> ParseColour(std::wstring value)
{
    const auto first = value.find_first_not_of(L" \t\r\n");
    if (first == std::wstring::npos) return {};
    value = value.substr(first, value.find_last_not_of(L" \t\r\n") - first + 1);
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    struct Named { const wchar_t* name; Colour colour; };
    // Default highlight presets contrast against defaultWorld. These are
    // reference material colours, not a guarantee after game post-processing.
    static constexpr Named presets[]{
        {L"blue", defaultAlly}, {L"red", defaultEnemy},
        {L"yellow", defaultImportant}, {L"green", {0.04f,0.60f,0.08f}},
        {L"cyan", {0.02f,0.55f,0.60f}}, {L"purple", {0.40f,0.04f,0.65f}},
        {L"magenta", {0.65f,0.03f,0.45f}}, {L"orange", {0.65f,0.20f,0.02f}},
        {L"white", {0.65f,0.65f,0.65f}}, {L"grey", {0.40f,0.40f,0.40f}},
        {L"gray", {0.40f,0.40f,0.40f}}, {L"black", {0,0,0}}
    };
    for (const auto& preset : presets) if (value == preset.name) return preset.colour;
    if (value.size() == 7 && value[0] == L'#') {
        Colour result{};
        for (std::size_t i = 0; i < 3; ++i) {
            unsigned byte = 0;
            for (std::size_t j = 0; j < 2; ++j) {
                const auto c = value[1 + 2*i + j];
                if (!((c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'f'))) return {};
                byte = byte * 16 + (c <= L'9' ? c - L'0' : c - L'a' + 10);
            }
            result[i] = byte / 255.0f;
        }
        return result;
    }
    std::replace(value.begin(), value.end(), L',', L' ');
    std::wistringstream stream(value);
    Colour result{};
    if (!(stream >> result[0] >> result[1] >> result[2])) return {};
    stream >> std::ws;
    if (!stream.eof() || !std::all_of(result.begin(), result.end(), [](float v) { return std::isfinite(v) && v >= 0 && v <= 1; })) return {};
    return result;
}
}
