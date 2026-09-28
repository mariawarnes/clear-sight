#pragma once
#include <cstdint>

namespace HC {
enum class Category : std::uint8_t { World, Neutral, Ally, Enemy, Important };
constexpr bool DesaturateScenery(Category category) { return category == Category::World; }
constexpr Category Classify(bool actor, bool hostile, bool teammate, bool important, bool nonHostileBlue = false, bool dead = false)
{
    // Death overrides allegiance and quest status. Quest markers on people
    // must not turn their whole body into an item-colour highlight.
    if (actor && dead) return Category::Neutral;
    if (actor && hostile) return Category::Enemy;
    if (actor && teammate) return Category::Ally;
    if (!actor && important) return Category::Important;
    if (actor && nonHostileBlue) return Category::Ally;
    return actor ? Category::Neutral : Category::World;
}
}
