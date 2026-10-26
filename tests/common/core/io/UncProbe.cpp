// Windows UNC paths through engine/io (docs/architecture/io.md, D54). Built on
// Windows only.
//
//   gyo_io_unc_probe <host>
//
// A tree under the temporary directory is named through the administrative
// share of its drive, \\<host>\C$\..., and each file there round-trips native
// path -> file URI -> native path, then reads and is written through
// NativeFileSystem: at a short path, and at a full path of more than 300
// characters, which engine/os hands the API as \\?\UNC\ (os.md, Long paths on
// Windows). With <host> localhost the URI is file:////localhost/...; with any
// other host it is file://<host>/....
//
// Exit 0 when every check passes, 1 when one fails, 77 (CTest: Skipped) when
// the share is not reachable from this machine, as on a runner without the
// Server service or administrative shares. Only that reachability, checked
// with the standard library before engine/io is asked anything, skips.
#include "engine/io/fs/NativeFileSystem.hpp"
#include "engine/io/helpers/ReadAllBytes.hpp"
#include "engine/io/path/Uri.hpp"

#include <chrono>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

using namespace Engine::IO;

namespace {

constexpr int kSkipped = 77;

int failures = 0;

void Check(const bool ok, const std::string& what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
    if (!ok) ++failures;
}

std::string Utf8(const std::filesystem::path& path) {
    const std::u8string text = path.generic_u8string();
    return std::string(text.begin(), text.end());
}

std::filesystem::path FromUtf8(const std::string& text) {
    return std::filesystem::path(std::u8string(text.begin(), text.end()));
}

// The test's own long-path form for setup and cleanup: the standard library
// has no long-path handling of its own.
std::filesystem::path Setup(const std::filesystem::path& local) {
    std::filesystem::path out(L"\\\\?\\");
    out += std::filesystem::absolute(local).make_preferred();
    return out;
}

std::string AsText(const std::vector<std::byte>& bytes) {
    std::string out;
    for (const std::byte b : bytes) out.push_back(static_cast<char>(b));
    return out;
}

std::string Describe(const IoError& error) { return error.message + " " + error.detail; }

// One file named by its UNC path: the URI text, the way back, a read and a
// write through NativeFileSystem.
void CheckFile(const std::string& host, const std::string& unc, const std::filesystem::path& local,
               const std::string& bytes) {
    std::printf("-- %zu-byte UNC path %s\n", unc.size(), unc.c_str());
    FS::NativeFileSystem fs;
    std::string backslashed = unc;
    for (auto& c : backslashed) {
        if (c == '/') c = '\\';
    }
    for (const auto& form : {unc, backslashed}) {
        auto uri = FS::NativeFileUri(form);
        Check(static_cast<bool>(uri), "file URI of " + form + (uri ? "" : ": " + Describe(uri.error())));
        if (!uri) continue;
        const std::string text = uri.value().ToString();
        const std::string expected =
            (host == "localhost" ? "file:////localhost" : "file://" + host) + unc.substr(2 + host.size());
        Check(text == expected, "URI text " + text + " is " + expected);
        auto parsed = Path::ParseUri(text);
        Check(static_cast<bool>(parsed), "URI text parses back");
        if (!parsed) continue;
        auto back = Path::NativePathFromFileUri(parsed.value());
        Check(back && back.value() == unc, "native path back is " + unc + (back ? " (got " + back.value() + ")" : ""));

        const auto read = Helpers::ReadAllBytes(fs, parsed.value());
        Check(read && AsText(read.value()) == bytes, "reads the file" + (read ? "" : ": " + Describe(read.error())));
        const auto status = fs.Stat(parsed.value());
        Check(status && status.value().sizeBytes == bytes.size(), "stat gives its size");
        const auto exists = fs.Exists(parsed.value());
        Check(exists && exists.value(), "exists");
    }

    // A file written through the same share lands beside it.
    const auto written = FS::NativeFileUri(unc + ".written");
    if (written) {
        auto stream = fs.Open(written.value(), Stream::OpenWriteBinaryTruncate());
        Check(static_cast<bool>(stream), "opens for writing" + (stream ? "" : ": " + Describe(stream.error())));
        if (stream) {
            const auto wrote = stream.value()->Write(bytes.data(), bytes.size());
            Check(wrote && wrote.value() == bytes.size(), "writes");
            stream.value().reset();
            auto writtenLocal = local;
            writtenLocal += ".written";
            std::ifstream file(Setup(writtenLocal), std::ios::binary);
            const std::string back{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
            Check(back == bytes, "the written bytes are in the local file");
        }
    }
    const auto missing = FS::NativeFileUri(unc + ".missing");
    if (missing) {
        const auto status = fs.Stat(missing.value());
        Check(!status && status.error().code == IoErrorCode::NotFound, "a missing file is NotFound");
    }
}

} // namespace

int main(const int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: gyo_io_unc_probe <host>\n");
        return 2;
    }
    const std::string host = argv[1];

    std::error_code error;
    const auto temp = std::filesystem::temp_directory_path(error);
    if (error) {
        std::printf("FAIL no temporary directory: %s\n", error.message().c_str());
        return 1;
    }
    const std::string tempText = Utf8(std::filesystem::absolute(temp));
    if (tempText.size() < 3 || tempText[1] != ':' || tempText[2] != '/') {
        std::printf("FAIL the temporary directory %s is not on a drive\n", tempText.c_str());
        return 1;
    }
    const std::string share = "//" + host + "/" + tempText.substr(0, 1) + "$";
    const bool reachable = std::filesystem::is_directory(FromUtf8(share + "/"), error);
    if (!reachable) {
        std::printf("skip %s is not reachable here: %s\n", share.c_str(),
                    error ? error.message().c_str() : "not a directory");
        return kSkipped;
    }

    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto local = std::filesystem::absolute(temp) / ("gyo-io-unc-" + std::to_string(stamp));
    std::filesystem::path deep = local;
    for (int i = 0; i < 8; ++i) deep /= std::string(40, static_cast<char>('a' + i));
    std::filesystem::create_directories(Setup(deep));
    {
        std::ofstream(Setup(local / "short.bin"), std::ios::binary) << "unc short";
        std::ofstream(Setup(deep / "long.bin"), std::ios::binary) << "unc long";
    }
    const auto ToUnc = [&](const std::filesystem::path& path) { return share + Utf8(path).substr(2); };

    CheckFile(host, ToUnc(local / "short.bin"), local / "short.bin", "unc short");
    const std::string longUnc = ToUnc(deep / "long.bin");
    Check(longUnc.size() > 300, "the long UNC path is longer than 300 bytes");
    CheckFile(host, longUnc, deep / "long.bin", "unc long");

    FS::NativeFileSystem fs;
    const auto made = FS::NativeFileUri(ToUnc(deep / "made" / "here"));
    Check(made && fs.CreateDirectories(made.value()), "creates directories under the long UNC path");
    Check(std::filesystem::is_directory(Setup(deep / "made" / "here")), "the directories are in the local tree");

    std::filesystem::remove_all(Setup(local), error);
    std::printf("%s\n", failures == 0 ? "passed" : "failed");
    return failures == 0 ? 0 : 1;
}
