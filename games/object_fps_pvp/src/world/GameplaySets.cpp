#include "object_fps_pvp/world/GameplaySets.hpp"

#include "engine/base/Assert.hpp"

#include <algorithm>
#include <array>
#include <utility>

namespace object_fps_pvp::world {

void HashValue(K::Hasher&, const ActiveMode&) {}

const std::vector<GameplaySet>& GameSets() {
    // None yet: the core PvP is the game without sets. W1, the doors, G6 and PV3 add theirs here.
    static const std::vector<GameplaySet> sets;
    return sets;
}

std::string GameplaySetFixed(const std::string_view set) { return "pvp." + std::string(set) + ".fixed"; }
std::string GameplaySetFrame(const std::string_view set) { return "pvp." + std::string(set) + ".frame"; }

std::optional<SetBlocks> ReadSetBlocks(const GYO::Engine::Json::ObjectView& blocks, const std::vector<GameplaySet>& sets, const bool arena,
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
        const SetReader& reader = arena ? found->readArena : found->readMode;
        if (!reader) {
            if (block->Size() != 0) {
                error = "the set '" + name + "' takes no " + (arena ? "arena data" : "settings");
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

std::optional<SetBlocks> ReadArenaSets(const std::string_view arenaText, const std::vector<GameplaySet>& sets, std::string& error) {
    error.clear();
    auto document = GYO::Engine::Json::Parse(arenaText);
    if (!document) {
        error = "the arena does not parse";
        return std::nullopt;
    }
    const auto root = document->Root().AsObject();
    if (!root) {
        error = "the arena is not an object";
        return std::nullopt;
    }
    const auto blocks = root->Find("sets");
    if (!blocks) return SetBlocks{};
    const auto object = blocks->AsObject();
    if (!object) {
        error = "sets is not an object";
        return std::nullopt;
    }
    return ReadSetBlocks(*object, sets, true, error);
}

namespace {

// A condition cannot capture (Kernel::Condition): the slot is its template argument.
template <std::size_t Slot>
bool SetOn(K::Res<ActiveMode> mode) {
    return mode->on.test(Slot);
}

using MakeCondition = K::ConditionRef (*)(std::string);

template <std::size_t... Slots>
constexpr std::array<MakeCondition, sizeof...(Slots)> Conditions(std::index_sequence<Slots...>) {
    return {+[](std::string name) { return K::Condition<&SetOn<Slots>>(std::move(name)); }...};
}

constexpr auto kConditions = Conditions(std::make_index_sequence<kMaxGameplaySets>{});

} // namespace

K::ConditionRef GameplaySetOn(const std::size_t slot, const std::string& name) {
    GYO_ASSERT(slot < kMaxGameplaySets);
    return kConditions[slot]("pvp.mode." + name);
}

} // namespace object_fps_pvp::world
