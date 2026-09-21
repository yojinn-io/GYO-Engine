#include "engine/asset/loaders/SoundLoader.hpp"

#include <cstddef>
#include <cstring>

namespace Engine::Asset::Loaders {

    static std::uint16_t ReadU16LE(const unsigned char* p) {
        return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
    }
    static std::uint32_t ReadU32LE(const unsigned char* p) {
        return static_cast<std::uint32_t>(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
    }

    AssetType SoundLoader::GetType() const noexcept {
        static const AssetType kType = AssetType::FromString("sound");
        return kType;
    }

    Base::Result<Core::AnyAsset, AssetError>
    SoundLoader::Load(Base::ConstSpan<std::byte> bytes, const Loading::LoadContext& ctx) {
        const auto* p = reinterpret_cast<const unsigned char*>(bytes.data());
        const std::size_t n = bytes.size();

        if (n < 12) {
            return Base::Result<Core::AnyAsset, AssetError>::Err(
                AssetError::Make(AssetErrorCode::DecodeFailed, "WAV: file too small", ctx.resolvedPath));
        }

        if (std::memcmp(p, "RIFF", 4) != 0 || std::memcmp(p + 8, "WAVE", 4) != 0) {
            return Base::Result<Core::AnyAsset, AssetError>::Err(
                AssetError::Make(AssetErrorCode::UnsupportedFormat, "Sound: only WAV(RIFF/WAVE) supported (PCM16)", ctx.resolvedPath));
        }

        std::uint16_t audioFormat = 0;
        std::uint16_t channels = 0;
        std::uint32_t sampleRate = 0;
        std::uint16_t bitsPerSample = 0;

        const unsigned char* dataPtr = nullptr;
        std::uint32_t dataSize = 0;

        std::size_t off = 12;
        while (off + 8 <= n) {
            const unsigned char* ck = p + off;
            const std::uint32_t ckSize = ReadU32LE(ck + 4);
            off += 8;

            if (off + ckSize > n) break;

            if (std::memcmp(ck, "fmt ", 4) == 0) {
                if (ckSize < 16) {
                    return Base::Result<Core::AnyAsset, AssetError>::Err(
                        AssetError::Make(AssetErrorCode::DecodeFailed, "WAV: invalid fmt chunk", ctx.resolvedPath));
                }
                audioFormat   = ReadU16LE(p + off + 0);
                channels      = ReadU16LE(p + off + 2);
                sampleRate    = ReadU32LE(p + off + 4);
                bitsPerSample = ReadU16LE(p + off + 14);
            } else if (std::memcmp(ck, "data", 4) == 0) {
                dataPtr = p + off;
                dataSize = ckSize;
            }

            // チャンクは word align（奇数なら+1）
            off += ckSize + (ckSize & 1u);
        }

        if (audioFormat != 1) {
            return Base::Result<Core::AnyAsset, AssetError>::Err(
                AssetError::Make(AssetErrorCode::UnsupportedFormat, "WAV: only PCM supported", ctx.resolvedPath));
        }
        if (channels == 0 || (channels != 1 && channels != 2)) {
            return Base::Result<Core::AnyAsset, AssetError>::Err(
                AssetError::Make(AssetErrorCode::UnsupportedFormat, "WAV: only mono/stereo supported", ctx.resolvedPath));
        }
        if (bitsPerSample != 16) {
            return Base::Result<Core::AnyAsset, AssetError>::Err(
                AssetError::Make(AssetErrorCode::UnsupportedFormat, "WAV: only 16-bit supported", ctx.resolvedPath));
        }
        if (!dataPtr || dataSize == 0) {
            return Base::Result<Core::AnyAsset, AssetError>::Err(
                AssetError::Make(AssetErrorCode::DecodeFailed, "WAV: missing data chunk", ctx.resolvedPath));
        }
        if ((dataSize % 2) != 0) {
            return Base::Result<Core::AnyAsset, AssetError>::Err(
                AssetError::Make(AssetErrorCode::DecodeFailed, "WAV: data size not aligned", ctx.resolvedPath));
        }

        const std::size_t sampleCount = dataSize / 2;
        auto snd = std::make_shared<SoundAsset>();
        snd->sampleRate = sampleRate;
        snd->channels = channels;
        snd->pcm16.resize(sampleCount);

        // little-endian PCM16 を int16 に変換
        for (std::size_t i = 0; i < sampleCount; ++i) {
            const unsigned char* s = dataPtr + i * 2;
            std::uint16_t u = static_cast<std::uint16_t>(s[0] | (s[1] << 8));
            snd->pcm16[i] = static_cast<std::int16_t>(u);
        }

        return Base::Result<Core::AnyAsset, AssetError>::Ok(
            Core::AnyAsset::FromShared<SoundAsset>(std::move(snd))
        );
    }

} // namespace Engine::Asset::Loaders
