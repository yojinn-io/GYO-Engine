// The yyjson half of the adapter: Parse, Document and the views over yyjson's
// immutable DOM (Design Invariant J-1: yyjson appears only in this directory).
// Numbers are read as raw text (YYJSON_READ_NUMBER_AS_RAW) and converted by
// the Module itself (J-3).
#include <cstddef>
#include <cstring>
#include <new>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#include <yyjson.h>

#include "ParseWith.hpp"
#include "engine/base/Assert.hpp"
#include "engine/json/Document.hpp"
#include "../../NumberText.hpp"
#include "../../TextPosition.hpp"
#include "../fast_float/DecimalToDouble.hpp"

namespace GYO::Engine::Json {

namespace Detail {

// A Document's tree is yyjson's document itself: Tree is never defined as an
// object, only used as the opaque pointer type (like Node).
struct Tree final {
    static Document Adopt(yyjson_doc* doc) noexcept { return Document(reinterpret_cast<Tree*>(doc)); }
    static yyjson_doc* Doc(Tree* tree) noexcept { return reinterpret_cast<yyjson_doc*>(tree); }
};

} // namespace Detail

namespace {

using ::Engine::Base::Err;

yyjson_val* Raw(const Detail::Node* node) noexcept {
    return reinterpret_cast<yyjson_val*>(const_cast<Detail::Node*>(node));
}

const Detail::Node* Wrap(yyjson_val* value) noexcept { return reinterpret_cast<const Detail::Node*>(value); }

Kind KindOf(yyjson_val* value) noexcept {
    switch (yyjson_get_type(value)) {
    case YYJSON_TYPE_NULL: return Kind::Null;
    case YYJSON_TYPE_BOOL: return Kind::Bool;
    case YYJSON_TYPE_RAW:
    case YYJSON_TYPE_NUM: return Kind::Number;
    case YYJSON_TYPE_STR: return Kind::String;
    case YYJSON_TYPE_ARR: return Kind::Array;
    case YYJSON_TYPE_OBJ: return Kind::Object;
    default: break;
    }
    GYO_ASSERT(false && "yyjson value of an unknown type");
    return Kind::Null;
}

JsonError WrongKindError(const Kind expected, const Kind found) {
    return JsonError::Make(JsonErrorCode::WrongKind, "wrong kind",
                           std::string("expected ") + ToString(expected) + ", found " + ToString(found));
}

JsonError OutOfMemoryError() { return JsonError::Make(JsonErrorCode::OutOfMemory, "out of memory while parsing"); }

std::string_view StringOf(yyjson_val* value) noexcept { return {yyjson_get_str(value), yyjson_get_len(value)}; }

std::string_view RawOf(yyjson_val* value) noexcept { return {yyjson_get_raw(value), yyjson_get_len(value)}; }

// The strict checks yyjson leaves to the adapter, in one walk without
// recursion (J-2, J-9): a key repeated in one object, and a number that does
// not convert to a finite double or whose non-zero text rounds to zero.
class Validation final {
public:
    Validation(yyjson_doc* doc, const std::string_view text) : base_(doc->str_pool), text_(text) {}

    std::optional<JsonError> Run(yyjson_val* root) {
        if (auto error = Visit(root)) return error;
        while (!stack_.empty()) {
            Frame& frame = stack_.back();
            if (frame.remaining == 0) {
                stack_.pop_back();
                continue;
            }
            yyjson_val* child = nullptr;
            if (frame.object) {
                frame.childKey = frame.next;
                child = frame.next + 1;
            } else {
                frame.childIndex = frame.index;
                child = frame.next;
            }
            frame.next = unsafe_yyjson_get_next(child);
            ++frame.index;
            --frame.remaining;
            if (auto error = Visit(child)) return error; // may push: frame is not used after this
        }
        return std::nullopt;
    }

private:
    struct Frame {
        yyjson_val* next;
        std::size_t remaining;
        std::size_t index;
        bool object;
        yyjson_val* childKey;
        std::size_t childIndex;
    };

    std::optional<JsonError> Visit(yyjson_val* value) {
        if (yyjson_is_obj(value)) {
            if (auto error = CheckRepeatedKeys(value)) return error;
            stack_.push_back({unsafe_yyjson_get_first(value), yyjson_obj_size(value), 0, true, nullptr, 0});
        } else if (yyjson_is_arr(value)) {
            stack_.push_back({unsafe_yyjson_get_first(value), yyjson_arr_size(value), 0, false, nullptr, 0});
        } else if (yyjson_is_raw(value)) {
            double converted = 0.0;
            if (Detail::DecimalToDouble(RawOf(value), converted) != Detail::DecimalRead::Ok) {
                return JsonError::Make(JsonErrorCode::NumberOutOfRange,
                                       "number out of range: " + std::string(RawOf(value)),
                                       Located(yyjson_get_raw(value), PointerToCurrent()));
            }
        }
        return std::nullopt;
    }

    std::optional<JsonError> CheckRepeatedKeys(yyjson_val* object) {
        constexpr std::size_t kPairwiseLimit = 16;
        const std::size_t size = yyjson_obj_size(object);
        yyjson_val* key = unsafe_yyjson_get_first(object);
        if (size <= kPairwiseLimit) {
            std::string_view seen[kPairwiseLimit];
            for (std::size_t i = 0; i < size; ++i, key = unsafe_yyjson_get_next(key + 1)) {
                const std::string_view name = StringOf(key);
                for (std::size_t j = 0; j < i; ++j) {
                    if (seen[j] == name) return RepeatedKey(key);
                }
                seen[i] = name;
            }
            return std::nullopt;
        }
        std::unordered_set<std::string_view> seen;
        seen.reserve(size);
        for (std::size_t i = 0; i < size; ++i, key = unsafe_yyjson_get_next(key + 1)) {
            if (!seen.insert(StringOf(key)).second) return RepeatedKey(key);
        }
        return std::nullopt;
    }

    // The pointer of the value being visited: one token per open container.
    std::string PointerToCurrent() const {
        std::string pointer;
        for (const Frame& frame : stack_) {
            if (frame.object) {
                Detail::AppendPointerToken(pointer, StringOf(frame.childKey));
            } else {
                Detail::AppendPointerToken(pointer, std::to_string(frame.childIndex));
            }
        }
        return pointer;
    }

    // Where a key or a raw number starts in the parsed text: yyjson's copy of
    // the input begins at str_pool and keeps every string's first byte at its
    // offset in the input (unescaping only shortens a string in place).
    // "line L, column C; <pointer>", leaving out what is unknown or empty
    // (the root's pointer is empty).
    std::string Located(const char* inCopy, const std::string& pointer) const {
        std::string detail;
        if (base_ != nullptr) detail = Detail::LineAndColumn(text_, static_cast<std::size_t>(inCopy - base_));
        if (!pointer.empty()) detail += (detail.empty() ? "" : "; ") + pointer;
        return detail;
    }

    JsonError RepeatedKey(yyjson_val* key) const {
        std::string pointer = PointerToCurrent();
        Detail::AppendPointerToken(pointer, StringOf(key));
        return JsonError::Make(JsonErrorCode::RepeatedKey,
                               "repeated key '" + std::string(StringOf(key)) + "'",
                               Located(yyjson_get_str(key), pointer));
    }

    const char* base_;
    std::string_view text_;
    std::vector<Frame> stack_;
};

} // namespace

// ---- Document ----

Document::Document(Document&& other) noexcept : tree_(std::exchange(other.tree_, nullptr)) {}

Document& Document::operator=(Document&& other) noexcept {
    if (this != &other) {
        yyjson_doc_free(Detail::Tree::Doc(tree_));
        tree_ = std::exchange(other.tree_, nullptr);
    }
    return *this;
}

Document::~Document() { yyjson_doc_free(Detail::Tree::Doc(tree_)); }

Value Document::Root() const {
    GYO_ASSERT(tree_ != nullptr);
    return Value(Wrap(yyjson_doc_get_root(Detail::Tree::Doc(tree_))));
}

namespace Detail {

JsonResult<Document> ParseWith(const std::string_view text, const yyjson_alc* allocator) {
    if (text.empty()) {
        return Err(JsonError::Make(JsonErrorCode::Syntax, "not JSON: empty text", LineAndColumn(text, 0)));
    }
    try {
        yyjson_read_err error{};
        yyjson_doc* doc = yyjson_read_opts(const_cast<char*>(text.data()), text.size(), YYJSON_READ_NUMBER_AS_RAW,
                                           allocator, &error);
        if (doc == nullptr) {
            if (error.code == YYJSON_READ_ERROR_MEMORY_ALLOCATION) return Err(OutOfMemoryError());
            return Err(JsonError::Make(JsonErrorCode::Syntax, std::string("not JSON: ") + error.msg,
                                       LineAndColumn(text, error.pos)));
        }
        Document document = Tree::Adopt(doc);
        Validation validation(doc, text);
        if (auto failure = validation.Run(yyjson_doc_get_root(doc))) return Err(std::move(*failure));
        return document;
    } catch (const std::bad_alloc&) {
        return Err(OutOfMemoryError());
    }
}

} // namespace Detail

JsonResult<Document> Parse(const std::string_view text) { return Detail::ParseWith(text, nullptr); }

// ---- Value ----

Kind Value::GetKind() const noexcept { return KindOf(Raw(node_)); }

bool Value::IsIntegerLiteral() const noexcept {
    yyjson_val* value = Raw(node_);
    return yyjson_is_raw(value) && Detail::IsIntegerLiteral(RawOf(value));
}

JsonResult<bool> Value::AsBool() const {
    yyjson_val* value = Raw(node_);
    if (!yyjson_is_bool(value)) return Err(WrongKindError(Kind::Bool, KindOf(value)));
    return yyjson_get_bool(value);
}

namespace {

// The integer literal of a number value, or the error for anything else.
JsonResult<std::string_view> IntegerText(yyjson_val* value) {
    if (!yyjson_is_raw(value)) return Err(WrongKindError(Kind::Number, KindOf(value)));
    const std::string_view text = RawOf(value);
    if (!Detail::IsIntegerLiteral(text)) {
        return Err(JsonError::Make(JsonErrorCode::WrongKind, "wrong kind",
                                   "expected an integer literal, found " + std::string(text)));
    }
    return text;
}

JsonError IntegerOutOfRange(const std::string_view text, const char* type) {
    return JsonError::Make(JsonErrorCode::NumberOutOfRange, "integer out of range",
                           std::string(text) + " does not fit " + type);
}

} // namespace

JsonResult<std::int64_t> Value::AsInt64() const {
    auto text = IntegerText(Raw(node_));
    if (!text) return Err(std::move(text).error());
    std::int64_t out = 0;
    if (Detail::ReadInt64(*text, out) != Detail::IntegerRead::Ok) return Err(IntegerOutOfRange(*text, "int64"));
    return out;
}

JsonResult<std::uint64_t> Value::AsUint64() const {
    auto text = IntegerText(Raw(node_));
    if (!text) return Err(std::move(text).error());
    std::uint64_t out = 0;
    if (Detail::ReadUint64(*text, out) != Detail::IntegerRead::Ok) return Err(IntegerOutOfRange(*text, "uint64"));
    return out;
}

JsonResult<double> Value::AsDouble() const {
    yyjson_val* value = Raw(node_);
    if (!yyjson_is_raw(value)) return Err(WrongKindError(Kind::Number, KindOf(value)));
    double out = 0.0;
    if (Detail::DecimalToDouble(RawOf(value), out) != Detail::DecimalRead::Ok) {
        // Parse has checked every number already; kept for a Document built otherwise.
        return Err(JsonError::Make(JsonErrorCode::NumberOutOfRange, "number out of range: " + std::string(RawOf(value))));
    }
    return out;
}

JsonResult<std::string_view> Value::AsString() const {
    yyjson_val* value = Raw(node_);
    if (!yyjson_is_str(value)) return Err(WrongKindError(Kind::String, KindOf(value)));
    return StringOf(value);
}

JsonResult<std::string_view> Value::AsNumberText() const {
    yyjson_val* value = Raw(node_);
    if (!yyjson_is_raw(value)) return Err(WrongKindError(Kind::Number, KindOf(value)));
    return RawOf(value);
}

JsonResult<ObjectView> Value::AsObject() const {
    yyjson_val* value = Raw(node_);
    if (!yyjson_is_obj(value)) return Err(WrongKindError(Kind::Object, KindOf(value)));
    return ObjectView(node_);
}

JsonResult<ArrayView> Value::AsArray() const {
    yyjson_val* value = Raw(node_);
    if (!yyjson_is_arr(value)) return Err(WrongKindError(Kind::Array, KindOf(value)));
    return ArrayView(node_);
}

// ---- ObjectView ----

std::size_t ObjectView::Size() const noexcept { return yyjson_obj_size(Raw(node_)); }

ObjectView::Iterator ObjectView::begin() const {
    yyjson_val* object = Raw(node_);
    const std::size_t size = yyjson_obj_size(object);
    if (size == 0) return {};
    return Iterator(Wrap(unsafe_yyjson_get_first(object)), size);
}

Member ObjectView::Iterator::operator*() const {
    GYO_ASSERT(remaining_ != 0);
    yyjson_val* key = Raw(key_);
    return Member{StringOf(key), Value(Wrap(key + 1))};
}

ObjectView::Iterator& ObjectView::Iterator::operator++() {
    GYO_ASSERT(remaining_ != 0);
    --remaining_;
    key_ = remaining_ == 0 ? nullptr : Wrap(unsafe_yyjson_get_next(Raw(key_) + 1));
    return *this;
}

std::optional<Value> ObjectView::Find(const std::string_view key) const {
    yyjson_val* found = yyjson_obj_getn(Raw(node_), key.data(), key.size());
    if (found == nullptr) return std::nullopt;
    return Value(Wrap(found));
}

// ---- ArrayView ----

std::size_t ArrayView::Size() const noexcept { return yyjson_arr_size(Raw(node_)); }

ArrayView::Iterator ArrayView::begin() const {
    yyjson_val* array = Raw(node_);
    const std::size_t size = yyjson_arr_size(array);
    if (size == 0) return {};
    return Iterator(Wrap(unsafe_yyjson_get_first(array)), size);
}

Value ArrayView::Iterator::operator*() const {
    GYO_ASSERT(remaining_ != 0);
    return Value(element_);
}

ArrayView::Iterator& ArrayView::Iterator::operator++() {
    GYO_ASSERT(remaining_ != 0);
    --remaining_;
    element_ = remaining_ == 0 ? nullptr : Wrap(unsafe_yyjson_get_next(Raw(element_)));
    return *this;
}

} // namespace GYO::Engine::Json
