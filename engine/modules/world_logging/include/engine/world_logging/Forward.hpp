#pragma once

#include <atomic>
#include <concepts>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

#include "engine/base/Assert.hpp"
#include "engine/base/Result.hpp"
#include "engine/kernel/Params.hpp"
#include "engine/kernel/Registry.hpp"
#include "engine/kernel/World.hpp"
#include "engine/logging/Level.hpp"
#include "engine/logging/RecordBuilder.hpp"
#include "engine/logging/Write.hpp"
#include "engine/orchestration/OrchestrationError.hpp"
#include "engine/orchestration/Setup.hpp"
#include "engine/world_logging/LogWorldLabel.hpp"

// Entrance ① of section 3 of docs/architecture/target/v2/logging-declaration.zh-Hant.md: a retained
// Message of a healthy World becomes records. The Module that sends the message describes it once, in a
// descriptor next to the message (question 1, approved: D); whoever wires the World decides whether it is
// logged, with one Forward line in its build:
//
//     struct TimeDiscardedLog {                              // next to TimeDiscarded, in engine/world_time
//         using Message = TimeDiscarded;
//         static constexpr std::string_view name = "TimeDiscarded";
//         static constexpr std::string_view category = "world_time";
//         static constexpr Logging::Level level = Logging::Level::Debug;
//         static void Describe(const TimeDiscarded& message, Logging::RecordBuilder& out);
//     };
//     WorldLogging::Forward<WorldTime::TimeDiscardedLog>(setup, {.schedule = "Main", .stage = "Last"});
//
// Forward registers one System, "Log" + D::name, whose only parameters are MessageReader<D::Message> and
// Res<LogWorldLabel<D>> (both read only, checked at compile time; Design Invariant LG-3). It always
// succeeds: a descriptor or sink that throws becomes one Error record of category "logging", because an
// exception leaving a System would interrupt the World. With no sink taking D's level and category it only
// moves its cursor (records are read, not formatted; reading still allocates, so this path does not promise
// to allocate nothing, LG-7). Records carry the World's name, never a tick (the logging Module knows no
// time, LG-4: a descriptor that wants a tick writes it from the message).
//
// Not guaranteed (LG-1): messages written before the System first ran in an outermost Run of another
// schedule are trimmed at the end of that Run and never read (event cursor declaration, "a reader that has
// never run"). In a World with the rollback strategy a message written in the fixed step can have no reader
// outside it (rollback declaration 7.1); how such messages are logged is left to batch 4 (question 7).

namespace GYO::Engine::WorldLogging {

// What a descriptor provides. Describe writes the record's text and fields; it may throw (the System
// catches it).
template <class D>
concept LogDescriptor = requires(const typename D::Message& message, Logging::RecordBuilder& out) {
    typename D::Message;
    { D::name } -> std::convertible_to<std::string_view>;
    { D::category } -> std::convertible_to<std::string_view>;
    { D::level } -> std::convertible_to<Logging::Level>;
    D::Describe(message, out);
};

// Where the System runs: no default (a World with ScheduleSkeleton::Off has no "Main"). With the default
// skeleton, "Main" and "Last" log each frame's messages at the end of the frame.
struct ForwardOptions final {
    std::string schedule;
    std::string stage;
};

// Descriptors or sinks that threw, process-wide, since the program started.
[[nodiscard]] std::uint64_t FailedDescriptions() noexcept;

namespace Detail {

void CountFailedDescription() noexcept;
void WriteDescriptionFailure(std::string_view descriptor, std::string_view world) noexcept;

// Design Invariant LG-3: every parameter of the System Forward registers only reads.
template <class... P>
consteval bool RequireReadOnlyParameters() {
    static_assert((Kernel::ParamTraits<P>::readOnly && ...),
                  "GYO_WORLD_LOGGING_REJECT_WRITE: a System that Forward registers only reads (Design Invariant LG-3)");
    return true;
}

template <class D>
consteval bool RequireDescriptor() {
    static_assert(LogDescriptor<D>,
                  "GYO_WORLD_LOGGING_REJECT_DESCRIPTOR: a log descriptor has Message, name, category, level and "
                  "static Describe(const Message&, Logging::RecordBuilder&)");
    return true;
}

template <class D>
Kernel::SystemResult ForwardMessages(Kernel::MessageReader<typename D::Message> reader, Kernel::Res<LogWorldLabel<D>> label) {
    static_assert(RequireReadOnlyParameters<Kernel::MessageReader<typename D::Message>, Kernel::Res<LogWorldLabel<D>>>());
    std::string_view world;
    try {
        if (label.Present()) world = label->Name();
        if (!Logging::Enabled(D::level, D::category)) {
            // Only the cursor moves: what was not logged now is not logged later.
            static_cast<void>(reader.Read());
            return {};
        }
        const auto messages = reader.Read();
        Logging::RecordBuilder builder;
        for (const typename D::Message* message : messages) {
            try {
                builder.Clear();
                D::Describe(*message, builder);
                Logging::Write(Logging::Entry{.level = D::level,
                                              .category = D::category,
                                              .text = builder.TextView(),
                                              .fields = builder.Fields(),
                                              .world = world});
            } catch (...) {
                CountFailedDescription();
                WriteDescriptionFailure(D::name, world);
            }
        }
    } catch (...) {
        // Reading itself failed (no memory): the World goes on.
        CountFailedDescription();
        WriteDescriptionFailure(D::name, world);
    }
    return {};
}

} // namespace Detail

template <class D>
void Forward(Orchestration::WorldSetup& setup, ForwardOptions mount) {
    static_assert(Detail::RequireDescriptor<D>());
    if constexpr (LogDescriptor<D>) {
        GYO_ASSERT(!mount.schedule.empty() && !mount.stage.empty());
        // The World's name is the author's constant; a longer one does not fit the label (Programmer Error).
        GYO_ASSERT(setup.WorldName().size() <= kMaxWorldNameBytes);
        Kernel::Registrar& registrar = setup.Registrar();
        const std::string name(D::name);
        registrar.Resource<LogWorldLabel<D>>({.name = "LogWorldLabel" + name});
        Kernel::SystemOptions options;
        options.name = "Log" + name;
        Kernel::Mount at;
        at.schedule = std::move(mount.schedule);
        at.stage = std::move(mount.stage);
        options.mounts.push_back(std::move(at));
        registrar.AddSystem<&Detail::ForwardMessages<D>>(std::move(options));
        setup.Prepare([](Kernel::World& world) -> Base::Result<void, Orchestration::OrchestrationError> {
            if (world.Resource<LogWorldLabel<D>>() == nullptr) {
                LogWorldLabel<D> label;
                const std::string& worldName = world.Name();
                std::memcpy(label.name, worldName.data(), worldName.size() < sizeof label.name ? worldName.size() : sizeof label.name - 1);
                world.SetResource(label);
            }
            return {};
        });
    }
}

} // namespace GYO::Engine::WorldLogging
