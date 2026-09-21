#include "gyo/ui_editor/FileService.hpp"

#include <array>
#include <chrono>
#include <cwctype>
#include <fstream>
#include <random>
#include <sstream>
#include <system_error>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace Gyo::Tools::UiEditor {
namespace {

[[nodiscard]] std::uint64_t HashText(const std::string_view text) noexcept {
    std::uint64_t hash = 14695981039346656037ULL;
    for (const unsigned char value : text) {
        hash ^= static_cast<std::uint64_t>(value);
        hash *= 1099511628211ULL;
    }
    return hash;
}

[[nodiscard]] std::filesystem::path AbsoluteNormalized(
    const std::filesystem::path& path) {
    std::error_code error;
    std::filesystem::path absolute = std::filesystem::absolute(path, error);
    if (error) {
        absolute = path;
    }

    const std::filesystem::path parent = absolute.parent_path();
    std::filesystem::path canonicalParent =
        std::filesystem::weakly_canonical(parent, error);
    if (error) {
        canonicalParent = parent.lexically_normal();
    }
    return (canonicalParent / absolute.filename()).lexically_normal();
}

[[nodiscard]] bool ComponentEqual(
    const std::filesystem::path& left,
    const std::filesystem::path& right) {
#if defined(_WIN32)
    std::wstring leftText = left.native();
    std::wstring rightText = right.native();
    for (wchar_t& value : leftText) {
        value = static_cast<wchar_t>(std::towlower(value));
    }
    for (wchar_t& value : rightText) {
        value = static_cast<wchar_t>(std::towlower(value));
    }
    return leftText == rightText;
#else
    return left == right;
#endif
}

[[nodiscard]] std::filesystem::path MakeTemporaryPath(
    const std::filesystem::path& target) {
    const auto time = static_cast<unsigned long long>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    std::random_device random;
    return target.parent_path() /
           (target.filename().string() + ".tmp." + std::to_string(time) + "." +
            std::to_string(random()));
}

[[nodiscard]] FileOperationResult ReplaceFile(
    const std::filesystem::path& temporary,
    const std::filesystem::path& target) {
#if defined(_WIN32)
    if (!MoveFileExW(
            temporary.c_str(),
            target.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        return {
            false,
            "failed to replace '" + target.string() +
                "' (Win32 error " + std::to_string(GetLastError()) + ")",
        };
    }
    return {true, {}};
#else
    std::error_code error;
    std::filesystem::rename(temporary, target, error);
    if (error) {
        return {
            false,
            "failed to replace '" + target.string() + "': " + error.message(),
        };
    }
    return {true, {}};
#endif
}

} // namespace

TextFileResult ReadTextFile(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        return {std::nullopt, "cannot open file: " + path.string()};
    }
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    if (stream.bad()) {
        return {std::nullopt, "failed while reading file: " + path.string()};
    }
    return {buffer.str(), {}};
}

std::optional<FileStamp> ProbeFileStamp(
    const std::filesystem::path& path,
    std::string& error) {
    error.clear();
    std::error_code fileError;
    const bool exists = std::filesystem::exists(path, fileError);
    if (fileError) {
        error = "cannot inspect '" + path.string() + "': " + fileError.message();
        return std::nullopt;
    }
    if (!exists) {
        return FileStamp{};
    }
    if (!std::filesystem::is_regular_file(path, fileError) || fileError) {
        error = "path is not a regular file: " + path.string();
        return std::nullopt;
    }

    const TextFileResult text = ReadTextFile(path);
    if (!text) {
        error = text.error;
        return std::nullopt;
    }
    FileStamp stamp;
    stamp.exists = true;
    stamp.size = std::filesystem::file_size(path, fileError);
    if (fileError) {
        error = "cannot inspect size of '" + path.string() + "': " +
                fileError.message();
        return std::nullopt;
    }
    stamp.writeTime = std::filesystem::last_write_time(path, fileError);
    if (fileError) {
        error = "cannot inspect timestamp of '" + path.string() + "': " +
                fileError.message();
        return std::nullopt;
    }
    stamp.contentHash = HashText(*text.text);
    return stamp;
}

FileOperationResult WriteTextFileAtomically(
    const std::filesystem::path& path,
    const std::string_view text) {
    if (path.empty()) {
        return {false, "output path is empty"};
    }

    std::error_code error;
    const std::filesystem::path parent = path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, error);
        if (error) {
            return {
                false,
                "cannot create output directory '" + parent.string() + "': " +
                    error.message(),
            };
        }
    }

    const std::filesystem::path temporary = MakeTemporaryPath(path);
    {
        std::ofstream stream(
            temporary,
            std::ios::binary | std::ios::trunc);
        if (!stream) {
            return {
                false,
                "cannot create temporary output: " + temporary.string(),
            };
        }
        stream.write(text.data(), static_cast<std::streamsize>(text.size()));
        stream.flush();
        if (!stream) {
            stream.close();
            std::filesystem::remove(temporary, error);
            return {
                false,
                "failed while writing temporary output: " + temporary.string(),
            };
        }
    }

    FileOperationResult result = ReplaceFile(temporary, path);
    if (!result) {
        std::filesystem::remove(temporary, error);
    }
    return result;
}

bool IsPathWithin(
    const std::filesystem::path& candidate,
    const std::filesystem::path& root) {
    if (candidate.empty() || root.empty()) {
        return false;
    }
    const std::filesystem::path normalizedCandidate =
        AbsoluteNormalized(candidate);
    const std::filesystem::path normalizedRoot = AbsoluteNormalized(root);

    auto candidatePart = normalizedCandidate.begin();
    auto rootPart = normalizedRoot.begin();
    for (; rootPart != normalizedRoot.end(); ++rootPart, ++candidatePart) {
        if (candidatePart == normalizedCandidate.end() ||
            !ComponentEqual(*candidatePart, *rootPart)) {
            return false;
        }
    }
    return true;
}

} // namespace Gyo::Tools::UiEditor
