#pragma once

// Test support: a Beat as text, one output or choice per line, so a test
// compares a whole beat with one string.
//   merchant: Welcome to my stall. [hub_welcome]      a line (speaker, text, line id, other tags)
//   > give_item("sword", 2)                           a command
//   * Buy a sword [hub_buy]                           a choice
//   (end)                                             the story ended

#include <string>
#include <variant>

#include "plugins/ink/Dialogue.hpp"

namespace GYO::Plugins::Ink::Test {

inline std::string RenderValue(const Value& value) {
    struct Visitor {
        std::string operator()(const bool b) const { return b ? "true" : "false"; }
        std::string operator()(const std::int32_t i) const { return std::to_string(i); }
        std::string operator()(const float f) const { return "float:" + std::to_string(f); }
        std::string operator()(const std::string& s) const { return "\"" + s + "\""; }
    };
    return std::visit(Visitor{}, value);
}

inline std::string RenderTags(const std::string& lineId, const std::vector<std::string>& tags) {
    std::string out;
    if (!lineId.empty()) out += " [" + lineId + "]";
    for (const std::string& tag : tags) out += " #" + tag;
    return out;
}

inline std::string Render(const Beat& beat) {
    std::string out;
    for (const Output& output : beat.outputs) {
        if (const auto* line = std::get_if<Line>(&output)) {
            out += (line->speaker.empty() ? "" : line->speaker + ": ") + line->text +
                   RenderTags(line->lineId, line->tags) + "\n";
        } else {
            const auto& command = std::get<Command>(output);
            out += "> " + command.name + "(";
            for (std::size_t i = 0; i < command.args.size(); ++i) {
                out += (i == 0 ? "" : ", ") + RenderValue(command.args[i]);
            }
            out += ")\n";
        }
    }
    for (const Choice& choice : beat.choices) {
        out += "* " + choice.text + RenderTags(choice.lineId, choice.tags) + "\n";
    }
    if (beat.ended) out += "(end)\n";
    return out;
}

} // namespace GYO::Plugins::Ink::Test
