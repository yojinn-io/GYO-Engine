#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "engine/logging/JsonLinesFileSink.hpp"

// Injected outputs for the file sink (declaration 5.7: the writer's output is
// injectable; judgement 2 uses one that never returns).

namespace GYO::Engine::Logging::Test {

// Keeps every byte; can be held (Write waits until Release) and can fail.
class RecordingOutput final : public LineOutput {
public:
    bool Write(const std::string_view bytes) noexcept override {
        std::unique_lock lock(mutex_);
        ++writesStarted_;
        changed_.notify_all();
        changed_.wait(lock, [&] { return !held_; });
        if (fail_) return false;
        bytes_.append(bytes);
        ++writes_;
        return true;
    }
    bool Close() noexcept override {
        std::scoped_lock lock(mutex_);
        closed_ = true;
        return true;
    }

    void Hold() {
        std::scoped_lock lock(mutex_);
        held_ = true;
    }
    void Release() {
        {
            std::scoped_lock lock(mutex_);
            held_ = false;
        }
        changed_.notify_all();
    }
    void Fail() {
        std::scoped_lock lock(mutex_);
        fail_ = true;
    }
    // Waits until a Write has started (and is perhaps held).
    bool WaitForWriteStarted(const std::uint64_t count, const std::chrono::milliseconds timeout) {
        std::unique_lock lock(mutex_);
        return changed_.wait_for(lock, timeout, [&] { return writesStarted_ >= count; });
    }

    [[nodiscard]] std::string Bytes() const {
        std::scoped_lock lock(mutex_);
        return bytes_;
    }
    [[nodiscard]] std::vector<std::string> Lines() const {
        std::vector<std::string> lines;
        const std::string all = Bytes();
        std::size_t start = 0;
        while (start < all.size()) {
            const std::size_t end = all.find('\n', start);
            if (end == std::string::npos) {
                lines.push_back(all.substr(start));
                break;
            }
            lines.push_back(all.substr(start, end - start));
            start = end + 1;
        }
        return lines;
    }
    [[nodiscard]] std::uint64_t Writes() const {
        std::scoped_lock lock(mutex_);
        return writes_;
    }
    [[nodiscard]] bool Closed() const {
        std::scoped_lock lock(mutex_);
        return closed_;
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable changed_;
    std::string bytes_;
    std::uint64_t writes_ = 0;
    std::uint64_t writesStarted_ = 0;
    bool held_ = false;
    bool fail_ = false;
    bool closed_ = false;
};

} // namespace GYO::Engine::Logging::Test
