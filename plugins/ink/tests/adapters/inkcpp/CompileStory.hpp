#pragma once

// Test support: compiles an ink.json file to the bytes LoadStory reads, with
// inkcpp's compiler, in memory. A product reads stories compiled ahead of
// time; only the tests link the compiler.

#include <cstddef>
#include <string>
#include <vector>

namespace GYO::Plugins::Ink::Test {

// The compiled bytes of tests/data/<name> (an .ink.json file). Fails the
// running test case when the file cannot be read or does not compile.
std::vector<std::byte> CompileStory(const std::string& name);

} // namespace GYO::Plugins::Ink::Test
