#include "Shutdown.hpp"

#include <atomic>
#include <cstdint>

namespace Engine::Os {

namespace {

// 0: no request yet; otherwise the ShutdownReason of the first request.
constinit std::atomic<std::uint8_t> requested{0};
static_assert(std::atomic<std::uint8_t>::is_always_lock_free, "a signal handler writes the shutdown request");

} // namespace

void Detail::RecordShutdown(const Console::ShutdownReason reason) noexcept {
    std::uint8_t none = 0;
    requested.compare_exchange_strong(none, static_cast<std::uint8_t>(reason), std::memory_order_acq_rel,
                                      std::memory_order_acquire);
}

std::optional<Console::ShutdownReason> Console::RequestedShutdown() noexcept {
    const std::uint8_t value = requested.load(std::memory_order_acquire);
    if (value == 0) return std::nullopt;
    return static_cast<ShutdownReason>(value);
}

} // namespace Engine::Os
