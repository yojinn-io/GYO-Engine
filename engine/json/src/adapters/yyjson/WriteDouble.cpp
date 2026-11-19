#include "WriteDouble.hpp"

#include <cmath>

#include <yyjson.h>

#include "engine/base/Assert.hpp"

namespace GYO::Engine::Json::Detail {

std::size_t WriteDouble(const double value, char (&buffer)[kDoubleTextCapacity]) {
    GYO_ASSERT(std::isfinite(value));
    yyjson_mut_val number;
    yyjson_mut_set_real(&number, value);
    const char* end = yyjson_mut_write_number(&number, buffer);
    GYO_ASSERT(end != nullptr);
    return static_cast<std::size_t>(end - buffer);
}

} // namespace GYO::Engine::Json::Detail
