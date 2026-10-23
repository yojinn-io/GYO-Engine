// IP-4 ip4b: the capture file writer of the SDL GPU backend (ip4-declaration.md section 2).
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "engine/render/backend/sdl_gpu/SceneCaptureFile.hpp"

#include <SDL3/SDL.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {

std::vector<char> ReadFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), {}};
}

// A directory of its own, so test runs in parallel do not share files.
std::filesystem::path TestDirectory() {
    return std::filesystem::temp_directory_path() /
           ("gyo-capture-test-" + std::to_string(reinterpret_cast<std::uintptr_t>(&TestDirectory)) + "-" +
            std::to_string(static_cast<long long>(SDL_GetTicksNS())));
}

} // namespace

TEST_CASE("SaveCaptureBmp writes the bytes SDL writes for the capture") {
    // Asymmetric: red is not blue, alpha is not opaque, width is not height.
    Engine::Render::SceneCapture capture{3, 2, {}};
    for (std::uint8_t index = 0; index < 6; ++index) {
        capture.rgba8.insert(capture.rgba8.end(), {static_cast<std::uint8_t>(10 + index), 0x20, 0xF0,
                                                   static_cast<std::uint8_t>(0x80 + index)});
    }
    const auto directory = TestDirectory();
    std::filesystem::create_directories(directory);
    const auto ours = directory / std::filesystem::path(u8"capture-é日.bmp");
    const auto reference = directory / "reference.bmp";
    REQUIRE(Engine::Render::Backend::SdlGpu::SaveCaptureBmp(capture, ours));
    SDL_Surface* surface = SDL_CreateSurfaceFrom(3, 2, SDL_PIXELFORMAT_RGBA32, capture.rgba8.data(), 12);
    REQUIRE(surface != nullptr);
    const std::u8string referenceUtf8 = reference.u8string();
    REQUIRE(SDL_SaveBMP(surface, reinterpret_cast<const char*>(referenceUtf8.c_str())));
    SDL_DestroySurface(surface);
    const auto written = ReadFile(ours);
    CHECK_FALSE(written.empty());
    CHECK_MESSAGE(written == ReadFile(reference), "the capture file has the bytes SDL writes for the capture");
    std::filesystem::remove_all(directory);
}

TEST_CASE("SaveCaptureBmp refuses a capture whose pixels do not match its size") {
    const Engine::Render::SceneCapture capture{4, 4, std::vector<std::uint8_t>(10)};
    const auto saved = Engine::Render::Backend::SdlGpu::SaveCaptureBmp(capture, "unused.bmp");
    REQUIRE_FALSE(saved);
    CHECK(saved.error().code == Engine::Render::RenderErrorCode::InvalidArgument);
}
