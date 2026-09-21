#include "engine/asset/loaders/FontLoader.hpp"

namespace Engine::Asset::Loaders {
    using AssetError = Base::Error<AssetErrorCode>;

    AssetType FontLoader::GetType() const noexcept {
        static const AssetType kType = AssetType::FromString("font");
        return kType;
    }

    Base::Result<Core::AnyAsset, AssetError>
    FontLoader::Load(Base::ConstSpan<std::byte> bytes, const Loading::LoadContext& ctx) {
        if (bytes.empty()) {
            return Base::Result<Core::AnyAsset, AssetError>::Err(
                AssetError::Make(AssetErrorCode::DecodeFailed, "Font: empty file", ctx.resolvedPath));
        }

        auto font = std::make_shared<FontAsset>();
        font->bytes.assign(bytes.begin(), bytes.end());

        return Base::Result<Core::AnyAsset, AssetError>::Ok(
            Core::AnyAsset::FromShared<FontAsset>(std::move(font))
        );
    }

} // namespace Engine::Asset::Loaders
