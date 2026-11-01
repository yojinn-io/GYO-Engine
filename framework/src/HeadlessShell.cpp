#include "gyo/framework/HeadlessShell.hpp"

#include "engine/base/Assert.hpp"
#include "engine/base/Error.hpp"
#include "engine/os/Console.hpp"

namespace Gyo::Tools::Framework {

int RunHeadlessShell(const int argc, char* argv[], const HeadlessMain& body, const CommandLineFailure& failure) {
    GYO_ASSERT(body && failure);
    const auto console = Engine::Os::Console::Utf8ConsoleOutput::Enable();
    const auto commandLine = Engine::Os::Console::CommandLineUtf8(argc, argv);
    if (!commandLine) {
        const std::string reason = Engine::Base::Describe(commandLine.error());
        return failure(reason);
    }
    return body(std::span<const std::string>(commandLine.value()));
}

} // namespace Gyo::Tools::Framework
