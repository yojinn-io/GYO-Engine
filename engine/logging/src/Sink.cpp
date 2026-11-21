#include "engine/logging/Sink.hpp"

namespace GYO::Engine::Logging {

bool Sink::WriteFatal(const Record&, std::string_view, std::string_view) noexcept {
    return true;
}

bool Sink::Flush(std::chrono::steady_clock::time_point) noexcept {
    return true;
}

void Sink::Close() noexcept {}

} // namespace GYO::Engine::Logging
