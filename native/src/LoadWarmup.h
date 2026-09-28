#pragma once
#include <cstdint>
namespace HC {
// Main-thread readiness window; any interruption or new load restarts it.
class LoadWarmup {
public:
    bool Poll(std::uint64_t generation, bool eligible, std::uint64_t now) {
        if (generation != generation_ || !eligible) {
            generation_ = generation;
            waiting_ = false;
        }
        if (!eligible) return false;
        if (!waiting_) { since_ = now; waiting_ = true; }
        return now - since_ >= 1500;
    }
private:
    std::uint64_t generation_ = 0, since_ = 0;
    bool waiting_ = false;
};
}
