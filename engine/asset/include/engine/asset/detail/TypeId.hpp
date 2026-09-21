#pragma once

#include <cstdint>
#include <cstddef>
#include <functional>

namespace Engine::Asset::Detail {

    // TypeId：型を一意に識別するための “不透明ID”
    // 目的：AnyAsset / AssetRecord など type-erasure で「元の型は何か」を照合する。
    // 方針：
    // - RTTI 文字列に依存しない（compiler差を避ける）
    // - 実行中に一意なら十分（シリアライズ用途には使わない）
    // 実装：型ごとの静的トークンのアドレスを ID として使う
    class TypeId final {
    public:
        using ValueType = std::uintptr_t;

        constexpr TypeId() noexcept : value_(0) {}
        constexpr explicit TypeId(ValueType v) noexcept : value_(v) {}

        constexpr bool valid() const noexcept { return value_ != 0; }
        constexpr ValueType value() const noexcept { return value_; }

        template <class T>
        static TypeId Of() noexcept {
            return TypeId(reinterpret_cast<ValueType>(Token<T>()));
        }

        friend constexpr bool operator==(TypeId a, TypeId b) noexcept { return a.value_ == b.value_; }
        friend constexpr bool operator!=(TypeId a, TypeId b) noexcept { return !(a == b); }
        friend constexpr bool operator<(TypeId a, TypeId b) noexcept { return a.value_ < b.value_; }

    private:
        template <class T>
        static const void* Token() noexcept {
            static const int kToken = 0;
            return &kToken;
        }

        ValueType value_;
    };

} // namespace Engine::Asset::Detail

namespace std {
    template <>
    struct hash<Engine::Asset::Detail::TypeId> {
        size_t operator()(const Engine::Asset::Detail::TypeId& t) const noexcept {
            return static_cast<size_t>(t.value());
        }
    };
} // namespace std
