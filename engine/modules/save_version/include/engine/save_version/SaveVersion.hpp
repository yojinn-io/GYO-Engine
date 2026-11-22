#pragma once

// Save versions (restore-entry declaration docs/architecture/target/v2/restore-entry-declaration.zh-Hant.md,
// section 19; Design Invariants SV-1 to SV-3 in docs/architecture/target/v2/save-version.zh-Hant.md;
// v8-arch-gyo2-save-version-module). A library (config/modules.csv library_only=yes, default=no): a product that
// wants it links GYO::SaveVersion; nothing is registered anywhere.
//
// It compares one number. Where the version sits in a save and how it is read is the author's format (the engine
// does not know save formats, v8-arch-gyo2-save-unaware-means-format-only). The author keeps one Policy, writes
// Current() into its saves and, on a load, reads the number back and hands it to Check before the new World is
// swapped in (before ReplaceWorld, in its extra prepare function, or in the description's prepare function for a
// startup load). A refusal is an OrchestrationError (PrepareFailed, so a prepare function can return it as it is;
// ReplaceWorld carries it unchanged, O-8) whose reporterCode is SaveVersionCodes::tooOld or tooNew: tell refusals
// apart by reporterCode. A load that does not call Check takes any save.
//
//     constexpr GYO::Engine::SaveVersion::Policy kSaveVersion{5, 4};  // current 5, oldest taken 4
//
// Not: reading or writing saves, a header format, converting old saves, events or logs (a refusal is a Result).

#include "engine/base/Result.hpp"
#include "engine/orchestration/OrchestrationError.hpp"

#include <cstdint>
#include <string_view>

namespace GYO::Engine::SaveVersion {

// The reporter codes of a refusal (OrchestrationError::reporterCode).
struct SaveVersionCodes final {
    static constexpr std::string_view tooOld = "save-version-too-old";
    static constexpr std::string_view tooNew = "save-version-too-new";
};

// The product's two numbers. oldestSupported == current takes only the current version; oldestSupported > current
// takes nothing (the author's mistake: nothing checks it).
class Policy final {
public:
    constexpr Policy(std::uint32_t current, std::uint32_t oldestSupported) noexcept
        : current_(current), oldestSupported_(oldestSupported) {}

    // The version to write into a save now.
    [[nodiscard]] constexpr std::uint32_t Current() const noexcept { return current_; }
    // Takes oldestSupported <= saved <= current; anything else is refused (PrepareFailed with the code).
    [[nodiscard]] Base::Result<void, Orchestration::OrchestrationError> Check(std::uint32_t saved) const;

private:
    std::uint32_t current_;
    std::uint32_t oldestSupported_;
};

} // namespace GYO::Engine::SaveVersion
