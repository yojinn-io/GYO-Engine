#include "engine/audio/WavLoader.hpp"

#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace GYO::Engine::Audio {
namespace {

using Failure = Asset::LoaderFailure<WavLoaderCode>;

std::uint16_t U16(const std::byte* p) noexcept {
    return static_cast<std::uint16_t>(std::to_integer<unsigned>(p[0]) | (std::to_integer<unsigned>(p[1]) << 8));
}

std::uint32_t U32(const std::byte* p) noexcept {
    return static_cast<std::uint32_t>(std::to_integer<std::uint32_t>(p[0]) | (std::to_integer<std::uint32_t>(p[1]) << 8) |
                                      (std::to_integer<std::uint32_t>(p[2]) << 16) |
                                      (std::to_integer<std::uint32_t>(p[3]) << 24));
}

bool Is(const std::byte* p, const char (&id)[5]) noexcept { return std::memcmp(p, id, 4) == 0; }

struct Chunk final {
    std::size_t offset = 0; // of the body
    std::uint32_t size = 0;
    int count = 0;
};

Base::Err<Failure> Fail(const WavLoaderCode code, const char* message, const Asset::LoadContext& context,
                        const std::string& detail) {
    return Base::Err(Failure::Make(code, message, context.Id().Text() + " (" + context.Path() + "): " + detail));
}

} // namespace

Base::Result<WavSettings, Asset::LoaderFailure<WavLoaderCode>> WavLoader::ParseSettings(
    const std::optional<std::string_view>) const {
    return WavSettings{};
}

Base::Result<Clip, Asset::LoaderFailure<WavLoaderCode>> WavLoader::Load(const std::span<const std::byte> bytes,
                                                                       const WavSettings&,
                                                                       Asset::LoadContext& context) const {
    const std::byte* const p = bytes.data();
    const std::size_t n = bytes.size();
    // 1
    if (n < 12 || !Is(p, "RIFF") || !Is(p + 8, "WAVE")) {
        return Fail(WavLoaderCode::NotWave, "not a RIFF/WAVE file", context, "bytes=" + std::to_string(n));
    }
    // 2: walk every chunk before counting any (rule 3 comes after rule 2).
    Chunk format;
    Chunk data;
    std::size_t at = 12;
    while (n - at >= 8) { // a tail of fewer than 8 bytes is ignored
        const std::uint32_t size = U32(p + at + 4);
        const std::size_t body = at + 8;
        if (size > n - body) {
            return Fail(WavLoaderCode::Truncated, "a chunk runs past the end of the file", context,
                        "chunk at byte " + std::to_string(at) + ", size " + std::to_string(size));
        }
        Chunk* const known = Is(p + at, "fmt ") ? &format : Is(p + at, "data") ? &data : nullptr;
        if (known != nullptr) {
            if (known->count == 0) *known = Chunk{body, size, 0};
            ++known->count;
        }
        // An odd size is followed by a pad byte, which the last chunk may lack.
        const std::size_t padded = std::size_t{size} + (size & 1u);
        at = padded > n - body ? n : body + padded;
    }
    // 3a, 3b
    if (format.count > 1 || data.count > 1) {
        return Fail(WavLoaderCode::DuplicateChunk, "a WAV file has one \"fmt \" and one \"data\" chunk", context,
                    "fmt=" + std::to_string(format.count) + ", data=" + std::to_string(data.count));
    }
    if (format.count == 0 || data.count == 0) {
        return Fail(WavLoaderCode::MissingChunk, "a WAV file has one \"fmt \" and one \"data\" chunk", context,
                    "fmt=" + std::to_string(format.count) + ", data=" + std::to_string(data.count));
    }
    // 4
    if (format.size < 16) {
        return Fail(WavLoaderCode::BadFormatChunk, "the \"fmt \" chunk is shorter than 16 bytes", context,
                    "size=" + std::to_string(format.size));
    }
    const std::byte* const f = p + format.offset;
    const std::uint16_t tag = U16(f);
    const std::uint16_t channels = U16(f + 2);
    const std::uint32_t sampleRate = U32(f + 4);
    const std::uint16_t blockAlign = U16(f + 12);
    const std::uint16_t bits = U16(f + 14);
    // 5
    if (tag != 1 || bits != 16) {
        return Fail(WavLoaderCode::NotPcm16, "a sound is integer PCM of 16 bits", context,
                    "format tag " + std::to_string(tag) + ", " + std::to_string(bits) + " bits");
    }
    // 6: compared, never divided by
    if (std::uint32_t{blockAlign} != std::uint32_t{channels} * 2u) {
        return Fail(WavLoaderCode::BadFormatChunk, "the block align is not channels times 2", context,
                    "block align " + std::to_string(blockAlign) + ", channels " + std::to_string(channels));
    }
    // 7
    if (data.size % 2 != 0) {
        return Fail(WavLoaderCode::PartialSample, "the \"data\" chunk ends inside a sample", context,
                    "size=" + std::to_string(data.size));
    }
    // Little-endian samples, put together byte by byte (any host byte order).
    std::vector<std::int16_t> samples(data.size / 2);
    const std::byte* const d = p + data.offset;
    for (std::size_t i = 0; i < samples.size(); ++i) samples[i] = static_cast<std::int16_t>(U16(d + i * 2));
    // 8
    auto clip = Clip::Make(std::move(samples), channels, sampleRate);
    if (!clip) {
        const AudioError& error = clip.error();
        return Fail(WavLoaderCode::InvalidClip, "the samples are not a clip the mixer can play", context,
                    std::string(error.message) + (error.detail.empty() ? "" : " (" + error.detail + ")"));
    }
    return std::move(clip).value();
}

} // namespace GYO::Engine::Audio
