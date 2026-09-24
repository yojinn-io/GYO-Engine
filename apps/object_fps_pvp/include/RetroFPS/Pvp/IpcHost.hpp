#pragma once
#include "RetroFPS/Pvp/MatchRuntimeHost.hpp"
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
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
