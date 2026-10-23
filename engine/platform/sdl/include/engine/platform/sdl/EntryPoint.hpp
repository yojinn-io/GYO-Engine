#pragma once

// The program's entry point (docs/architecture/platform-sdl.md, "Entry point").
// A product defines GyoMain and links GYO::PlatformSDLMain, which owns main:
// the platform sets itself up, passes argv (converted to UTF-8 on Windows; the
// bytes as given elsewhere, UTF-8 in practice) and
// calls GyoMain through Engine::Platform::Sdl::RunEntry, so an exception from
// GyoMain is logged and becomes exit code 1.
int GyoMain(int argc, char* argv[]);
