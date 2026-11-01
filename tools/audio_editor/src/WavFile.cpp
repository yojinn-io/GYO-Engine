#include "gyo/audio_editor/WavFile.hpp"

#include <bit>
#include <cstring>
#include <string>

namespace Gyo::Tools::AudioEditor {
namespace {

constexpr std::uint16_t kFormatPcm = 1;
constexpr std::uint16_t kFormatFloat = 3;
constexpr std::uint16_t kFormatExtensible = 0xFFFE;

std::uint32_t ReadU32(const std::span<const std::byte> bytes, const std::size_t at) {
    return std::to_integer<std::uint32_t>(bytes[at]) | (std::to_integer<std::uint32_t>(bytes[at + 1]) << 8) |
           (std::to_integer<std::uint32_t>(bytes[at + 2]) << 16) |
           (std::to_integer<std::uint32_t>(bytes[at + 3]) << 24);
}

std::uint16_t ReadU16(const std::span<const std::byte> bytes, const std::size_t at) {
    return static_cast<std::uint16_t>(std::to_integer<unsigned>(bytes[at]) |
                                      (std::to_integer<unsigned>(bytes[at + 1]) << 8));
}

bool HasTag(const std::span<const std::byte> bytes, const std::size_t at, const char* tag) {
    return std::memcmp(bytes.data() + at, tag, 4) == 0;
}

AudioEditorError Invalid(std::string message) {
    return AudioEditorError::Make(AudioEditorErrorCode::InvalidWav, std::move(message));
}

// One sample of the data chunk, scaled as convert_wav.py scales it.
double Sample(const std::span<const std::byte> frames, const std::size_t index, const std::uint16_t tag,
              const std::uint16_t bits) {
    const std::size_t at = index * (bits / 8u);
    if (tag == kFormatFloat) {
        return static_cast<double>(std::bit_cast<float>(ReadU32(frames, at)));
    }
    switch (bits) {
    case 8: return (std::to_integer<int>(frames[at]) - 128) / 128.0;
    case 16: return static_cast<std::int16_t>(ReadU16(frames, at)) / 32768.0;
    case 24: {
        const std::uint32_t value = std::to_integer<std::uint32_t>(frames[at]) |
                                    (std::to_integer<std::uint32_t>(frames[at + 1]) << 8) |
                                    (std::to_integer<std::uint32_t>(frames[at + 2]) << 16);
        const auto signedValue = static_cast<std::int32_t>(value & 0x800000u ? value | 0xFF000000u : value);
        return signedValue / 8388608.0;
    }
    default: return static_cast<std::int32_t>(ReadU32(frames, at)) / 2147483648.0;
    }
}

} // namespace

Result<DecodedWav, AudioEditorError> DecodeWav(const std::span<const std::byte> bytes) {
    if (bytes.size() < 12 || !HasTag(bytes, 0, "RIFF") || !HasTag(bytes, 8, "WAVE")) {
        return Engine::Base::Err(Invalid("not a RIFF/WAVE file"));
    }
    bool haveFormat = false;
    std::uint16_t tag = 0;
    std::uint16_t channels = 0;
    std::uint32_t rate = 0;
    std::uint16_t bits = 0;
    std::span<const std::byte> frames;
    bool haveData = false;
    // Chunks as the script walks them: a chunk may be cut short by the end of
    // the file, and the last fmt and data chunks win.
    for (std::size_t offset = 12; offset + 8 <= bytes.size();) {
        const std::uint32_t size = ReadU32(bytes, offset + 4);
        const std::size_t begin = offset + 8;
        const std::size_t end = begin + size < begin || begin + size > bytes.size() ? bytes.size() : begin + size;
        const std::span<const std::byte> body = bytes.subspan(begin, end - begin);
        if (HasTag(bytes, offset, "fmt ")) {
            if (body.size() < 16) {
                return Engine::Base::Err(Invalid("fmt chunk shorter than 16 bytes"));
            }
            tag = ReadU16(body, 0);
            channels = ReadU16(body, 2);
            rate = ReadU32(body, 4);
            bits = ReadU16(body, 14);
            if (tag == kFormatExtensible && body.size() >= 26) {
                tag = ReadU16(body, 24);
            }
            haveFormat = true;
        } else if (HasTag(bytes, offset, "data")) {
            frames = body;
            haveData = true;
        }
        offset = begin + static_cast<std::size_t>(size) + (size & 1u);
        if (offset < begin) {
            break; // a size that overflows ends the walk
        }
    }
    if (!haveFormat || !haveData) {
        return Engine::Base::Err(Invalid("missing fmt or data chunk"));
    }
    if (channels != 1 && channels != 2) {
        return Engine::Base::Err(AudioEditorError::Make(AudioEditorErrorCode::UnsupportedFormat,
                                                        std::to_string(channels) + " channels; a clip has 1 or 2"));
    }
    const bool supported = (tag == kFormatFloat && bits == 32) ||
                           (tag == kFormatPcm && (bits == 8 || bits == 16 || bits == 24 || bits == 32));
    if (!supported || rate == 0) {
        return Engine::Base::Err(AudioEditorError::Make(
            AudioEditorErrorCode::UnsupportedFormat,
            "unsupported sample format (tag " + std::to_string(tag) + ", " + std::to_string(bits) + " bits, " +
                std::to_string(rate) + " Hz)"));
    }

    DecodedWav decoded;
    decoded.sampleRate = rate;
    decoded.formatTag = tag;
    decoded.bits = bits;
    std::size_t count = frames.size() / (bits / 8u);
    count -= count % channels;
    decoded.planes.assign(channels, std::vector<double>(count / channels));
    for (std::size_t index = 0; index < count; ++index) {
        decoded.planes[index % channels][index / channels] = Sample(frames, index, tag, bits);
    }
    return decoded;
}

std::vector<std::byte> EncodePcm16Wav(const std::uint16_t channels, const std::uint32_t sampleRate,
                                      const std::span<const std::int16_t> interleaved) {
    std::vector<std::byte> out;
    out.reserve(44 + interleaved.size() * 2);
    const auto put = [&out](const std::uint32_t value, const int bytes) {
        for (int index = 0; index < bytes; ++index) {
            out.push_back(static_cast<std::byte>((value >> (8 * index)) & 0xFFu));
        }
    };
    const auto tag = [&out](const char* text) {
        for (int index = 0; index < 4; ++index) out.push_back(static_cast<std::byte>(text[index]));
    };
    const auto dataBytes = static_cast<std::uint32_t>(interleaved.size() * 2);
    tag("RIFF");
    put(36 + dataBytes, 4);
    tag("WAVE");
    tag("fmt ");
    put(16, 4);
    put(kFormatPcm, 2);
    put(channels, 2);
    put(sampleRate, 4);
    put(sampleRate * channels * 2u, 4);
    put(channels * 2u, 2);
    put(16, 2);
    tag("data");
    put(dataBytes, 4);
    for (const std::int16_t sample : interleaved) {
        put(static_cast<std::uint16_t>(sample), 2);
    }
    return out;
}

} // namespace Gyo::Tools::AudioEditor
