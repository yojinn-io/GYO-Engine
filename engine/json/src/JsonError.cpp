#include "engine/json/JsonError.hpp"

namespace GYO::Engine::Json {

const char* ToString(const JsonErrorCode code) noexcept {
    switch (code) {
    case JsonErrorCode::Syntax: return "Syntax";
    case JsonErrorCode::RepeatedKey: return "RepeatedKey";
    case JsonErrorCode::NumberOutOfRange: return "NumberOutOfRange";
    case JsonErrorCode::WrongKind: return "WrongKind";
    case JsonErrorCode::NotRepresentable: return "NotRepresentable";
    case JsonErrorCode::OutOfMemory: return "OutOfMemory";
    }
    return "Unknown";
}

} // namespace GYO::Engine::Json
