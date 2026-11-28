#pragma once

// The interaction set (interact-door declaration section 2): a player presses E (PlayerInput's kPressInteract), the
// Match casts that step's view (the shot's ray) and, when the first collider it meets within reach is Interactable,
// writes Interacted. What an interaction does is the kind's own set: it reads Interacted and looks at its own
// component on the target (the doors: DoorState). The skeleton knows no kind, no prompt and no range of a target
// (Bevy's picking: the hit in one place, each author's System looks at the target's components).

#include "object_fps_pvp/world/GameplaySets.hpp"

#include "engine/kernel/Kernel.hpp"

#include <cstdint>

namespace object_fps_pvp::world {

// A target of the interaction: what kind it is is the other components on the entity.
struct Interactable final {};
void HashValue(K::Hasher&, const Interactable&);

// How far a player reaches (metres from the eye), from the mode configuration's sets.interaction.reach.
struct InteractionRules final {
    float reach = 1.5F;
};
void HashValue(K::Hasher&, const InteractionRules&);

// A player used a target at this step (a decision message: written by the Match and a Local World, in ascending
// actor order).
struct Interacted final {
    K::EntityId actor = 0;
    K::EntityId target = 0;
    std::uint64_t tick = 0;
};

// The set's name, its table entry and the name of its judging System (a kind's System runs after it).
inline constexpr const char* kInteractionSet = "interaction";
inline constexpr const char* kJudgeInteractions = "JudgeInteractions";
[[nodiscard]] GameplaySet InteractionSet();

} // namespace object_fps_pvp::world
