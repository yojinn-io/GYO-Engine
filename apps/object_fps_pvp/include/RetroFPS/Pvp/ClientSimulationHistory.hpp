#pragma once

// The simulation role's own receipt-stamped snapshot history inside the
// network worker (ClientConnection::DrainSimulation), with the one-shot resume
// flag of network-architecture §4, recovery step 3. The flag can only be raised
// together with the snapshot that ended the interruption, so no drained batch
// holds that snapshot without it. Not synchronized: the connection's mutex
// guards every call. Header-only and clock-free for its tests.
#include "RetroFPS/Pvp/ClientConnection.hpp"

#include <cstdint>
#include <deque>
#include <utility>
#include <vector>

namespace fps::pvp {

class ClientSimulationHistory final {
public:
    struct Taken final {
        std::vector<ReceivedSnapshot> snapshots;
        // Dropped because the history was full, cumulative since the last Clear.
        std::uint64_t overflowCount{};
        bool resumed{};
    };

    // A valid snapshot; resumes: it ended an interruption. Kept only after the
    // first Take, so a connection without a simulation role copies nothing.
    // Returns whether the oldest snapshot was dropped to make room.
    bool Push(ReceivedSnapshot snapshot, bool resumes) {
        if (!consumer_) return false;
        bool dropped = false;
        if (snapshots_.size() == MaxReceivedSnapshots) {
            snapshots_.pop_front();
            ++overflowCount_;
            dropped = true;
        }
        snapshots_.push_back(std::move(snapshot));
        resumed_ = resumed_ || resumes;
        return dropped;
    }
    // Everything since the previous Take; the flag is handed over once.
    [[nodiscard]] Taken Take() {
        consumer_ = true;
        Taken taken{{}, overflowCount_, std::exchange(resumed_, false)};
        taken.snapshots.reserve(snapshots_.size());
        for (auto& received : snapshots_) taken.snapshots.push_back(std::move(received));
        snapshots_.clear();
        return taken;
    }
    // A connection generation boundary (Close, Failure, a new request). The
    // consumer stays. Clearing the flag here is defensive: after a generation
    // change the simulation starts over and reseeds, so a stale flag would
    // restart nothing (ClientSimulation::Observe asks Reseeds first).
    void Clear() noexcept {
        snapshots_.clear();
        overflowCount_ = 0;
        resumed_ = false;
    }

private:
    std::deque<ReceivedSnapshot> snapshots_;
    std::uint64_t overflowCount_{};
    bool resumed_{};
    bool consumer_{};
};

} // namespace fps::pvp
