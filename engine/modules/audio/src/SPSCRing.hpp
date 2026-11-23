#pragma once

#include <atomic>
#include <cstdint>
#include <type_traits>
#include <vector>

namespace GYO::Engine::Audio::Detail {

// One bounded single-producer, single-consumer ring of trivially copyable
// values, allocated once. The producer owns tail, the consumer owns head; each
// publishes its index with release and reads the other's with acquire, so a
// slot is written before the consumer sees it and read before the producer
// reuses it. Indices only grow; the slot is the index modulo the capacity.
// Neither side allocates, locks or waits.
template <class T>
class SPSCRing final {
    static_assert(std::is_trivially_copyable_v<T>);

public:
    explicit SPSCRing(const std::uint32_t capacity) : slots_(capacity) {}

    // The producer. False, with nothing written, when the ring is full.
    bool TryPush(const T& value) noexcept {
        const std::uint64_t tail = tail_.load(std::memory_order_relaxed);
        const std::uint64_t head = head_.load(std::memory_order_acquire);
        const std::uint64_t capacity = slots_.size();
        if (tail - head >= capacity) return false;
        slots_[tail % capacity] = value;
        tail_.store(tail + 1, std::memory_order_release);
        return true;
    }

    // The consumer. Calls take(value) for every value pushed before, oldest
    // first, then frees their slots at once.
    template <class Take>
    void Drain(Take&& take) noexcept {
        std::uint64_t head = head_.load(std::memory_order_relaxed);
        const std::uint64_t tail = tail_.load(std::memory_order_acquire);
        if (head == tail) return;
        const std::uint64_t capacity = slots_.size();
        for (; head != tail; ++head) take(static_cast<const T&>(slots_[head % capacity]));
        head_.store(head, std::memory_order_release);
    }

private:
    std::vector<T> slots_;
    alignas(64) std::atomic<std::uint64_t> tail_{0};
    alignas(64) std::atomic<std::uint64_t> head_{0};
};

} // namespace GYO::Engine::Audio::Detail
