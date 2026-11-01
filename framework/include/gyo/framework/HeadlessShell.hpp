#pragma once

#include <functional>
#include <span>
#include <string>
#include <string_view>

namespace Gyo::Tools::Framework {

// The process's command line as Engine::Os::Console::CommandLineUtf8 gives
// it: UTF-8, the program name first.
using HeadlessMain = std::function<int(std::span<const std::string> arguments)>;
// Called instead of the body when the command line has no UTF-8 form, with
// Engine's description of the failure. Returns the exit code.
using CommandLineFailure = std::function<int(std::string_view reason)>;

// The headless shell (docs/framework/architecture.md, Design Invariant 6).
// While it runs, UTF-8 written to the console shows as UTF-8
// (Os::Console::Utf8ConsoleOutput). It converts the command line, calls the
// body once and returns its value unchanged; a window shell may run inside
// the body. It catches nothing and logs nothing.
[[nodiscard]] int RunHeadlessShell(int argc, char* argv[], const HeadlessMain& body,
                                   const CommandLineFailure& failure);

} // namespace Gyo::Tools::Framework
