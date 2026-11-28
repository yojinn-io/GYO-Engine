#include "object_fps_pvp/world/GameplaySets.hpp"

#include "object_fps_pvp/world/Interaction.hpp"

#include <algorithm>
#include <utility>

namespace object_fps_pvp::world {

void HashValue(K::Hasher&, const ActiveMode&) {}

const std::vector<GameplaySet>& GameSets() {
    // The core PvP is the game without sets. W1, G6 and PV3 add theirs here.
    static const std::vector<GameplaySet> sets{InteractionSet()};
    return sets;
}

std::string GameplaySetFixed(const std::string_view set) { return "pvp." + std::string(set) + ".fixed"; }
std::string GameplaySetFrame(const std::string_view set) { return "pvp." + std::string(set) + ".frame"; }

std::optional<SetBlocks> ReadSetBlocks(const GYO::Engine::Json::ObjectView& blocks, const std::vector<GameplaySet>& sets,
                                       std::string& error) {
    error.clear();
    SetBlocks read;
    for (const auto member : blocks) {
        const std::string name(member.key);
        const auto found = std::find_if(sets.begin(), sets.end(), [&](const GameplaySet& set) { return set.name == name; });
        if (found == sets.end()) {
            error = "the game has no gameplay set '" + name + "'";
            return std::nullopt;
        }
        const std::size_t slot = static_cast<std::size_t>(found - sets.begin());
        if (std::any_of(read.begin(), read.end(), [&](const auto& block) { return block.first == slot; })) {
            error = "sets names '" + name + "' twice";
            return std::nullopt;
        }
        const auto block = member.value.AsObject();
        if (!block) {
            error = "the block of set '" + name + "' is not an object";
            return std::nullopt;
        }
        const SetReader& reader = found->readMode;
        if (!reader) {
            if (block->Size() != 0) {
                error = "the set '" + name + "' takes no settings";
                return std::nullopt;
            }
            read.emplace_back(slot, SetData{});
            continue;
        }
        std::string why;
        auto data = reader(*block, why);
        if (!data) {
            error = "set '" + name + "': " + why;
            return std::nullopt;
        }
        read.emplace_back(slot, std::move(*data));
    }
    return read;
}

bool SetsOn::Has(const std::size_t slot) const { return std::find(slots.begin(), slots.end(), slot) != slots.end(); }

void SetsOn::Add(const std::size_t slot) {
    if (!Has(slot)) slots.push_back(slot);
}

namespace {

// The slot is the condition's value (Kernel::Condition<&Fn>(name, value)): the set the mode may turn on.
bool SetOn(const std::size_t& slot, K::Res<ActiveMode> mode) { return mode->on.Has(slot); }

} // namespace

K::ConditionRef GameplaySetOn(const std::size_t slot, const std::string& name) {
    return K::Condition<&SetOn>("pvp.mode." + name, slot);
}

} // namespace object_fps_pvp::world
