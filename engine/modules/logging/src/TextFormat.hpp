#pragma once

#include <string>

#include "engine/logging/Record.hpp"

// The text form of a record, for people (section 5.12 of
// docs/architecture/target/v2/logging-declaration.zh-Hant.md): not a data
// contract. "[category] level: text key=value ...", the start of
// GYO::Engine::Base::Log's default line.

namespace GYO::Engine::Logging::Detail {

// Appends the line without its newline.
void AppendTextLine(std::string& out, const Record& record);

// The agreed text of a double: the shortest form that reads back, and "NaN",
// "Infinity", "-Infinity" for the non-finite values (question 3, approved).
void AppendDouble(std::string& out, double value);

} // namespace GYO::Engine::Logging::Detail
