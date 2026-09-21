#pragma once
#include <string>
#include <type_traits>
#include <utility>


// Error：Error に保持する失敗理由
// - code：機械的に扱える分類
// - message：人間向け（ログ/デバッグ）
// - detail：任意補助情報（path/typeなどを埋める用途）
namespace Engine::Base {

    template <class T>
    class Error final {
        static_assert(std::is_enum_v<T>, "Error<T>: T must be an enum type.");

    public:
        using Code = T;
        using Under = std::underlying_type_t<T>;

        // 不定値を防ぐ
        T code{};
        std::string message;
        std::string detail;

        bool ok() const noexcept {
            return static_cast<Under>(code) == 0;
        }
        explicit operator bool() const noexcept { // true = has error
            return !ok();
        }

        static Error None() noexcept { return Error{}; }

        static Error Make(T c, std::string msg, std::string det = {}) {
            Error e;
            e.code = c;
            e.message = std::move(msg);
            e.detail = std::move(det);
            return e;
        }

        void Clear() noexcept {
            code = T{};
            message.clear();
            detail.clear();
        }

        // getter
        T CodeValue() const noexcept { return code; }
        const std::string& Message() const noexcept { return message; }
        const std::string& Detail() const noexcept { return detail; }
    };

} // namespace Engine::Base
