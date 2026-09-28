#pragma once
#include <cstdint>

namespace HC {
// XMarkers can sit above/in front of the door rather than at its origin.
struct NearestQuestDoor {
    std::uint32_t id = 0;
    float distanceSquared = 384.0f * 384.0f;
    void Consider(std::uint32_t candidate, float squared, bool sameSpace) {
        if (sameSpace && squared >= 0 && squared < distanceSquared) {
            id = candidate;
            distanceSquared = squared;
        }
    }
};
}
