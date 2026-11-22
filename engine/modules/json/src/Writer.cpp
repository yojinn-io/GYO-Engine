// The Writer's structure, escaping and UTF-8 check: the Module's own code, no
// external component (the number text comes from adapters/yyjson/WriteDouble).
#include "engine/json/Writer.hpp"

#include <charconv>
#include <cmath>
#include <new>
#include <string>
#include <utility>
#include <vector>

#include "engine/base/Assert.hpp"
#include "adapters/yyjson/WriteDouble.hpp"

namespace GYO::Engine::Json {

namespace {

using ::GYO::Engine::Base::Err;

// The length of the valid UTF-8 sequence starting at text[i] (RFC 3629: no
// overlong forms, no surrogates, nothing above U+10FFFF), or 0. The same set
// Parse accepts (Design Invariant J-5).
std::size_t SequenceLength(const std::string_view text, const std::size_t i) noexcept {
    const auto byte = [&](const std::size_t at) { return static_cast<unsigned char>(text[at]); };
    const auto continuation = [&](const std::size_t at, const unsigned char low, const unsigned char high) {
        return at < text.size() && byte(at) >= low && byte(at) <= high;
    };
    const unsigned char lead = byte(i);
    if (lead < 0x80) return 1;
    if (lead >= 0xC2 && lead <= 0xDF) return continuation(i + 1, 0x80, 0xBF) ? 2 : 0;
    if (lead == 0xE0) return continuation(i + 1, 0xA0, 0xBF) && continuation(i + 2, 0x80, 0xBF) ? 3 : 0;
    if ((lead >= 0xE1 && lead <= 0xEC) || lead == 0xEE || lead == 0xEF) {
        return continuation(i + 1, 0x80, 0xBF) && continuation(i + 2, 0x80, 0xBF) ? 3 : 0;
    }
    if (lead == 0xED) return continuation(i + 1, 0x80, 0x9F) && continuation(i + 2, 0x80, 0xBF) ? 3 : 0;
    if (lead == 0xF0) {
        return continuation(i + 1, 0x90, 0xBF) && continuation(i + 2, 0x80, 0xBF) && continuation(i + 3, 0x80, 0xBF)
                   ? 4
                   : 0;
    }
    if (lead >= 0xF1 && lead <= 0xF3) {
        return continuation(i + 1, 0x80, 0xBF) && continuation(i + 2, 0x80, 0xBF) && continuation(i + 3, 0x80, 0xBF)
                   ? 4
                   : 0;
    }
    if (lead == 0xF4) {
        return continuation(i + 1, 0x80, 0x8F) && continuation(i + 2, 0x80, 0xBF) && continuation(i + 3, 0x80, 0xBF)
                   ? 4
                   : 0;
    }
    return 0;
}

// Appends text as a JSON string (with quotes). False, with nothing promised
// about what was appended, when text is not valid UTF-8.
bool AppendString(std::string& out, const std::string_view text) {
    static constexpr char kHex[] = "0123456789abcdef";
    out += '"';
    std::size_t runStart = 0;
    std::size_t i = 0;
    while (i < text.size()) {
        const auto c = static_cast<unsigned char>(text[i]);
        const char* shortEscape = nullptr;
        switch (c) {
        case '"': shortEscape = "\\\""; break;
        case '\\': shortEscape = "\\\\"; break;
        case '\b': shortEscape = "\\b"; break;
        case '\f': shortEscape = "\\f"; break;
        case '\n': shortEscape = "\\n"; break;
        case '\r': shortEscape = "\\r"; break;
        case '\t': shortEscape = "\\t"; break;
        default: break;
        }
        if (shortEscape != nullptr || c < 0x20) {
            out.append(text.data() + runStart, i - runStart);
            if (shortEscape != nullptr) {
                out += shortEscape;
            } else {
                const char escape[] = {'\\', 'u', '0', '0', kHex[c >> 4], kHex[c & 0xF]};
                out.append(escape, sizeof escape);
            }
            ++i;
            runStart = i;
            continue;
        }
        const std::size_t length = SequenceLength(text, i);
        if (length == 0) return false;
        i += length;
    }
    out.append(text.data() + runStart, text.size() - runStart);
    out += '"';
    return true;
}

} // namespace

Writer::Writer(std::string& out) noexcept : out_(out), startLength_(out.size()) {}

Writer::Frame& Writer::Top() {
    GYO_ASSERT(depth_ > 0);
    return depth_ <= kInlineDepth ? inline_[depth_ - 1] : deeper_.back();
}

void Writer::Push(const Frame frame) {
    if (lost_ > 0) {
        ++lost_;
        return;
    }
    if (depth_ < kInlineDepth) {
        inline_[depth_] = frame;
    } else {
        try {
            deeper_.push_back(frame);
        } catch (const std::bad_alloc&) {
            ++lost_;  // the level exists for the caller; its state is gone
            throw;
        }
    }
    ++depth_;
}

void Writer::Pop() {
    if (depth_ > kInlineDepth) deeper_.pop_back();
    --depth_;
}

void Writer::Fail(const JsonErrorCode code, std::string message) {
    if (!error_) error_ = JsonError::Make(code, std::move(message), "value " + std::to_string(values_));
}

void Writer::BeforeValue() {
    GYO_ASSERT(!finished_ && "the Writer is finished");
    ++values_;
    if (lost_ > 0) return;  // output is discarded at Finish anyway
    if (depth_ == 0) {
        GYO_ASSERT(!topWritten_ && "a second top-level value");
        topWritten_ = true;
        return;
    }
    Frame& frame = Top();
    if (frame.container == Container::Object) {
        GYO_ASSERT(frame.awaitingValue && "a value in an object needs a Key first");
        frame.awaitingValue = false;
        return;
    }
    if (!frame.first) out_ += ',';
    frame.first = false;
}

void Writer::BeginContainer(const Container container, const char open) {
    try {
        BeforeValue();
        Push({container, true, false});
        out_ += open;
    } catch (const std::bad_alloc&) {
        Fail(JsonErrorCode::OutOfMemory, "out of memory");
    }
}

void Writer::EndContainer(const Container container, const char close) {
    GYO_ASSERT(!finished_ && "the Writer is finished");
    if (lost_ > 0) {
        --lost_;
        return;
    }
    GYO_ASSERT(depth_ > 0 && "an end without its begin");
    const Frame& frame = Top();
    GYO_ASSERT(frame.container == container && "the end does not match the open container");
    GYO_ASSERT(!frame.awaitingValue && "a Key without its value");
    Pop();
    try {
        out_ += close;
    } catch (const std::bad_alloc&) {
        Fail(JsonErrorCode::OutOfMemory, "out of memory");
    }
}

void Writer::BeginObject() { BeginContainer(Container::Object, '{'); }
void Writer::EndObject() { EndContainer(Container::Object, '}'); }
void Writer::BeginArray() { BeginContainer(Container::Array, '['); }
void Writer::EndArray() { EndContainer(Container::Array, ']'); }

void Writer::Key(const std::string_view key) {
    GYO_ASSERT(!finished_ && "the Writer is finished");
    if (lost_ > 0) {
        ++values_;
        return;
    }
    GYO_ASSERT(depth_ > 0 && "a Key outside an object");
    Frame& frame = Top();
    GYO_ASSERT(frame.container == Container::Object && "a Key outside an object");
    GYO_ASSERT(!frame.awaitingValue && "two Keys in a row");
    ++values_;
    frame.awaitingValue = true;
    try {
        if (!frame.first) out_ += ',';
        frame.first = false;
        if (!AppendString(out_, key)) {
            Fail(JsonErrorCode::NotRepresentable, "invalid UTF-8");
            return;
        }
        out_ += ':';
    } catch (const std::bad_alloc&) {
        Fail(JsonErrorCode::OutOfMemory, "out of memory");
    }
}

void Writer::String(const std::string_view text) {
    try {
        BeforeValue();
        if (!AppendString(out_, text)) Fail(JsonErrorCode::NotRepresentable, "invalid UTF-8");
    } catch (const std::bad_alloc&) {
        Fail(JsonErrorCode::OutOfMemory, "out of memory");
    }
}

void Writer::Int(const std::int64_t value) {
    try {
        BeforeValue();
        char buffer[24];
        const auto result = std::to_chars(buffer, buffer + sizeof buffer, value);
        out_.append(buffer, result.ptr);
    } catch (const std::bad_alloc&) {
        Fail(JsonErrorCode::OutOfMemory, "out of memory");
    }
}

void Writer::Uint(const std::uint64_t value) {
    try {
        BeforeValue();
        char buffer[24];
        const auto result = std::to_chars(buffer, buffer + sizeof buffer, value);
        out_.append(buffer, result.ptr);
    } catch (const std::bad_alloc&) {
        Fail(JsonErrorCode::OutOfMemory, "out of memory");
    }
}

void Writer::Double(const double value) {
    try {
        BeforeValue();
        if (!std::isfinite(value)) {
            Fail(JsonErrorCode::NotRepresentable, "non-finite number");
            return;
        }
        char buffer[Detail::kDoubleTextCapacity];
        out_.append(buffer, Detail::WriteDouble(value, buffer));
    } catch (const std::bad_alloc&) {
        Fail(JsonErrorCode::OutOfMemory, "out of memory");
    }
}

void Writer::Bool(const bool value) {
    try {
        BeforeValue();
        out_ += value ? "true" : "false";
    } catch (const std::bad_alloc&) {
        Fail(JsonErrorCode::OutOfMemory, "out of memory");
    }
}

void Writer::Null() {
    try {
        BeforeValue();
        out_ += "null";
    } catch (const std::bad_alloc&) {
        Fail(JsonErrorCode::OutOfMemory, "out of memory");
    }
}

void Writer::NumberText(const std::string_view text) {
    try {
        BeforeValue();
        out_.append(text);
    } catch (const std::bad_alloc&) {
        Fail(JsonErrorCode::OutOfMemory, "out of memory");
    }
}

JsonResult<void> Writer::Finish() {
    GYO_ASSERT(!finished_ && "Finish called twice");
    GYO_ASSERT(depth_ == 0 && lost_ == 0 && "Finish with an open container");
    GYO_ASSERT(topWritten_ && "Finish with nothing written");
    finished_ = true;
    if (error_) {
        out_.resize(startLength_);
        return Err(std::move(*error_));
    }
    return {};
}

void WriteValue(Writer& writer, const Value value) {
    struct Open {
        bool object;
        ObjectView::Iterator member;
        ObjectView::Iterator memberEnd;
        ArrayView::Iterator element;
        ArrayView::Iterator elementEnd;
    };
    // An allocation failure inside closes what this call opened, so the
    // caller's structure is as if the value had been written, and is reported
    // by Finish (OutOfMemory) like any other.
    const std::size_t depthBefore = writer.depth_;
    const std::size_t lostBefore = writer.lost_;
    try {
        std::vector<Open> open;
        const auto emit = [&](const Value item) {
            switch (item.GetKind()) {
            case Kind::Null: writer.Null(); break;
            case Kind::Bool: writer.Bool(item.AsBool().value()); break;
            case Kind::Number: writer.NumberText(item.AsNumberText().value()); break;
            case Kind::String: writer.String(item.AsString().value()); break;
            case Kind::Object: {
                const ObjectView object = item.AsObject().value();
                writer.BeginObject();
                open.push_back({true, object.begin(), object.end(), {}, {}});
                break;
            }
            case Kind::Array: {
                const ArrayView array = item.AsArray().value();
                writer.BeginArray();
                open.push_back({false, {}, {}, array.begin(), array.end()});
                break;
            }
            }
        };
        emit(value);
        while (!open.empty()) {
            Open& top = open.back();
            if (top.object) {
                if (top.member == top.memberEnd) {
                    open.pop_back();
                    writer.EndObject();
                    continue;
                }
                const Member member = *top.member;
                ++top.member;
                writer.Key(member.key);
                emit(member.value);  // may push: top is not used after this
            } else {
                if (top.element == top.elementEnd) {
                    open.pop_back();
                    writer.EndArray();
                    continue;
                }
                const Value element = *top.element;
                ++top.element;
                emit(element);
            }
        }
    } catch (const std::bad_alloc&) {
        writer.lost_ = lostBefore;
        while (writer.depth_ > depthBefore) writer.Pop();
        writer.Fail(JsonErrorCode::OutOfMemory, "out of memory");
    }
}

} // namespace GYO::Engine::Json
