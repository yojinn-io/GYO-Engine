#include "engine/world_logging/TakeKernelDiagnostics.hpp"

#include <string>
#include <vector>

#include "engine/logging/Write.hpp"

namespace GYO::Engine::WorldLogging {

namespace {

std::string_view ReasonName(const Kernel::SpawnRejectReason reason) noexcept {
    switch (reason) {
    case Kernel::SpawnRejectReason::IdInAllotterRange: return "IdInAllotterRange";
    case Kernel::SpawnRejectReason::IdSpawnedTwice: return "IdSpawnedTwice";
    case Kernel::SpawnRejectReason::IdAlive: return "IdAlive";
    }
    return "Unknown";
}

// "System@schedule" (an observer: "System@observer"; a run condition: "System@schedule?condition"), comma separated.
std::string ReaderNames(const std::vector<Kernel::MessageReaderInfo>& readers) {
    std::string names;
    for (const Kernel::MessageReaderInfo& reader : readers) {
        if (!names.empty()) names += ',';
        names += reader.system;
        names += '@';
        names += reader.observer ? std::string_view("observer") : std::string_view(reader.schedule);
        if (!reader.condition.empty()) {
            names += '?';
            names += reader.condition;
        }
    }
    return names;
}

} // namespace

std::size_t TakeKernelDiagnostics(Kernel::World& world) {
    const bool interrupted = world.State() == Kernel::WorldState::Interrupted;
    const std::string_view worldName = world.Name();
    std::size_t taken = 0;
    for (const Kernel::DroppedMessages& dropped : world.TakeDiagnostics<Kernel::DroppedMessages>()) {
        ++taken;
        const std::string text = "dropped " + std::to_string(dropped.count) + " records of message " + dropped.message +
                                 " (the buffer was full)";
        const std::string readers = ReaderNames(dropped.readers);
        const Logging::Field fields[] = {{"message", std::string_view(dropped.message)},
                                         {"count", dropped.count},
                                         {"first_seq", dropped.firstSeq},
                                         {"last_seq", dropped.lastSeq},
                                         {"readers", std::string_view(readers)}};
        Logging::Write(Logging::Entry{.level = Logging::Level::Warning,
                                      .category = "kernel",
                                      .text = text,
                                      .fields = fields,
                                      .world = worldName,
                                      .worldInterrupted = interrupted});
    }
    for (const Kernel::RejectedSpawn& rejected : world.TakeDiagnostics<Kernel::RejectedSpawn>()) {
        ++taken;
        const std::string_view reason = ReasonName(rejected.reason);
        const std::string text = "a spawn of entity " + std::to_string(rejected.entity) + " was rejected (" +
                                 std::string(reason) + ")";
        const Logging::Field fields[] = {{"entity", static_cast<std::uint64_t>(rejected.entity)},
                                         {"reason", reason},
                                         {"emitter", std::string_view(rejected.emitter)},
                                         {"emitter_is_hook", rejected.emitterIsHook},
                                         {"from_allotter", rejected.fromAllotter},
                                         {"schedule", std::string_view(rejected.schedule)},
                                         {"step", rejected.step},
                                         {"applied_at", rejected.appliedAt},
                                         {"sync_epoch", rejected.syncEpoch},
                                         {"other_emitter", std::string_view(rejected.otherEmitter)}};
        Logging::Write(Logging::Entry{.level = Logging::Level::Warning,
                                      .category = "kernel",
                                      .text = text,
                                      .fields = fields,
                                      .world = worldName,
                                      .worldInterrupted = interrupted});
    }
    return taken;
}

void ForwardKernelDiagnostics(Orchestration::WorldSetup& setup) {
    setup.ExchangeOut([](Kernel::World& world) -> Base::Result<void, Orchestration::OrchestrationError> {
        static_cast<void>(TakeKernelDiagnostics(world));
        return {};
    });
}

} // namespace GYO::Engine::WorldLogging
