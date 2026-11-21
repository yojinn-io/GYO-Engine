#include "engine/world_logging/Forward.hpp"

#include <atomic>
#include <string>

namespace GYO::Engine::WorldLogging {

namespace {

std::atomic<std::uint64_t> failedDescriptions{0};

} // namespace

std::uint64_t FailedDescriptions() noexcept {
    return failedDescriptions.load(std::memory_order_relaxed);
}

namespace Detail {

void CountFailedDescription() noexcept {
    failedDescriptions.fetch_add(1, std::memory_order_relaxed);
}

void WriteDescriptionFailure(const std::string_view descriptor, const std::string_view world) noexcept {
    try {
        std::string text = "a log descriptor failed and its record was dropped: ";
        text.append(descriptor);
        Logging::Write(Logging::Entry{.level = Logging::Level::Error, .category = "logging", .text = text, .world = world});
    } catch (...) {
        // No memory even for the Error record: it is counted only.
    }
}

} // namespace Detail

} // namespace GYO::Engine::WorldLogging
