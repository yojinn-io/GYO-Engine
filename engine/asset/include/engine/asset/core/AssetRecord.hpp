#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>

#include "engine/asset/AssetId.hpp"
#include "engine/asset/AssetType.hpp"
#include "engine/asset/AssetState.hpp"
#include "engine/asset/AssetError.hpp"

#include "engine/asset/core/AnyAsset.hpp"
#include "engine/base/Error.hpp"

namespace Engine::Asset::Core {
    using AssetError = Base::Error<AssetErrorCode>;

    // AssetRecord:
    // - 「1つの AssetId」に対する状態・キャッシュ実体・エラー等の集合
    // - AssetManager / AssetPipeline が更新する
    // - AssetStorage が所有する
    struct AssetRecord final {
        AssetId   id{};
        AssetType type{};

        // - AssetCatalog が解決済みにする設計なら、LoadContext に渡しやすい
        std::string resolvedPath;

        AssetState state = AssetState::Unloaded;

        // 世代：AssetHandle の stale 検出に使える（AssetManager側で運用）
        std::uint32_t generation = 1;

        // 実体（型消去）
        AnyAsset asset{};

        // 失敗時の情報（成功時は空にしておく）
        AssetError error{};

        // Total number of references across every still-live handle generation.
        // A reload makes old handles stale for reads, but it does not revoke the
        // references acquired by the Load calls which returned those handles.
        std::uint32_t refCount = 0;

        // Reference accounting is generation-aware so releasing an old handle
        // cannot consume a reference acquired by the current generation.
        std::unordered_map<std::uint32_t, std::uint32_t> refCountsByGeneration;

        // ---- helpers ----
        bool IsReady() const noexcept { return state == AssetState::Ready; }
        bool IsFailed() const noexcept { return state == AssetState::Failed; }
        bool IsLoading() const noexcept { return state == AssetState::Loading; }

        void AddReference(std::uint32_t handleGeneration) {
            ++refCount;
            ++refCountsByGeneration[handleGeneration];
        }

        bool ReleaseReference(std::uint32_t handleGeneration) {
            const auto it = refCountsByGeneration.find(handleGeneration);
            if (it == refCountsByGeneration.end() || it->second == 0) {
                return false;
            }

            --it->second;
            if (refCount > 0) {
                --refCount;
            }
            if (it->second == 0) {
                refCountsByGeneration.erase(it);
            }
            return true;
        }

        void AdvanceGeneration() noexcept {
            ++generation;
            // AssetHandle reserves zero for Invalid().
            if (generation == 0) {
                generation = 1;
            }
        }

        void MarkLoading() noexcept { state = AssetState::Loading; }

        void SetReady(AnyAsset a) {
            asset = std::move(a);
            error = AssetError{}; // clear
            state = AssetState::Ready;
        }

        void SetFailed(AssetError e) {
            asset.Reset();
            error = std::move(e);
            state = AssetState::Failed;
        }

        void ResetToUnloaded() {
            asset.Reset();
            error = AssetError{};
            state = AssetState::Unloaded;
            // generation は「同一IDで中身が変わる」時に増やすことが多い
        }
    };

} // namespace Engine::Asset::Core
