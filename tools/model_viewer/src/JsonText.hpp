#pragma once

#include <string>
#include <string_view>

// The pieces of JSON text the viewer writes (defined in EventDocument.cpp).
namespace Gyo::Tools::ModelViewer {

// A JSON string: quotes, backslashes and control characters escaped, every
// other byte (UTF-8 included) as it is.
void AppendString(std::string& out, std::string_view text);
// The shortest decimal that reads back as the same double; negative zero as "-0.0".
void AppendSeconds(std::string& out, double seconds);

} // namespace Gyo::Tools::ModelViewer
