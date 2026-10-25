// The child process of console_check.py (docs/architecture/os.md, Console).
//
//   --shutdown           install the shutdown-request handlers, print "ready",
//                        wait (10 s at most) for a request, then 300 ms more so
//                        that later requests can arrive, and print
//                        "requested reason=<reason>" (exit 0; 4 on timeout).
//                        On POSIX it first prints the sa_flags std::signal sets
//                        on this C library and those of the installed SIGINT
//                        and SIGTERM handlers.
//   --close-own-console  like --shutdown, but close the console window the
//                        process runs in (Windows); exit 5 without one.
//   --ctrl-c-own-console N
//                        like --shutdown, but send Ctrl+C N times to the
//                        process's own console (Windows; started with a new
//                        console, so no other process gets it); exit 5 when
//                        the event cannot be sent.
//   --echo-arguments     print each UTF-8 argument after the program name, in
//                        hexadecimal, one per line.
#include "engine/os/Console.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <thread>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <csignal>
#include <signal.h>
#endif

using namespace Engine::Os::Console;
using namespace std::chrono_literals;

namespace {

#if !defined(_WIN32)
void Ignore(int) {}

int FlagsOf(const int signal) {
    struct sigaction current {};
    sigaction(signal, nullptr, &current);
    return current.sa_flags;
}
#endif

enum class Trigger { None, CloseOwnConsole, CtrlCOwnConsole };

int WaitForShutdown(const Trigger trigger, const int count) {
#if !defined(_WIN32)
    // The reference: how std::signal installs a handler on this C library.
    std::signal(SIGUSR1, &Ignore);
    std::printf("reference_flags=%#x\n", FlagsOf(SIGUSR1));
#endif
    if (auto installed = InstallShutdownRequests(); !installed) {
        std::printf("install failed: %s\n", Engine::Base::Describe(installed.error()).c_str());
        return 3;
    }
#if !defined(_WIN32)
    std::printf("sigint_flags=%#x\nsigterm_flags=%#x\n", FlagsOf(SIGINT), FlagsOf(SIGTERM));
#endif
    std::printf("ready\n");
    std::fflush(stdout);
    if (trigger == Trigger::CtrlCOwnConsole) {
#if defined(_WIN32)
        for (int sent = 0; sent < count; ++sent) {
            if (!::GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0)) {
                std::printf("cannot send ctrl-c: %lu\n", ::GetLastError());
                return 5;
            }
            std::this_thread::sleep_for(50ms);
        }
#else
        static_cast<void>(count);
        std::printf("cannot send ctrl-c: not Windows\n");
        return 5;
#endif
    }
    if (trigger == Trigger::CloseOwnConsole) {
#if defined(_WIN32)
        const HWND console = ::GetConsoleWindow();
        if (console == nullptr) {
            std::printf("no console window\n");
            return 5;
        }
        ::PostMessageW(console, WM_CLOSE, 0, 0);
#else
        std::printf("no console window\n");
        return 5;
#endif
    }
    const auto until = std::chrono::steady_clock::now() + 10s;
    while (!RequestedShutdown() && std::chrono::steady_clock::now() < until) std::this_thread::sleep_for(10ms);
    if (!RequestedShutdown()) {
        std::printf("timeout\n");
        return 4;
    }
    std::this_thread::sleep_for(300ms);
    std::printf("requested reason=%s\n", ToString(*RequestedShutdown()));
    std::fflush(stdout);
    return 0;
}

int EchoArguments(const int argc, char** argv) {
    auto arguments = CommandLineUtf8(argc, argv);
    if (!arguments) {
        std::printf("command line failed: %s\n", Engine::Base::Describe(arguments.error()).c_str());
        return 3;
    }
    for (std::size_t index = 2; index < arguments->size(); ++index) {
        std::string hex;
        for (const unsigned char byte : (*arguments)[index]) {
            char pair[3]{};
            std::snprintf(pair, sizeof pair, "%02x", byte);
            hex += pair;
        }
        std::printf("arg=%s\n", hex.c_str());
    }
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    const std::string_view mode = argc > 1 ? argv[1] : "";
    if (mode == "--shutdown") return WaitForShutdown(Trigger::None, 0);
    if (mode == "--close-own-console") return WaitForShutdown(Trigger::CloseOwnConsole, 0);
    if (mode == "--ctrl-c-own-console" && argc > 2) return WaitForShutdown(Trigger::CtrlCOwnConsole, std::atoi(argv[2]));
    if (mode == "--echo-arguments") return EchoArguments(argc, argv);
    std::printf("usage: gyo_os_console_probe --shutdown | --close-own-console | --ctrl-c-own-console N |"
                " --echo-arguments ...\n");
    return 2;
}
