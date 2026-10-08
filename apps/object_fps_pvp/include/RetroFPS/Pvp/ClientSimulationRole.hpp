#pragma once

#include "RetroFPS/Pvp/ClientConnection.hpp"
#include "RetroFPS/Pvp/ClientSimulation.hpp"
#include "engine/threads/RoleThread.hpp"
#include "engine/time/LateWakeStats.hpp"
#include "engine/time/MonotonicClock.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace fps::pvp {

// The main thread's latest movement intent. Aim is absolute: the main thread
// owns the view and integrates pointer motion, so no delta can be lost or
// applied twice between simulation steps.
struct ClientIntent final {
    float forward{};
    float right{};
    float yaw{};
    float pitch{};
    // False without window focus, pointer capture or a living player: movement
    // is neutral and jumps are dropped; aim is kept.
    bool controls{};
    // Jump presses counted by the publisher while it had controls; each one the
    // simulation has not seen requests one jump.
    std::uint64_t jumpPresses{};
    // When the publisher sampled it; the default value means never published.
    Engine::Time::TimePoint sampledAt{};
};

// Decision D30: the simulation keeps using the latest intent until it is older
// than max(this minimum, ClientIntentStaleIntervals x the longest recent publish
// interval); then movement is neutral, aim stays and jumps are dropped. The
// product supports frame rates down to ClientMinimumSupportedFps.
inline constexpr double ClientIntentMinimumStaleSeconds = 0.1;
inline constexpr double ClientIntentStaleIntervals = 3;
inline constexpr std::size_t ClientIntentRecentIntervals = 4;
inline constexpr double ClientMinimumSupportedFps = 20;

// What a simulation step publishes for the main thread: the observation, the
// state that places the local player at a later render time, and the shot timing.
struct ClientPresentation final {
    LocalMovementObservation observation;
    LocalPresentationState presentation;
    std::optional<FireGateTiming> shotTiming;
    Engine::Time::TimePoint steppedAt{};
    std::size_t arenaIndex{};
    // The sample time of the intent the step used.
    Engine::Time::TimePoint intentSampledAt{};
};

// A presentation placed at a render time, with the shot timing advanced to it.
struct ClientPresented final {
    LocalMovementObservation observation;
    std::optional<FireGateTiming> shotTiming;
    Engine::Time::TimePoint intentSampledAt{};
};

[[nodiscard]] ClientPresented PresentAt(const std::vector<Arena>& arenas, const ClientPresentation& presentation,
                                        Engine::Time::TimePoint now);

// One simulation step, independent of threads and of the real clock, so tests
// drive it with injected times. Run takes the snapshots drained since the last
// step: a connection generation, player or arena change starts the prediction
// over; every snapshot gives its phase sample and the newest reconciles; then
// the intent decides this step's input and the prediction advances.
class ClientSimulationLoop final {
public:
    struct Step final {
        // The complete window to send when it changed.
        std::optional<PlayerInput> window;
        // The connection generation the window's authority came from.
        std::uint64_t generation{};
        // The next fixed-step boundary, or one step later while inactive.
        Engine::Time::TimePoint nextDeadline{};
        ClientPresentation presentation;
        bool intentStale{};
    };

    // Throws std::invalid_argument without an installed arena.
    explicit ClientSimulationLoop(std::vector<Arena> installed);

    [[nodiscard]] Step Run(Engine::Time::TimePoint now, const ClientIntent& intent, ClientSimulationDrain drain);

    [[nodiscard]] const std::vector<Arena>& Arenas() const noexcept { return arenas_; }
    [[nodiscard]] const ClientSimulation& Simulation() const noexcept { return simulation_; }

private:
    void TrackIntent(const ClientIntent& intent);
    [[nodiscard]] bool IntentStale(Engine::Time::TimePoint now) const noexcept;

    std::vector<Arena> arenas_;
    std::size_t arenaIndex_{};
    ClientSimulation simulation_;
    std::optional<std::uint64_t> generation_;
    PlayerId playerId_{};
    std::uint64_t consumedJumps_{};
    Engine::Time::TimePoint lastIntentAt_{};
    std::array<double, ClientIntentRecentIntervals> intervals_{};
    std::size_t nextInterval_{};
};

// The client simulation role: a named thread that runs ClientSimulationLoop at
// fixed-step deadlines on the engine time base, sends each changed window to
// the connection and publishes the presentation. The main thread publishes
// intents and reads presentations; neither waits for the other. Session, player
// and arena changes reach the role through its own drain of the connection, in
// the same order as the snapshots. Destroy it before the connection.
class ClientSimulationRole final {
public:
    // Throws std::invalid_argument without an installed arena and
    // std::runtime_error when the thread cannot start.
    ClientSimulationRole(ClientConnection& connection, std::vector<Arena> installed,
                         std::string name = "gyo-client-sim");
    ~ClientSimulationRole();
    ClientSimulationRole(const ClientSimulationRole&) = delete;
    ClientSimulationRole& operator=(const ClientSimulationRole&) = delete;

    // The simulation runs on its own deadlines; an intent does not wake it.
    void PublishIntent(const ClientIntent& intent);

    [[nodiscard]] ClientPresentation Latest() const;
    [[nodiscard]] ClientPresented PresentAt(Engine::Time::TimePoint now) const;
    // Set when the loop stopped on a Runtime Error; the role does no more work.
    [[nodiscard]] std::optional<std::string> Error() const;
    // Loop runs, and late wakes of the deadline waits, since the last call.
    struct WakeReport final {
        std::uint64_t runs{};
        Engine::Time::LateWakeStats late;
    };
    [[nodiscard]] WakeReport TakeWakeReport();

private:
    void Body(Engine::Threads::RoleContext& context);

    ClientConnection& connection_;
    // Only the role thread touches the loop after construction; its installed
    // arenas never change, so PresentAt reads them from the main thread.
    ClientSimulationLoop loop_;
    mutable std::mutex mutex_;
    ClientIntent intent_;
    ClientPresentation presentation_;
    std::optional<std::string> error_;
    WakeReport wakes_;
    // Last member: destroyed first, so the thread stops before the state it uses.
    std::optional<Engine::Threads::RoleThread> thread_;
};

} // namespace fps::pvp
