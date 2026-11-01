#pragma once
#include "RetroFPS/Pvp/MatchRuntimeHost.hpp"
#include <cstdint>
#include <memory>
#include <string>

namespace fps::pvp {
// Wire adapter only. MatchRuntimeHost owns the independent simulation thread.
class IpcHost final {
public:
    // animationDigest: AnimationContentDigest of the Match's player content,
    // sent in Ready (protocol-v9 §4.6); zero is invalid and throws.
    IpcHost(MatchRuntimeHost& runtime, const Arena& arena, std::uint64_t animationDigest);
    ~IpcHost();
    bool Start(const std::string& listenAddress, std::string& error);
    void Stop();
    // Diagnostics only: passes of the connection and accept loops since Start.
    [[nodiscard]] std::uint64_t Iterations() const noexcept;
    // Diagnostics only (log-contract, E1-7): the lanes' pace since the previous call.
    [[nodiscard]] IpcLaneStatistics TakeLaneStatistics();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
// The Match log line of a failed connection-quality window (log-contract,
// A grade), without the "[ObjectFPS/PvP Match] " prefix.
[[nodiscard]] std::string QualityWindowFailedLine(const QualityWindowFailure& failure);
}
