#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

#include "engine/json/JsonError.hpp"

// Reading JSON: Parse turns a text into a Document; Value, ObjectView and
// ArrayView are views into it. See Design Invariants J-2, J-3, J-8 and J-9 of
// docs/architecture/target/v2/json-module-declaration.zh-Hant.md.
//
//     auto document = Json::Parse(text);
//     if (!document) return ...;             // Syntax, RepeatedKey, NumberOutOfRange, OutOfMemory
//     auto root = document->Root().AsObject();
//     if (!root) return ...;                 // WrongKind: the content is not an object
//     for (const Json::Member& member : *root) { ... member.key, member.value ... }
//
// Views stay valid while their Document lives, also after the Document is
// moved; they do not refer to the parsed text, which may go away after Parse.

namespace GYO::Engine::Json {

enum class Kind : std::uint8_t { Null = 1, Bool, Number, String, Array, Object };

[[nodiscard]] const char* ToString(Kind kind) noexcept;

namespace Detail {
// A node of the parsed tree. Opaque: only the adapter knows what it is.
struct Node;
struct Tree;
} // namespace Detail

class ObjectView;
class ArrayView;

class Value final {
public:
    [[nodiscard]] Kind GetKind() const noexcept;
    // A number written without '.', 'e' or 'E'. False for anything else.
    [[nodiscard]] bool IsIntegerLiteral() const noexcept;

    [[nodiscard]] JsonResult<bool> AsBool() const;
    // Integer literals only (WrongKind for "1.0" or "1e2"); NumberOutOfRange
    // when the value does not fit. "-0" reads as 0.
    [[nodiscard]] JsonResult<std::int64_t> AsInt64() const;
    [[nodiscard]] JsonResult<std::uint64_t> AsUint64() const;
    // Any number, correctly rounded; "-0" reads as -0.0.
    [[nodiscard]] JsonResult<double> AsDouble() const;
    // The unescaped UTF-8 bytes; may contain U+0000.
    [[nodiscard]] JsonResult<std::string_view> AsString() const;
    // The number's text exactly as written.
    [[nodiscard]] JsonResult<std::string_view> AsNumberText() const;
    [[nodiscard]] JsonResult<ObjectView> AsObject() const;
    [[nodiscard]] JsonResult<ArrayView> AsArray() const;

private:
    friend class Document;
    friend class ObjectView;
    friend class ArrayView;
    explicit Value(const Detail::Node* node) noexcept : node_(node) {}

    const Detail::Node* node_;
};

struct Member final {
    std::string_view key;
    Value value;
};

// The members of an object in document order. Walking is constant time per
// member; there is no index access (J-9).
class ObjectView final {
public:
    class Iterator final {
    public:
        Iterator() noexcept = default;
        [[nodiscard]] Member operator*() const;
        Iterator& operator++();
        [[nodiscard]] bool operator==(const Iterator&) const noexcept = default;

    private:
        friend class ObjectView;
        Iterator(const Detail::Node* key, std::size_t remaining) noexcept : key_(key), remaining_(remaining) {}
        const Detail::Node* key_ = nullptr;
        std::size_t remaining_ = 0;
    };

    [[nodiscard]] std::size_t Size() const noexcept;
    [[nodiscard]] Iterator begin() const;
    [[nodiscard]] Iterator end() const noexcept { return {}; }
    // The member with this key, if any: a linear search (keys are unique, J-2).
    [[nodiscard]] std::optional<Value> Find(std::string_view key) const;

private:
    friend class Value;
    explicit ObjectView(const Detail::Node* node) noexcept : node_(node) {}

    const Detail::Node* node_;
};

// The elements of an array in document order, constant time per element; no
// index access (J-9).
class ArrayView final {
public:
    class Iterator final {
    public:
        Iterator() noexcept = default;
        [[nodiscard]] Value operator*() const;
        Iterator& operator++();
        [[nodiscard]] bool operator==(const Iterator&) const noexcept = default;

    private:
        friend class ArrayView;
        Iterator(const Detail::Node* element, std::size_t remaining) noexcept
            : element_(element), remaining_(remaining) {}
        const Detail::Node* element_ = nullptr;
        std::size_t remaining_ = 0;
    };

    [[nodiscard]] std::size_t Size() const noexcept;
    [[nodiscard]] Iterator begin() const;
    [[nodiscard]] Iterator end() const noexcept { return {}; }

private:
    friend class Value;
    explicit ArrayView(const Detail::Node* node) noexcept : node_(node) {}

    const Detail::Node* node_;
};

class Document final {
public:
    Document(Document&& other) noexcept;
    Document& operator=(Document&& other) noexcept;
    Document(const Document&) = delete;
    Document& operator=(const Document&) = delete;
    ~Document();

    // The top-level value. Calling it on a moved-from Document is a
    // Programmer Error.
    [[nodiscard]] Value Root() const;

private:
    friend struct Detail::Tree;
    explicit Document(Detail::Tree* tree) noexcept : tree_(tree) {}

    Detail::Tree* tree_;
};

// Strict RFC 8259 parsing (J-2): UTF-8 without a byte order mark, no comments,
// no trailing content, no NaN or Infinity, not empty; a key repeated in one
// object is RepeatedKey; every number must convert to a finite double, and
// non-zero text must not round to zero (NumberOutOfRange). Error details give
// "line L, column C" (1-based, columns in bytes) and, for RepeatedKey and
// NumberOutOfRange, the JSON Pointer of the value.
[[nodiscard]] JsonResult<Document> Parse(std::string_view text);

} // namespace GYO::Engine::Json
