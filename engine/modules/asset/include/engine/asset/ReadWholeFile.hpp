#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include "engine/base/Error.hpp"
#include "engine/base/Result.hpp"

// Reading one whole file into memory before it is decoded, with a byte limit
// (declaration 5.16 "whole file in memory", R9). Goes through engine/io; no
// engine/io type appears here.

namespace GYO::Engine::Asset {

// E2: this file was not read. Zero is not a valid code; the numeric values are
// not a data contract.
enum class FileReadFailureCode : std::uint8_t {
    // There is no file at the path.
    NotFound = 1,
    // The file exists or may exist but could not be opened or read.
    ReadFailed,
    // The file has more bytes than the limit.
    TooLarge,
};

[[nodiscard]] constexpr const char* ToString(const FileReadFailureCode code) noexcept {
    switch (code) {
    case FileReadFailureCode::NotFound: return "NotFound";
    case FileReadFailureCode::ReadFailed: return "ReadFailed";
    case FileReadFailureCode::TooLarge: return "TooLarge";
    }
    return "Unknown";
}

using FileReadFailure = Base::Error<FileReadFailureCode>;
static_assert(Base::CodedError<FileReadFailure>);

// The bytes of the file at nativePath (UTF-8). A file of exactly maxBytes
// bytes is read; one more is TooLarge.
[[nodiscard]] Base::Result<std::vector<std::byte>, FileReadFailure> ReadWholeFile(std::string_view nativePath,
                                                                                  std::size_t maxBytes);

} // namespace GYO::Engine::Asset
