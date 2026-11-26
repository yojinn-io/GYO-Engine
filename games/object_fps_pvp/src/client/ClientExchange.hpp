#pragma once

// What the Sim World (or the Solo World) and the Frame World hand each other: one object of the Product
// (ConfigureClient makes it), shared by every scene.

#include "object_fps_pvp/client/ClientWorld.hpp"
#include "object_fps_pvp/present/Controls.hpp"
#include "object_fps_pvp/world/Components.hpp"

#include "engine/kernel/Kernel.hpp"
#include "engine/physics_3d/Shapes.hpp"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace object_fps_pvp::client {

namespace K = GYO::Kernel;
namespace PW = object_fps_pvp::world;

// What the Sim World and the Frame World hand each other (live-frame-sim-thread declaration 4.3, O-13 (3)): one object
// of the Product, each side behind its own lock, neither side waiting for the other. The Sim World's "out" puts the
// newest picture of the game (only the newest is kept: a frame wants the latest); the Frame World's "out" adds the
// controls (the held keys and the view are the newest; the presses add up, so none is lost however the two clocks
// run), and the Sim World's "in" takes them.
struct PlayerSnapshot final {
    K::EntityId id = 0;
    PW::Owner owner;
    std::optional<PW::PlayerBody> body;
    std::optional<PW::PlayerCombat> combat;
    std::optional<PW::HitRecord> hits;
    std::optional<PW::ShotRecord> shot;
};
struct WorldSnapshot final {
    std::uint64_t version = 0;
    std::vector<PlayerSnapshot> players;  // every entity with an Owner
    std::string arenaId;                  // the arena the Sim World plays (GameRules)
    std::uint64_t wallsDigest = 0;
    // Those walls once built (a BoxSet's body is shared and immutable: a copy is a reference), for the sounds'
    // occlusion in the Frame World.
    std::optional<GYO::Engine::Physics3d::BoxSet> walls;
    ClientView view;
};
class ClientExchange final {
public:
    // (Made outside the lock; the lock only swaps the pointer.)
    void PutSnapshot(WorldSnapshot snapshot) {
        auto shared = std::make_shared<WorldSnapshot>(std::move(snapshot));
        const std::lock_guard<std::mutex> lock(snapshotLock_);
        shared->version = ++version_;
        snapshot_ = std::move(shared);
    }
    // The newest picture when it is newer than seen (nullptr otherwise).
    std::shared_ptr<const WorldSnapshot> TakeSnapshot(std::uint64_t& seen) {
        const std::lock_guard<std::mutex> lock(snapshotLock_);
        if (!snapshot_ || snapshot_->version == seen) return nullptr;
        seen = snapshot_->version;
        return snapshot_;
    }
    // viewTick: the Match tick the frame showed of the other players (none: not timed yet); the newest is kept.
    void AddControls(const present::StepInput& step, const std::optional<double> viewTick) {
        const std::lock_guard<std::mutex> lock(controlsLock_);
        Merge(controls_, step);
        viewTick_ = viewTick;
        haveControls_ = true;
    }
    struct Sent final {
        present::StepInput step;
        std::optional<double> viewTick;
    };
    // The controls since the last take (nullopt: none came).
    std::optional<Sent> TakeControls() {
        const std::lock_guard<std::mutex> lock(controlsLock_);
        if (!haveControls_) return std::nullopt;
        Sent taken{controls_, viewTick_};
        controls_.jump = controls_.fire = controls_.reload = false;
        haveControls_ = false;
        return taken;
    }
    // The newest held keys and view; the presses of either add up.
    static void Merge(present::StepInput& into, const present::StepInput& newer) {
        const bool jump = into.jump || newer.jump;
        const bool fire = into.fire || newer.fire;
        const bool reload = into.reload || newer.reload;
        into = newer;
        into.jump = jump;
        into.fire = fire;
        into.reload = reload;
    }

    // Whether the game is paused (single player's pause: the Frame World's out sets it, the Solo World's in reads it).
    void SetPaused(const bool paused) noexcept { paused_.store(paused, std::memory_order_relaxed); }
    [[nodiscard]] bool Paused() const noexcept { return paused_.load(std::memory_order_relaxed); }

    // A scene change (both Worlds are being replaced): neither the picture nor the controls of the World that goes
    // reach the next one.
    void Reset() {
        {
            const std::lock_guard<std::mutex> lock(snapshotLock_);
            snapshot_.reset();
        }
        const std::lock_guard<std::mutex> lock(controlsLock_);
        controls_ = {};
        viewTick_.reset();
        haveControls_ = false;
        paused_.store(false, std::memory_order_relaxed);
    }

private:
    std::mutex snapshotLock_;
    std::shared_ptr<const WorldSnapshot> snapshot_;
    std::uint64_t version_ = 0;
    std::mutex controlsLock_;
    present::StepInput controls_;
    std::optional<double> viewTick_;
    bool haveControls_ = false;
    std::atomic<bool> paused_{false};
};

} // namespace object_fps_pvp::client
