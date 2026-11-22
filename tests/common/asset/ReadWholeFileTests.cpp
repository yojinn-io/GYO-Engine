// Reading a whole file with a byte limit (declaration 5.16, R9).
#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "engine/asset/ReadWholeFile.hpp"

namespace Asset = GYO::Engine::Asset;
using Asset::FileReadFailureCode;

namespace {

std::string WriteFile(const std::string& name, const std::string& bytes) {
    const std::string path = std::string(GYO_ASSET_TEST_BINARY_DIR) + "/" + name;
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    return path;
}

} // namespace

TEST_CASE("read: a file within the limit is read whole") {
    std::string bytes(1000, 'x');
    bytes[0] = '\0';
    bytes[999] = '\xff';
    const std::string path = WriteFile("read-exact.bin", bytes);
    auto read = Asset::ReadWholeFile(path, 1000);
    REQUIRE_MESSAGE(read, "a file of exactly the limit is refused");
    CHECK(std::string(reinterpret_cast<const char*>(read->data()), read->size()) == bytes);

    auto empty = Asset::ReadWholeFile(WriteFile("read-empty.bin", ""), 0);
    REQUIRE(empty);
    CHECK(empty->empty());
}

TEST_CASE("read: a file over the limit is TooLarge") {
    const std::string path = WriteFile("read-over.bin", std::string(1001, 'x'));
    auto read = Asset::ReadWholeFile(path, 1000);
    REQUIRE(!read);
    CHECK(std::string(Asset::ToString(read.error().code)) == "TooLarge");
}

TEST_CASE("read: a missing file is NotFound") {
    auto read = Asset::ReadWholeFile(std::string(GYO_ASSET_TEST_BINARY_DIR) + "/no-such-file.bin", 1000);
    REQUIRE(!read);
    CHECK(std::string(Asset::ToString(read.error().code)) == "NotFound");
}

TEST_CASE("read: a directory is ReadFailed") {
    const std::string path = std::string(GYO_ASSET_TEST_BINARY_DIR) + "/read-directory";
    std::filesystem::create_directories(path);
    auto read = Asset::ReadWholeFile(path, 1000);
    REQUIRE(!read);
    CHECK(std::string(Asset::ToString(read.error().code)) == "ReadFailed");
}
