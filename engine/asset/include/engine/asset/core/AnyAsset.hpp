#pragma once

#include <memory>
#include <utility>

#include "engine/asset/detail/TypeId.hpp"

namespace Engine::Asset::Core {

    // AnyAsset:
    // - 実体は shared_ptr<void>（所有権/寿命は shared_ptr に委譲）
    // - 型識別は Detail::TypeId（RTTIに依存しない）
    // - Loader が生成した「任意型の資産」を AssetRecord/Storage に格納するための器
    class AnyAsset final {
    public:
        AnyAsset() = default;

        bool empty() const noexcept { return !ptr_; }
        explicit operator bool() const noexcept { return !empty(); }

        Detail::TypeId type() const noexcept { return type_; }

        // T の shared_ptr をそのまま包む（推奨）
        template <class T>
        static AnyAsset FromShared(std::shared_ptr<T> p) noexcept {
            AnyAsset a;
            a.type_ = Detail::TypeId::Of<T>();
            a.ptr_  = std::move(p);
            return a;
        }

        // ムーブ用（unique_ptr を shared_ptr にする等）
        template <class T, class... Args>
        static AnyAsset MakeShared(Args&&... args) {
            return FromShared<T>(std::make_shared<T>(std::forward<Args>(args)...));
        }

        template <class T>
        bool Is() const noexcept {
            return ptr_ && (type_ == Detail::TypeId::Of<T>());
        }

        template <class T>
        T* As() noexcept {
            return Is<T>() ? static_cast<T*>(ptr_.get()) : nullptr;
        }

        template <class T>
        const T* As() const noexcept {
            return Is<T>() ? static_cast<const T*>(ptr_.get()) : nullptr;
        }

        // shared_ptr<T> として取り出す（型が違うなら空）
        template <class T>
        std::shared_ptr<T> ShareAs() const noexcept {
            if (!Is<T>()) return {};
            return std::static_pointer_cast<T>(ptr_);
        }

        void Reset() noexcept {
            ptr_.reset();
            type_ = Detail::TypeId{};
        }

    private:
        Detail::TypeId      type_{};
        std::shared_ptr<void> ptr_{};
    };

} // namespace Engine::Asset::Core
