#pragma once

#include <utility>
#include <variant>

namespace Engine::Base {

    // C++17 で使える軽量 Result（std::expected の代替）
    // Result<T, E>：成功なら T、失敗なら E を保持する。
    template <class T, class E>
    class Result final {
    public:
        using value_type = T;
        using error_type = E;

        static Result Ok(T v) { return Result(std::in_place_index<0>, std::move(v)); }
        static Result Err(E e) { return Result(std::in_place_index<1>, std::move(e)); }

        bool ok() const noexcept { return data_.index() == 0; }
        explicit operator bool() const noexcept { return ok(); }

        T& value() & { return std::get<0>(data_); }
        const T& value() const& { return std::get<0>(data_); }
        T&& value() && { return std::move(std::get<0>(data_)); }

        E& error() & { return std::get<1>(data_); }
        const E& error() const& { return std::get<1>(data_); }
        E&& error() && { return std::move(std::get<1>(data_)); }

    private:
        template <class... Args>
        explicit Result(std::in_place_index_t<0>, Args&&... args)
            : data_(std::in_place_index<0>, std::forward<Args>(args)...) {}

        template <class... Args>
        explicit Result(std::in_place_index_t<1>, Args&&... args)
            : data_(std::in_place_index<1>, std::forward<Args>(args)...) {}

        std::variant<T, E> data_;
    };

    // void 成功版：Result<void, E>
    template <class E>
    class Result<void, E> final {
    public:
        using value_type = void;
        using error_type = E;

        static Result Ok() { return Result(std::in_place_index<0>); }
        static Result Err(E e) { return Result(std::in_place_index<1>, std::move(e)); }

        bool ok() const noexcept { return data_.index() == 0; }
        explicit operator bool() const noexcept { return ok(); }

        // value() は何もしない（API互換のため）
        void value() const noexcept {}

        E& error() & { return std::get<1>(data_); }
        const E& error() const& { return std::get<1>(data_); }
        E&& error() && { return std::move(std::get<1>(data_)); }

    private:
        explicit Result(std::in_place_index_t<0>) : data_(std::in_place_index<0>, std::monostate{}) {}

        template <class... Args>
        explicit Result(std::in_place_index_t<1>, Args&&... args)
            : data_(std::in_place_index<1>, std::forward<Args>(args)...) {}

        std::variant<std::monostate, E> data_;
    };

} // namespace Engine::Asset::Detail
