#pragma once
#include "RetroFPS/Pvp/MatchRuntimeHost.hpp"
#include <cstdint>
#include <memory>
#include <string>

namespace fps::pvp {
// Wire adapter only. MatchRuntimeHost owns the independent simulation thread.
class IpcHost final {
public:
    IpcHost(MatchRuntimeHost& runtime, const Arena& arena);
    ~IpcHost();
    bool Start(const std::string& listenAddress, std::string& error);
    void Stop();
    // Diagnostics only: passes of the connection and accept loops since Start.
    [[nodiscard]] std::uint64_t Iterations() const noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
