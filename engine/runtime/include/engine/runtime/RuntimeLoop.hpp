#pragma once

#include "engine/runtime/IRuntimeClient.hpp"

namespace Engine::Runtime {

class RuntimeLoop final {
public:
    explicit RuntimeLoop(IRuntimeClient& client) noexcept;

    void Run();

private:
    IRuntimeClient* client_;
};

} // namespace Engine::Runtime
