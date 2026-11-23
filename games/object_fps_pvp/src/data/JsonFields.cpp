#include "data/JsonFields.hpp"

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

} // namespace object_fps_pvp::data
