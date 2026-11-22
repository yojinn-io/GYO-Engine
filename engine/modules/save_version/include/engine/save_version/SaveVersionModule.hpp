#pragma once

// The save version Module (restore-entry declaration docs/architecture/target/v2/restore-entry-declaration.zh-Hant.md,
// section 19; Design Invariants SV-1 to SV-3 in docs/architecture/target/v2/save-version.zh-Hant.md;
// v8-arch-gyo2-save-version-module). Not carried by default (config/modules.csv default=no); a product enables it
// with gyo_product_units(... MODULES save_version).
//
// It compares one number. Where the version sits in a save and how it is read is the author's format (the engine
// does not know save formats, v8-arch-gyo2-save-unaware-means-format-only): the author writes Policy::Current()
// into its save and, on a load, reads the number back and hands it to Policy::Check before the new World is swapped
// in (before ReplaceWorld, in its extra prepare function, or in a prepare function a startup load registers from
// its build callback). A refusal is an OrchestrationError whose reporterCode is SaveVersionCodes::tooOld or tooNew;
// returned from a prepare function, ReplaceWorld carries it unchanged (ReplacePrepareFailed, O-8). Enabling it blocks
// nothing by itself: a load that does not call Check takes any save.
//
// Its duties: keep the two numbers given in SaveVersionOptions, check one. Not: reading or writing saves, a header
// format, converting old saves, events or logs (a refusal is a Result). It registers nothing in any World.

#include "engine/base/Result.hpp"
#include "engine/orchestration/Module.hpp"
#include "engine/orchestration/OrchestrationError.hpp"
#include "engine/orchestration/Setup.hpp"

#include <cstdint>
#include <string_view>

namespace GYO::Engine::SaveVersion {

// The product's two numbers (ProductDescription::options). Required: a product that enables the Module without
// them, or with oldestSupported > current, fails to start (a Configure report).
struct SaveVersionOptions final {
    std::uint32_t current = 0;
    std::uint32_t oldestSupported = 0;
};

// The reporter codes of a refusal (OrchestrationError::reporterCode) and of the Configure reports.
struct SaveVersionCodes final {
    static constexpr std::string_view tooOld = "save-version-too-old";
    static constexpr std::string_view tooNew = "save-version-too-new";
    static constexpr std::string_view optionsMissing = "save-version-options-missing";
    static constexpr std::string_view optionsOrder = "save-version-options-order";
};

// What Configure puts into the Product's buffer (Product::Buffer(), WorldSetup::ProductBuffer()).
class Policy final {
public:
    Policy(std::uint32_t current, std::uint32_t oldestSupported) noexcept
        : current_(current), oldestSupported_(oldestSupported) {}

    // The version to write into a save now.
    [[nodiscard]] std::uint32_t Current() const noexcept { return current_; }
    [[nodiscard]] std::uint32_t OldestSupported() const noexcept { return oldestSupported_; }
    // Takes oldestSupported <= saved <= current; anything else is refused (PrepareFailed with the code).
    [[nodiscard]] Base::Result<void, Orchestration::OrchestrationError> Check(std::uint32_t saved) const;

private:
    std::uint32_t current_;
    std::uint32_t oldestSupported_;
};

class SaveVersionModule final : public Orchestration::Module {
public:
    SaveVersionModule() = default;

    void Configure(Orchestration::ProductSetup& setup) override;
    // Registers nothing (SV-2).
    void Build(Orchestration::WorldSetup& setup) override;
};

} // namespace GYO::Engine::SaveVersion
