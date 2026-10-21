// A program on GYO::PlatformSDLMain for the entry point tests: exits with the
// code given as argument, or throws when the argument is "throw".
#include "engine/platform/sdl/EntryPoint.hpp"

#include <cstdlib>
#include <stdexcept>
#include <string_view>

int GyoMain(int argc, char* argv[]) {
    if (argc > 1 && std::string_view(argv[1]) == "throw") throw std::runtime_error("entry-probe");
    return argc > 1 ? std::atoi(argv[1]) : 0;
}
