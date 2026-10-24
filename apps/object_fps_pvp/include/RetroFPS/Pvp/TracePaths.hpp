#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace fps::pvp {

// v7.1 trace-no-overwrite (D191): a restarted Match or Client never truncates
// the trace of an earlier run. The n-th name (n >= 2) inserts ".n" before
// "-commands.jsonl", or before the extension of any other name, or appends it:
// match-commands.jsonl, match.2-commands.jsonl, match.3-commands.jsonl; the
// files named after a movement trace (-ingress.jsonl, -shots.jsonl) follow it.
// Each file stays one run with one trace_end, so readers are unchanged.
[[nodiscard]] inline std::filesystem::path NumberedTracePath(const std::filesystem::path& path, unsigned number) {
    if (number < 2) return path;
    static constexpr std::string_view commands = "-commands.jsonl";
    const auto name = path.filename().string();
    const auto mark = "." + std::to_string(number);
    auto numbered = path;
    if (name.ends_with(commands))
        numbered.replace_filename(name.substr(0, name.size() - commands.size()) + mark + std::string(commands));
    else if (const auto extension = path.extension().string(); !extension.empty() && extension != name)
        numbered.replace_filename(path.stem().string() + mark + extension);
    else
        numbered.replace_filename(name + mark);
    return numbered;
}

// The first numbered path such that it and every file derived from it are free
// (none exists). An empty path stays empty: no trace was asked for.
[[nodiscard]] inline std::filesystem::path FreshTracePath(
    const std::filesystem::path& path,
    const std::function<std::vector<std::filesystem::path>(const std::filesystem::path&)>& derived = {}) {
    if (path.empty()) return path;
    // A path that cannot be checked counts as free: opening it then fails loudly.
    const auto taken = [](const std::filesystem::path& candidate) {
        std::error_code error;
        return std::filesystem::exists(candidate, error);
    };
    for (unsigned number = 1;; ++number) {
        const auto candidate = NumberedTracePath(path, number);
        bool free = !taken(candidate);
        if (free && derived)
            for (const auto& file : derived(candidate)) free = free && (file.empty() || !taken(file));
        if (free) return candidate;
    }
}

} // namespace fps::pvp
