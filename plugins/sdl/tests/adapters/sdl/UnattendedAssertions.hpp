#pragma once

// Test executables that start SDL link gyo_sdl_unattended_assertions (plugins/sdl/tests/CMakeLists.txt): a failed SDL
// assertion (SDL_assert_release; for example SDL_GPU's validation of a texture's usage, SDL_gpu.c:1274) prints SDL's
// condition and place to stderr and ends the process, instead of SDL's default handler, which shows a message box
// and waits for a person (SDL_PromptAssertion, SDL_assert.c:129-220). For running tests unattended; not a guard
// against misuse. In GYO's words: no SDL type here (SDL-1).

namespace GYO::Plugins::SDL {

// Whether this process's SDL assertion handler is the unattended one (installed before main).
[[nodiscard]] bool UnattendedAssertionsInstalled() noexcept;

} // namespace GYO::Plugins::SDL
