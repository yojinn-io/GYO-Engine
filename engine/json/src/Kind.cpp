#include "engine/json/Document.hpp"

namespace GYO::Engine::Json {

const char* ToString(const Kind kind) noexcept {
    switch (kind) {
    case Kind::Null: return "Null";
    case Kind::Bool: return "Bool";
    case Kind::Number: return "Number";
    case Kind::String: return "String";
    case Kind::Array: return "Array";
    case Kind::Object: return "Object";
    }
    return "Unknown";
}

} // namespace GYO::Engine::Json
