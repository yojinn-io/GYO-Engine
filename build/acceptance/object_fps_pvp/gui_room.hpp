// The room of a two-role GUI probe run (create and join). It may also hold
// passive bots (quad probe --passive): the room then has more players than the
// two GUI roles, and each role finds the other GUI, its peer, by the player ID
// that role publishes. A two-player room needs no publication: its only remote
// is the peer, so every check below equals the former "exactly two players".
// Bots first walk off the line between the GUI spawns; the room counts as full
// only after they mark themselves parked (BotsParkedFile).
#pragma once

#include "RetroFPS/Pvp/ClientConnection.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

class GuiRoom final {
public:
    GuiRoom(std::filesystem::path output, std::string role, unsigned players)
        : output_(std::move(output)), role_(std::move(role)), players_(players) {
        if (players_ < 2 || players_ > fps::pvp::MaxPlayers) throw std::invalid_argument("--room-players must be 2..room capacity");
        if (role_ != "create" && role_ != "join") throw std::invalid_argument("GUI role must be create or join");
    }
    [[nodiscard]] unsigned Players() const noexcept { return players_; }
    [[nodiscard]] bool WithBots() const noexcept { return players_ > 2; }
    static constexpr const char* BotsParkedFile = "bots-parked.txt";
    [[nodiscard]] std::filesystem::path PublishedPath(const std::string& role) const {
        return output_ / (role + "-gui-player.txt");
    }

    // Once per frame. With bots: publishes this role's current player ID
    // (again after a rejoin) and reads the peer's until it is in the room.
    // Returns the peer to observe; a two-player room never needs one.
    std::optional<fps::pvp::PlayerId> Observe(const fps::pvp::ClientConnectionState& state) {
        if (!WithBots()) return std::nullopt;
        if (!parked_) parked_ = std::filesystem::exists(output_ / BotsParkedFile);
        if (state.phase == fps::pvp::ConnectionPhase::Playing && state.playerId && state.playerId != published_) {
            const auto temporary = PublishedPath(role_).string() + ".tmp";
            {
                std::ofstream file(temporary);
                file << state.playerId << '\n';
                if (!file) throw std::runtime_error("Cannot publish the GUI player ID");
            }
            std::filesystem::rename(temporary, PublishedPath(role_));
            published_ = state.playerId;
        }
        if (!peer_ || !InSnapshot(state, *peer_)) {
            std::ifstream file(PublishedPath(role_ == "create" ? "join" : "create"));
            fps::pvp::PlayerId id{};
            if (file >> id && id) peer_ = id;
        }
        return peer_;
    }

    // The other GUI role's player in this snapshot: the only remote of a
    // two-player room, else the published peer while it is present.
    [[nodiscard]] std::optional<fps::pvp::PlayerId> Peer(const fps::pvp::ClientConnectionState& state) const {
        if (!state.snapshot) return std::nullopt;
        if (!WithBots()) {
            for (const auto& player : state.snapshot->players)
                if (player.playerId != state.playerId) return player.playerId;
            return std::nullopt;
        }
        if (peer_ && *peer_ != state.playerId && InSnapshot(state, *peer_)) return peer_;
        return std::nullopt;
    }

    // Both GUI roles and every (parked) bot are in the room.
    [[nodiscard]] bool Full(const fps::pvp::ClientConnectionState& state) const {
        return state.snapshot && state.snapshot->players.size() == players_ && Peer(state).has_value() &&
            (!WithBots() || parked_);
    }

private:
    static bool InSnapshot(const fps::pvp::ClientConnectionState& state, fps::pvp::PlayerId id) {
        if (!state.snapshot) return false;
        for (const auto& player : state.snapshot->players)
            if (player.playerId == id) return true;
        return false;
    }

    std::filesystem::path output_;
    std::string role_;
    unsigned players_{};
    fps::pvp::PlayerId published_{};
    bool parked_{};
    std::optional<fps::pvp::PlayerId> peer_;
};

// The two GUI participants' entries, in snapshot order; none unless both are
// present. With exactly two entries this is the snapshot's own two entries.
template <class Entry>
std::optional<std::array<Entry, 2>> GuiParticipants(const std::vector<Entry>& entries, fps::pvp::PlayerId a,
                                                    fps::pvp::PlayerId b) {
    std::array<Entry, 2> pair{};
    std::size_t found = 0;
    for (const auto& entry : entries) {
        if (entry.playerId != a && entry.playerId != b) continue;
        if (found == 2) return std::nullopt;
        pair[found++] = entry;
    }
    if (found != 2 || a == b) return std::nullopt;
    return pair;
}
