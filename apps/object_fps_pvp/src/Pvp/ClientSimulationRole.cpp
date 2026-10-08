#include "RetroFPS/Pvp/ClientSimulationRole.hpp"

#include <algorithm>
#include <chrono>
#include <exception>
#include <stdexcept>
#include <utility>

namespace fps::pvp {
namespace {
using Engine::Time::TimePoint;

double Seconds(const Engine::Time::Duration duration) { return std::chrono::duration<double>(duration).count(); }

// Rounded up: a deadline never comes before the boundary it stands for.
TimePoint After(const TimePoint at, const double seconds) {
    return at + std::chrono::ceil<Engine::Time::Duration>(std::chrono::duration<double>(seconds));
}

const PlayerState* FindSelf(const WorldSnapshot& snapshot, const PlayerId self) {
    for (const auto& player : snapshot.players)
        if (player.playerId == self) return &player;
    return nullptr;
}
} // namespace

ClientPresented PresentAt(const std::vector<Arena>& arenas, const ClientPresentation& presentation,
                          const TimePoint now) {
    ClientPresented result{presentation.observation, presentation.shotTiming, presentation.intentSampledAt};
    if (!presentation.observation.active || presentation.arenaIndex >= arenas.size()) return result;
    const double since = std::max(0.0, Seconds(now - presentation.steppedAt));
    const auto sample = InterpolateLocalPresentation(arenas[presentation.arenaIndex], presentation.presentation, since);
    result.observation.renderPosition = sample.renderPosition;
    result.observation.correctionOffset = sample.correctionOffset;
    result.observation.interpolationAlpha = sample.interpolationAlpha;
    // FireGate counts whole steps from the newest command; time past the next
    // boundary that the simulation has not stepped yet counts the same way.
    if (result.shotTiming) result.shotTiming->secondsSinceStep += since;
    return result;
}

ClientSimulationLoop::ClientSimulationLoop(std::vector<Arena> installed)
    : arenas_(std::move(installed)), simulation_(arenas_.empty() ? Arena{} : arenas_.front()) {
    if (arenas_.empty()) throw std::invalid_argument("The client simulation needs an installed arena");
}

void ClientSimulationLoop::TrackIntent(const ClientIntent& intent) {
    if (intent.sampledAt <= lastIntentAt_) return;
    if (lastIntentAt_ != TimePoint{}) {
        intervals_[nextInterval_] = Seconds(intent.sampledAt - lastIntentAt_);
        nextInterval_ = (nextInterval_ + 1) % intervals_.size();
    }
    lastIntentAt_ = intent.sampledAt;
}

bool ClientSimulationLoop::IntentStale(const TimePoint now) const noexcept {
    if (lastIntentAt_ == TimePoint{}) return true;
    const double longest = *std::max_element(intervals_.begin(), intervals_.end());
    const double limit = std::max(ClientIntentMinimumStaleSeconds, ClientIntentStaleIntervals * longest);
    return Seconds(now - lastIntentAt_) > limit;
}

ClientSimulationLoop::Step ClientSimulationLoop::Run(const TimePoint now, const ClientIntent& intent,
                                                     ClientSimulationDrain drain) {
    // A new connection generation is a new session, and a new player id a new
    // pawn: the prediction starts over before any of their snapshots.
    if (!generation_ || *generation_ != drain.generation || playerId_ != drain.playerId) simulation_.Reset();
    generation_ = drain.generation;
    playerId_ = drain.playerId;
    // The joined Match chose the arena: predict in that one.
    if (!drain.arenaId.empty() && drain.arenaId != arenas_[arenaIndex_].id) {
        const auto found = std::find_if(arenas_.begin(), arenas_.end(),
            [&](const Arena& arena) { return arena.id == drain.arenaId; });
        if (found != arenas_.end()) {
            arenaIndex_ = static_cast<std::size_t>(found - arenas_.begin());
            simulation_.SelectArena(*found);
        }
    }
    const WorldSnapshot* newest{};
    for (const auto& received : drain.snapshots) {
        simulation_.ObserveSample(received.snapshot, drain.playerId);
        if (!newest || received.snapshot.tick > newest->tick) newest = &received.snapshot;
    }
    if (newest) {
        // Without the local player the session has nothing to predict.
        if (const auto* self = FindSelf(*newest, drain.playerId))
            simulation_.Observe(*self, newest->tick, drain.movementRules);
        else simulation_.Reset();
    }

    TrackIntent(intent);
    const bool stale = IntentStale(now);
    ClientInputSample input{0, 0, intent.yaw, intent.pitch, false, false};
    if (!stale && intent.controls)
        input = {intent.forward, intent.right, intent.yaw, intent.pitch, intent.jumpPresses > consumedJumps_, true};
    // A press seen while stale or without controls is dropped, not deferred.
    consumedJumps_ = std::max(consumedJumps_, intent.jumpPresses);

    Step step;
    step.intentStale = stale;
    step.generation = drain.generation;
    step.window = simulation_.Frame(now, input);
    const bool active = simulation_.Observation().active;
    step.nextDeadline = After(now, active ? simulation_.SecondsUntilNextStep() : MovementTickSeconds);
    step.presentation = {simulation_.Observation(), simulation_.Presentation(), simulation_.ShotTiming(), now,
        arenaIndex_, intent.sampledAt};
    return step;
}

ClientSimulationRole::ClientSimulationRole(ClientConnection& connection, std::vector<Arena> installed,
                                           std::string name)
    : connection_(connection), loop_(std::move(installed)) {
    auto started = Engine::Threads::RoleThread::Start({std::move(name), Engine::Threads::ThreadPriority::Interactive},
        [this](Engine::Threads::RoleContext& context) { Body(context); });
    if (!started) throw std::runtime_error(Engine::Base::Describe(started.error()));
    thread_.emplace(std::move(*started));
}

ClientSimulationRole::~ClientSimulationRole() = default;

void ClientSimulationRole::PublishIntent(const ClientIntent& intent) {
    const std::lock_guard lock(mutex_);
    intent_ = intent;
}

ClientPresentation ClientSimulationRole::Latest() const {
    const std::lock_guard lock(mutex_);
    return presentation_;
}

ClientPresented ClientSimulationRole::PresentAt(const TimePoint now) const {
    return fps::pvp::PresentAt(loop_.Arenas(), Latest(), now);
}

std::optional<std::string> ClientSimulationRole::Error() const {
    const std::lock_guard lock(mutex_);
    return error_;
}

ClientSimulationRole::WakeReport ClientSimulationRole::TakeWakeReport() {
    const std::lock_guard lock(mutex_);
    return std::exchange(wakes_, {});
}

void ClientSimulationRole::Body(Engine::Threads::RoleContext& context) {
    // A thread entry point handles its own Runtime Errors: the main thread
    // reads Error() and fails visibly.
    try {
        while (!context.StopRequested()) {
            const auto now = Engine::Time::Now();
            ClientIntent intent;
            {
                const std::lock_guard lock(mutex_);
                intent = intent_;
            }
            auto step = loop_.Run(now, intent, connection_.DrainSimulation());
            if (step.window) connection_.SendInput(std::move(*step.window), step.generation);
            {
                const std::lock_guard lock(mutex_);
                presentation_ = std::move(step.presentation);
                ++wakes_.runs;
            }
            const auto wake = context.WaitUntil(step.nextDeadline);
            const std::lock_guard lock(mutex_);
            wakes_.late.Record(wake);
        }
    } catch (const std::exception& error) {
        const std::lock_guard lock(mutex_);
        error_ = error.what();
    }
}

} // namespace fps::pvp
