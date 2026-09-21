#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include "engine/asset/AssetId.hpp"
#include "engine/asset/AssetType.hpp"

namespace Engine::Asset {

// AssetRequest：AssetManager に渡す「ロード要求」
// - DTO として public メンバ中心（Get/Set地獄を避ける）
// - ただし “意味が壊れやすい” 部分は enum / helper で制約する
struct AssetRequest final {
    // どのようにロードするか
    enum class Mode : std::uint8_t {
        Auto = 0,   // 既にReadyならそのまま / 無ければロード
        ForceReload // たとえReadyでも再読み込み（hot-reload/開発用）
    };

    // 同期/非同期（将来 async にするための枠）
    enum class SyncWith : std::uint8_t {
        Sync = 0,
        Async
    };

    // 失敗時のフォールバック方針
    enum class Fallback : std::uint8_t {
        None = 0,          // 失敗は失敗として返す
        KeepOldIfAny       // Reload が失敗したら旧キャッシュを維持する
    };

    // 要求の優先度（キューを持つなら使う）
    // 数字が大きいほど高優先度、のように運用してOK
    std::int32_t priority = 0;

    // mode / sync / fallback
    Mode mode = Mode::Auto;
    SyncWith sync = SyncWith::Sync;
    Fallback fallback = Fallback::KeepOldIfAny;

    // type hint（任意）
    // - Catalog の type と一致するべき。未指定なら Catalog を信じる。
    // - 指定する場合、型ミスマッチ検出にも使える。
    AssetType expectedType = AssetType::Invalid();
    bool useTypeHint = false;

    // パス上書き（任意）
    // - 通常は Catalog を使う
    // - テスト/ツール/一時的なデバッグロードで直接指定したい場合用
    std::string overridePath;

    // cache/policy hint（任意）
    // - pin: 強制保持したい場合（AssetLifetime.Pin と連動させる）
    // - keepAliveFrames: この要求だけTTLを上書きしたい場合（0=デフォルト運用）
    bool pin = false;
    std::uint64_t keepAliveFramesOverride = 0;

    // 追加のメタ情報（任意）
    // - 将来：variant で loader オプション（decode設定）なども入れられる
    // - 今は軽量なタグだけ用意
    std::string tag;

    // ---- factories ----
    static AssetRequest Default() { return {}; }

    static AssetRequest Reload() {
        AssetRequest r;
        r.mode = Mode::ForceReload;
        r.fallback = Fallback::KeepOldIfAny;
        return r;
    }

    static AssetRequest AsyncLoad(std::int32_t prio = 0) {
        AssetRequest r;
        r.sync = SyncWith::Async;
        r.priority = prio;
        return r;
    }

    // type hint を付けたい場合（例：Texture を期待）
    static AssetRequest WithTypeHint(AssetType type) {
        AssetRequest r;
        r.expectedType = type;
        r.useTypeHint = true;
        return r;
    }

    // path を直接指定したい場合（Catalogを使わない/上書き）
    static AssetRequest WithOverridePath(std::string path) {
        AssetRequest r;
        r.overridePath = std::move(path);
        return r;
    }

    // ---- helpers ----
    bool IsReload() const noexcept { return mode == Mode::ForceReload; }
    bool IsAsync() const noexcept { return sync == SyncWith::Async; }
    bool HasOverridePath() const noexcept { return !overridePath.empty(); }
};

} // namespace Engine::Asset
