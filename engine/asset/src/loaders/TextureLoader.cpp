#include "engine/asset/loaders/TextureLoader.hpp"

#include <cctype>
#include <cstring>
#include <string>
#include <string_view>

namespace Engine::Asset::Loaders {

    static bool StartsWith(std::string_view s, std::string_view p) {
        return s.size() >= p.size() && s.substr(0, p.size()) == p;
    }

    static const char* SkipSpaces(const char* p, const char* end) {
        while (p < end && std::isspace(static_cast<unsigned char>(*p))) ++p;
        return p;
    }

    static const char* SkipCommentsAndSpaces(const char* p, const char* end) {
        while (true) {
            p = SkipSpaces(p, end);
            if (p >= end) return p;
            if (*p == '#') {
                while (p < end && *p != '\n') ++p;
                continue;
            }
            return p;
        }
    }

    static bool ReadInt(const char*& p, const char* end, int& out) {
        p = SkipCommentsAndSpaces(p, end);
        if (p >= end) return false;

        int sign = 1;
        if (*p == '-') { sign = -1; ++p; }

        if (p >= end || !std::isdigit(static_cast<unsigned char>(*p))) return false;

        int v = 0;
        while (p < end && std::isdigit(static_cast<unsigned char>(*p))) {
            v = v * 10 + (*p - '0');
            ++p;
        }
        out = v * sign;
        return true;
    }

    // PPM (P6 binary, P3 ascii) をデコードして RGBA8 を返す
    static Base::Result<std::shared_ptr<TextureAsset>, AssetError>
    DecodePPM(Base::ConstSpan<std::byte> bytes, const Loading::LoadContext& ctx) {
        const char* p = reinterpret_cast<const char*>(bytes.data());
        const char* end = p + bytes.size();

        if (end - p < 2) {
            return Base::Result<std::shared_ptr<TextureAsset>, AssetError>::Err(
                AssetError::Make(AssetErrorCode::DecodeFailed, "PPM: file too small", ctx.resolvedPath));
        }

        const bool isP6 = (p[0] == 'P' && p[1] == '6');
        const bool isP3 = (p[0] == 'P' && p[1] == '3');
        if (!isP6 && !isP3) {
            return Base::Result<std::shared_ptr<TextureAsset>, AssetError>::Err(
                AssetError::Make(AssetErrorCode::UnsupportedFormat, "Texture: only PPM(P6/P3) supported (no external decoder)", ctx.resolvedPath));
        }
        p += 2;

        int w = 0, h = 0, maxv = 0;
        if (!ReadInt(p, end, w) || !ReadInt(p, end, h) || !ReadInt(p, end, maxv)) {
            return Base::Result<std::shared_ptr<TextureAsset>, AssetError>::Err(
                AssetError::Make(AssetErrorCode::DecodeFailed, "PPM: header parse failed", ctx.resolvedPath));
        }
        if (w <= 0 || h <= 0) {
            return Base::Result<std::shared_ptr<TextureAsset>, AssetError>::Err(
                AssetError::Make(AssetErrorCode::DecodeFailed, "PPM: invalid width/height", ctx.resolvedPath));
        }
        if (maxv != 255) {
            return Base::Result<std::shared_ptr<TextureAsset>, AssetError>::Err(
                AssetError::Make(AssetErrorCode::UnsupportedFormat, "PPM: only maxval=255 supported", ctx.resolvedPath));
        }

        // ヘッダ後の1文字分の空白をスキップ（P6はここからバイナリ）
        p = SkipCommentsAndSpaces(p, end);
        if (p >= end) {
            return Base::Result<std::shared_ptr<TextureAsset>, AssetError>::Err(
                AssetError::Make(AssetErrorCode::DecodeFailed, "PPM: missing body", ctx.resolvedPath));
        }

        auto tex = std::make_shared<TextureAsset>();
        tex->width = static_cast<std::uint32_t>(w);
        tex->height = static_cast<std::uint32_t>(h);
        tex->rgba.resize(static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4);

        if (isP6) {
            const std::size_t need = static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 3;
            const std::size_t remain = static_cast<std::size_t>(end - p);
            if (remain < need) {
                return Base::Result<std::shared_ptr<TextureAsset>, AssetError>::Err(
                    AssetError::Make(AssetErrorCode::DecodeFailed, "PPM(P6): body too small", ctx.resolvedPath));
            }

            const unsigned char* src = reinterpret_cast<const unsigned char*>(p);
            std::size_t di = 0;
            for (std::size_t si = 0; si < need; si += 3) {
                tex->rgba[di + 0] = src[si + 0];
                tex->rgba[di + 1] = src[si + 1];
                tex->rgba[di + 2] = src[si + 2];
                tex->rgba[di + 3] = 255;
                di += 4;
            }
            return Base::Result<std::shared_ptr<TextureAsset>, AssetError>::Ok(std::move(tex));
        }

        // P3 (ASCII)
        // 注意：遅いが最小実装としてOK
        std::size_t di = 0;
        for (int i = 0; i < w * h; ++i) {
            int r = 0, g = 0, b = 0;
            if (!ReadInt(p, end, r) || !ReadInt(p, end, g) || !ReadInt(p, end, b)) {
                return Base::Result<std::shared_ptr<TextureAsset>, AssetError>::Err(
                    AssetError::Make(AssetErrorCode::DecodeFailed, "PPM(P3): body parse failed", ctx.resolvedPath));
            }
            if ((unsigned)r > 255 || (unsigned)g > 255 || (unsigned)b > 255) {
                return Base::Result<std::shared_ptr<TextureAsset>, AssetError>::Err(
                    AssetError::Make(AssetErrorCode::DecodeFailed, "PPM(P3): color out of range", ctx.resolvedPath));
            }
            tex->rgba[di + 0] = static_cast<std::uint8_t>(r);
            tex->rgba[di + 1] = static_cast<std::uint8_t>(g);
            tex->rgba[di + 2] = static_cast<std::uint8_t>(b);
            tex->rgba[di + 3] = 255;
            di += 4;
        }
        return Base::Result<std::shared_ptr<TextureAsset>, AssetError>::Ok(std::move(tex));
    }

    AssetType TextureLoader::GetType() const noexcept {
        // ここは AssetType 実装に合わせて調整
        static const AssetType kType = AssetType::FromString("texture");
        return kType;
    }

    Base::Result<Core::AnyAsset, AssetError>
    TextureLoader::Load(Base::ConstSpan<std::byte> bytes, const Loading::LoadContext& ctx) {
        auto decoded = DecodePPM(bytes, ctx);
        if (!decoded) {
            return Base::Result<Core::AnyAsset, AssetError>::Err(std::move(decoded.error()));
        }

        return Base::Result<Core::AnyAsset, AssetError>::Ok(
            Core::AnyAsset::FromShared<TextureAsset>(std::move(decoded.value()))
        );
    }

} // namespace Engine::Asset::Loaders
