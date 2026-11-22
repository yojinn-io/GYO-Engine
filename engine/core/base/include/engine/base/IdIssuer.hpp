#pragma once

#include "engine/base/Assert.hpp"

#include <atomic>
#include <cstdint>

namespace GYO::Engine::Base {

// Issues numbers from one source. Every Next() of one IdIssuer returns a
// number no earlier Next() of it returned while the program runs: numbers are
// never given back or reused, carry no meaning beyond "different", start at 1
// and are never 0 (0 is free for "none"). Any thread may call Next(). Domain
// free, like Sha256: any system may keep its own issuer.
class IdIssuer {
public:
    constexpr IdIssuer() noexcept = default;
    IdIssuer(const IdIssuer&) = delete;
    IdIssuer& operator=(const IdIssuer&) = delete;

    [[nodiscard]] std::uint64_t Next() noexcept {
        const std::uint64_t id = next_.fetch_add(1, std::memory_order_relaxed);
        // 2^64 numbers issued: the next one would repeat. No test reaches it (it
        // would take 2^64 calls); the mutant list says so.
        GYO_ASSERT(id != 0);
        return id;
    }

private:
    std::atomic<std::uint64_t> next_{1};
};

namespace Detail {
inline constinit IdIssuer threadSerials;
inline constinit thread_local std::uint64_t threadSerial = 0;
} // namespace Detail

// The calling thread's serial: taken from one IdIssuer kept for this the first
// time the thread asks, the same number afterwards. Never 0, and no two
// threads of one program run share one, also when one has ended before the
// other started (unlike a thread_local address or a reused thread id).
[[nodiscard]] inline std::uint64_t CurrentThreadSerial() noexcept {
    if (Detail::threadSerial == 0) Detail::threadSerial = Detail::threadSerials.Next();
    return Detail::threadSerial;
}

} // namespace GYO::Engine::Base
