#include "RetroFPS/Pvp/ClientLog.hpp"

#include "engine/time/Waiter.hpp"

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <optional>
#include <string>
#include <thread>

namespace fps::pvp {

void ClientLog(const Engine::Base::LogLevel level, const char* format, ...) noexcept {
    try {
        va_list arguments;
        va_start(arguments, format);
        va_list measure;
        va_copy(measure, arguments);
        const int size = std::vsnprintf(nullptr, 0, format, measure);
        va_end(measure);
        std::string text;
        if (size > 0) {
            text.resize(static_cast<std::size_t>(size) + 1);
            std::vsnprintf(text.data(), text.size(), format, arguments);
            text.resize(static_cast<std::size_t>(size));
        }
        va_end(arguments);
        Engine::Base::Log(level, "app", text);
    } catch (...) {
        // A log line is never worth a failure.
    }
}

void PauseMilliseconds(const int milliseconds) noexcept {
    // One Waiter per thread; without one (creation failed) a plain sleep.
    thread_local std::optional<Engine::Time::Waiter> waiter = [] () -> std::optional<Engine::Time::Waiter> {
        auto created = Engine::Time::Waiter::Create();
        if (!created) return std::nullopt;
        return std::move(created.value());
    }();
    const auto deadline = Engine::Time::Now() + std::chrono::milliseconds(milliseconds);
    if (waiter) {
        static_cast<void>(waiter->WaitUntil(deadline));
    } else {
        std::this_thread::sleep_until(deadline);
    }
}

} // namespace fps::pvp
