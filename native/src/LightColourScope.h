#pragma once
#include <algorithm>
#include <vector>

namespace HC {
// Temporarily neutralize local-light RGB while vanilla uploads draw constants.
// Keep brightness (including HDR values), attenuation and shadow data intact.
class LightColourScope {
public:
    LightColourScope() = default;
    LightColourScope(const LightColourScope&) = delete;
    LightColourScope& operator=(const LightColourScope&) = delete;
    void Add(float& r, float& g, float& b) {
        if (std::any_of(saved_.begin(), saved_.end(), [&](const auto& s) { return s.r == &r; })) return;
        saved_.push_back({&r, &g, &b, r, g, b});
        const float luminance = r * 0.2126f + g * 0.7152f + b * 0.0722f;
        r = g = b = luminance;
    }
    ~LightColourScope() {
        for (const auto& s : saved_) { *s.r = s.red; *s.g = s.green; *s.b = s.blue; }
    }
private:
    struct Saved { float* r; float* g; float* b; float red, green, blue; };
    std::vector<Saved> saved_;
};
}
