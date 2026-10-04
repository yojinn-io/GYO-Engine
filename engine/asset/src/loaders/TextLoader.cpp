#include "engine/asset/loaders/TextLoader.hpp"

namespace Engine::Asset::Loaders {

    AssetType TextLoader::GetType() const noexcept {
        static const AssetType kType = AssetType::FromString("text");
        return kType;
    }

    Base::Result<Core::AnyAsset, AssetError>
    TextLoader::Load(Base::ConstSpan<std::byte> bytes, const Loading::LoadContext& ctx) {
        // 空でもテキストとしてはOKだが、運用によってはエラーにしても良い
        auto txt = std::make_shared<TextAsset>();

        if (!bytes.empty()) {
            const char* p = reinterpret_cast<const char*>(bytes.data());
            std::size_t n = bytes.size();

            // UTF-8 BOM を除去
            if (n >= 3 &&
                static_cast<unsigned char>(p[0]) == 0xEF &&
                static_cast<unsigned char>(p[1]) == 0xBB &&
                static_cast<unsigned char>(p[2]) == 0xBF) {
                p += 3;
                n -= 3;
                }

            txt->text.assign(p, p + n);
        }

        (void)ctx;
        return Core::AnyAsset::FromShared<TextAsset>(std::move(txt));
    }

} // namespace Engine::Asset::Loaders
