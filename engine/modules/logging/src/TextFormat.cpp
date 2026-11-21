#include "TextFormat.hpp"

#include <charconv>
#include <cmath>
#include <string_view>

namespace GYO::Engine::Logging::Detail {

namespace {

template <class T>
void AppendNumber(std::string& out, const T value) {
    char buffer[32];
    const auto result = std::to_chars(buffer, buffer + sizeof buffer, value);
    out.append(buffer, result.ptr);
}

// A value is quoted when it is empty or holds a space, a quote, a backslash,
// '=' or a control character, so that "key=value" stays readable.
bool NeedsQuotes(const std::string_view text) noexcept {
    if (text.empty()) return true;
    for (const char c : text) {
        const auto byte = static_cast<unsigned char>(c);
        if (byte <= 0x20 || c == '"' || c == '\\' || c == '=' || byte == 0x7f) return true;
    }
    return false;
}

void AppendQuoted(std::string& out, const std::string_view text) {
    out += '"';
    for (const char c : text) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default: out += c; break;
        }
    }
    out += '"';
}

} // namespace

void AppendDouble(std::string& out, const double value) {
    if (std::isnan(value)) {
        out += "NaN";
    } else if (std::isinf(value)) {
        out += value > 0 ? "Infinity" : "-Infinity";
    } else {
        AppendNumber(out, value);
    }
}

void AppendTextLine(std::string& out, const Record& record) {
    out += '[';
    out += record.category;
    out += "] ";
    out += ToString(record.level);
    out += ": ";
    // A line break in the text would split the line; it is written escaped.
    for (const char c : record.text) {
        if (c == '\n') {
            out += "\\n";
        } else if (c == '\r') {
            out += "\\r";
        } else {
            out += c;
        }
    }
    if (!record.world.empty()) {
        out += " world=";
        if (NeedsQuotes(record.world)) {
            AppendQuoted(out, record.world);
        } else {
            out += record.world;
        }
    }
    if (record.worldInterrupted) out += " world_interrupted=true";
    for (const Field& field : record.fields) {
        out += ' ';
        out += field.key;
        out += '=';
        switch (field.value.GetType()) {
        case FieldValue::Type::Int: AppendNumber(out, field.value.AsInt()); break;
        case FieldValue::Type::Uint: AppendNumber(out, field.value.AsUint()); break;
        case FieldValue::Type::Double: AppendDouble(out, field.value.AsDouble()); break;
        case FieldValue::Type::Bool: out += field.value.AsBool() ? "true" : "false"; break;
        case FieldValue::Type::String:
            if (NeedsQuotes(field.value.AsString())) {
                AppendQuoted(out, field.value.AsString());
            } else {
                out += field.value.AsString();
            }
            break;
        }
    }
}

} // namespace GYO::Engine::Logging::Detail
