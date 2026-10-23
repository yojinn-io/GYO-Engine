// main of every program that links GYO::PlatformSDLMain. SDL_main.h supplies
// the platform's real entry (for example converting the Windows command line to
// UTF-8) and calls this main.
#include <SDL3/SDL_main.h>

#include "engine/platform/sdl/EntryPoint.hpp"
#include "engine/platform/sdl/SdlPlatform.hpp"

int main(int argc, char* argv[]) {
    return Engine::Platform::Sdl::RunEntry(&GyoMain, argc, argv);
}
