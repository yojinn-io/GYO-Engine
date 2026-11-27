#include "adapters/inkcpp/CompileStory.hpp"

#include <doctest/doctest.h>

#include <array>
#include <span>
#include <sstream>
#include <stdexcept>

#include "engine/base/Error.hpp"
#include "engine/os/Files.hpp"

#include <compiler.h>

namespace GYO::Plugins::Ink::Test {

namespace {

std::string ReadText(const std::string& path) {
    auto reader = Engine::OS::Files::FileReader::Open(path);
    REQUIRE_MESSAGE(reader, path << ": " << (reader ? std::string() : Engine::Base::Describe(reader.error())));
    std::string text;
    std::array<std::byte, 4096> block{};
    for (;;) {
        auto read = reader->Read(block);
        REQUIRE_MESSAGE(read, path << ": " << (read ? std::string() : Engine::Base::Describe(read.error())));
        if (*read == 0) break;
        for (std::size_t i = 0; i < *read; ++i) text.push_back(static_cast<char>(block[i]));
    }
    return text;
}

} // namespace

std::vector<std::byte> CompileStory(const std::string& name) {
    const std::string path = std::string(GYO_INK_TEST_DATA) + "/" + name;
    std::istringstream in(ReadText(path));
    std::ostringstream out;
    try {
        ink::compiler::run(in, out);
    } catch (const std::runtime_error& e) {
        FAIL("inkcpp's compiler rejected " << path << ": " << e.what());
    }
    const std::string bytes = out.str();
    std::vector<std::byte> compiled(bytes.size());
    for (std::size_t i = 0; i < bytes.size(); ++i) compiled[i] = static_cast<std::byte>(bytes[i]);
    return compiled;
}

} // namespace GYO::Plugins::Ink::Test
