#pragma once

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include <doctest/doctest.h>

#include "engine/kernel/Kernel.hpp"
#include "engine/logging/Logger.hpp"
#include "engine/logging/MemorySink.hpp"
#include "engine/orchestration/Orchestration.hpp"

// Shared by the World logging tests: a Logger with one MemorySink, and Product helpers.

namespace GYO::Engine::WorldLogging::Test {

namespace K = GYO::Kernel;
namespace O = GYO::Engine::Orchestration;
namespace L = GYO::Engine::Logging;

struct Logged {
    std::shared_ptr<L::MemorySink> memory = std::make_shared<L::MemorySink>();
    std::optional<L::LoggerHandle> handle;

    explicit Logged(L::Level minimum = L::Level::Trace) {
        L::LoggerOptions options;
        options.sinks.push_back({.name = "memory", .sink = memory, .filter = {.minimum = minimum}});
        auto installed = L::Install(std::move(options));
        REQUIRE(installed);
        handle.emplace(std::move(installed.value()));
    }

    [[nodiscard]] std::vector<L::StoredRecord> Category(const std::string& category) const {
        std::vector<L::StoredRecord> out;
        for (auto& record : memory->Records()) {
            if (record.category == category) out.push_back(std::move(record));
        }
        return out;
    }
};

inline K::SystemOptions At(std::string name, std::string schedule, std::string stage) {
    K::SystemOptions o;
    o.name = std::move(name);
    K::Mount m;
    m.schedule = std::move(schedule);
    m.stage = std::move(stage);
    o.mounts.push_back(std::move(m));
    return o;
}

inline std::unique_ptr<O::Product> Make(O::ProductDescription d, O::RegisteredUnits units = {}) {
    auto built = O::Product::Build(std::move(d), std::move(units));
    REQUIRE_MESSAGE(static_cast<bool>(built), (built ? std::string() : built.error().message + "\n" + built.error().detail));
    return std::move(*built);
}

inline void Frame(O::Product& p) {
    auto ran = p.RunFrame();
    REQUIRE_MESSAGE(static_cast<bool>(ran), (ran ? std::string() : ran.error().message + "\n" + ran.error().detail));
}

} // namespace GYO::Engine::WorldLogging::Test
