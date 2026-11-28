#include "data/JsonFields.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace object_fps_pvp::data {

namespace Json = GYO::Engine::Json;

namespace {

[[noreturn]] void Fail(const std::string_view what, const Json::JsonError& error) {
    std::string message{what};
    message += ": ";
    message += error.message;
    if (!error.detail.empty()) {
        message += " (";
        message += error.detail;
        message += ')';
    }
    throw std::runtime_error(message);
}

} // namespace

Json::Document ParseDocument(const std::string_view text) {
    auto document = Json::Parse(text);
    if (!document) Fail("not JSON", document.error());
    return std::move(document).value();
}

Json::ObjectView AsObject(const Json::Value& value, const std::string_view what) {
    const auto object = value.AsObject();
    if (!object) Fail(what, object.error());
    return object.value();
}

Json::Value Member(const Json::ObjectView& object, const std::string_view key) {
    const auto value = object.Find(key);
    if (!value) throw std::runtime_error("missing member " + std::string{key});
    return *value;
}

bool HasMember(const Json::ObjectView& object, const std::string_view key) {
    return object.Find(key).has_value();
}

double Number(const Json::Value& value, const std::string_view what) {
    const auto number = value.AsDouble();
    if (!number) Fail(what, number.error());
    return number.value();
}

std::int64_t Integer(const Json::Value& value, const std::string_view what) {
    const auto number = value.AsInt64();
    if (!number) Fail(what, number.error());
    return number.value();
}

std::uint64_t Unsigned(const Json::Value& value, const std::string_view what) {
    const auto number = value.AsUint64();
    if (!number) Fail(what, number.error());
    return number.value();
}

std::string String(const Json::Value& value, const std::string_view what) {
    const auto text = value.AsString();
    if (!text) Fail(what, text.error());
    return std::string{text.value()};
}

bool Boolean(const Json::Value& value, const std::string_view what) {
    const auto flag = value.AsBool();
    if (!flag) Fail(what, flag.error());
    return flag.value();
}

float Positive(const Json::Value& value, const std::string_view what) {
    const auto number = static_cast<float>(Number(value, what));
    if (!std::isfinite(number) || number <= 0) throw std::runtime_error(std::string{what} + " is not a finite positive number");
    return number;
}

float PositiveOr(const Json::ObjectView& object, const std::string_view key, const float fallback) {
    return HasMember(object, key) ? Positive(Member(object, key), key) : fallback;
}

std::string ContentPath(const Json::Value& value, const std::string_view what) {
    std::string path = String(value, what);
    std::size_t start = 0;
    while (true) {
        const std::size_t end = path.find('/', start);
        const std::string_view part = std::string_view(path).substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (part.empty() || part == "." || part == ".." || part.find_first_of("\\:") != std::string_view::npos) {
            throw std::runtime_error(std::string{what} + " is not a path inside the content: " + path);
        }
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return path;
}

} // namespace object_fps_pvp::data
