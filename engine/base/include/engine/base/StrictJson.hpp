#pragma once

#include <cstddef>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace Engine::Base {

// Strict JSON parsing for the modules that own a JSON data contract (today
// gyo.voxel_mesh): a repeated key in any object is an error, not "the last one
// wins" as in the parser's DOM.
//
// Json is a nlohmann::basic_json type (nlohmann::json or nlohmann::ordered_json).
// This header names nlohmann only through that parameter and includes none of
// it, so Base stays dependent on the C++ standard library alone; a module that
// instantiates it links the JSON parser itself (privately).
//
// Two passes: a SAX scan with one key set per open object finds repeated keys
// and syntax errors; then a plain parse builds the DOM. A parser callback would
// do it in one pass, but nlohmann's callback parser rescans the parent array at
// the end of every object, quadratic in the number of objects in one array
// (docs/architecture/plans/big-map/survey.zh-Hant.md, section 5 (c)).
namespace Detail {

template <class Json>
class RepeatedKeyScan final {
public:
    using number_integer_t = typename Json::number_integer_t;
    using number_unsigned_t = typename Json::number_unsigned_t;
    using number_float_t = typename Json::number_float_t;
    using string_t = typename Json::string_t;
    using binary_t = typename Json::binary_t;

    bool null() { return true; }
    bool boolean(bool) { return true; }
    bool number_integer(number_integer_t) { return true; }
    bool number_unsigned(number_unsigned_t) { return true; }
    bool number_float(number_float_t, const string_t&) { return true; }
    bool string(string_t&) { return true; }
    bool binary(binary_t&) { return true; }
    bool start_object(std::size_t) {
        open_.emplace_back();
        return true;
    }
    bool key(string_t& key) {
        if (open_.back().insert(key).second) return true;
        failure_ = "repeated key '" + std::string(key) + "'";
        return false;
    }
    bool end_object() {
        open_.pop_back();
        return true;
    }
    bool start_array(std::size_t) { return true; }
    bool end_array() { return true; }
    bool parse_error(std::size_t, const std::string&, const typename Json::exception& error) {
        failure_ = std::string("not JSON: ") + error.what();
        return false;
    }

    [[nodiscard]] const std::string& Failure() const noexcept { return failure_; }

private:
    std::vector<std::set<string_t>> open_;
    std::string failure_;
};

} // namespace Detail

// The document, or std::nullopt with failure set to "repeated key '<key>'" or
// "not JSON: <parser message>". Never throws for malformed text.
template <class Json>
[[nodiscard]] std::optional<Json> ParseStrictJson(const std::string_view text, std::string& failure) {
    Detail::RepeatedKeyScan<Json> scan;
    if (!Json::sax_parse(text.begin(), text.end(), &scan)) {
        failure = scan.Failure().empty() ? std::string("not JSON") : scan.Failure();
        return std::nullopt;
    }
    auto document = Json::parse(text.begin(), text.end(), nullptr, false);
    if (document.is_discarded()) {
        failure = "not JSON";
        return std::nullopt;
    }
    return document;
}

} // namespace Engine::Base
