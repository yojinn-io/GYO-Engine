#pragma once

#include <cstdint>
#include <functional>
#include <utility>

#include "engine/asset/AssetId.hpp"
#include "engine/asset/detail/TypeId.hpp"

namespace Engine::Asset {

// AssetHandle：
// - ゲーム側や上位層に渡す「トークン（ID + 世代 + 型）」
// - 実体（shared_ptr等）を直接持たない（= エンジン内部のキャッシュに依存しない）
// - AssetManager がこのハンドルを受け取って AssetStorage/Record を参照する
class AssetHandle final {
public:
    AssetHandle() = default;

    // 無効ハンドル（generation==0 を無効扱いにする：AssetId の仕様に依存しない）
    static AssetHandle Invalid() noexcept { return AssetHandle{}; }

    // 型を指定しない（デフォルト）
    static AssetHandle Make(AssetId id, std::uint32_t generation) noexcept {
        AssetHandle h;
        h.id_ = std::move(id);
        h.generation_ = generation;
        h.type_ = Detail::TypeId{}; // unknown
        return h;
    }

    // 型を指定する（デバッグ/安全性用）
    template <class T>
    static AssetHandle MakeTyped(AssetId id, std::uint32_t generation) noexcept {
        AssetHandle h;
        h.id_ = std::move(id);
        h.generation_ = generation;
        h.type_ = Detail::TypeId::Of<T>();
        return h;
    }

    bool valid() const noexcept {
        return generation_ != 0;
    }
    explicit operator bool() const noexcept { return valid(); }

    const AssetId& id() const noexcept { return id_; }
    std::uint32_t generation() const noexcept { return generation_; }

    // 型ヒント：未指定なら invalid(TypeId{}) になる
    Detail::TypeId type_hint() const noexcept { return type_; }
    bool has_type_hint() const noexcept { return type_.valid(); }

    template <class T>
    bool type_is() const noexcept {
        return has_type_hint() && (type_ == Detail::TypeId::Of<T>());
    }

    // 便利：中身を空にする
    void reset() noexcept {
        *this = AssetHandle{};
    }

    // 比較：同一トークン（同じidでも generation が違えば別物）
    friend bool operator==(const AssetHandle& a, const AssetHandle& b) noexcept {
        return a.generation_ == b.generation_ && a.id_ == b.id_;
    }
    friend bool operator!=(const AssetHandle& a, const AssetHandle& b) noexcept {
        return !(a == b);
    }

private:
    AssetId id_{};
    std::uint32_t generation_ = 0;
    Detail::TypeId type_{};
};

} // namespace Engine::Asset

namespace std {
template <>
struct hash<Engine::Asset::AssetHandle> {
    size_t operator()(const Engine::Asset::AssetHandle& h) const noexcept {
        size_t a = std::hash<Engine::Asset::AssetId>{}(h.id());
        size_t b = std::hash<std::uint32_t>{}(h.generation());
        // combine
        return a ^ (b + 0x9e3779b97f4a7c15ull + (a << 6) + (a >> 2));
    }
};
} // namespace std
