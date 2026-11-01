// The framework's FileService against engine/io's user-file helpers that replace it
// (docs/architecture/plans/framework-rework, v8-engine-io-user-files). Stamps must match. For containment,
// engine/io must agree with where the OS puts the file; where the framework's IsPathWithin answers otherwise
// (a root ending in a separator, a root that is a link or spelled in another case, '..' after a link) the
// case is reported, so the tool line sees each behaviour change it takes on (T2). Removed with FileService
// in v8-framework-invariants.
#include <doctest/doctest.h>

#include "engine/io/fs/NativeFileSystem.hpp"
#include "engine/io/helpers/UserFiles.hpp"
#include "gyo/framework/FileService.hpp"

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace {

class ParityTree final {
public:
    ParityTree() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        root_ = std::filesystem::temp_directory_path() / ("gyo-user-files-parity-" + std::to_string(stamp));
        std::filesystem::create_directories(root_);
    }
    ParityTree(const ParityTree&) = delete;
    ParityTree& operator=(const ParityTree&) = delete;
    ~ParityTree() {
        std::error_code ignored;
        std::filesystem::remove_all(root_, ignored);
    }
    void Write(const std::string& relative, const std::string& bytes) const {
        const auto file = Path(relative);
        std::filesystem::create_directories(file.parent_path());
        std::ofstream(file, std::ios::binary).write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }
    std::filesystem::path Path(const std::string& relative = {}) const {
        return relative.empty() ? root_ : root_ / std::filesystem::path(std::u8string(relative.begin(), relative.end()));
    }
    std::string Native(const std::string& relative = {}) const {
        const std::u8string text = Path(relative).generic_u8string();
        return std::string(text.begin(), text.end());
    }

private:
    std::filesystem::path root_;
};

std::filesystem::path FromUtf8(const std::string& text) {
    return std::filesystem::path(std::u8string(text.begin(), text.end()));
}

// Where the OS puts a file written at path: the parent as the OS resolves it, then the last name.
bool WhereTheOsPutsItIsInside(const std::string& candidate, const std::string& root) {
    if (candidate.empty() || root.empty()) return false;
    const std::filesystem::path path = std::filesystem::absolute(FromUtf8(candidate));
    const std::filesystem::path lands = path.filename() == ".." || path.filename() == "."
                                            ? std::filesystem::weakly_canonical(path)
                                            : std::filesystem::weakly_canonical(path.parent_path()) / path.filename();
    std::filesystem::path base = std::filesystem::weakly_canonical(std::filesystem::absolute(FromUtf8(root)));
    if (!base.has_filename() && base.has_relative_path()) base = base.parent_path();
    auto part = lands.begin();
    for (auto basePart = base.begin(); basePart != base.end(); ++basePart, ++part) {
        if (part == lands.end() || *part != *basePart) return false;
    }
    return true;
}

int changed = 0;

void CheckContainment(const std::string& candidate, const std::string& root) {
    const bool old = Gyo::Tools::Framework::IsPathWithin(FromUtf8(candidate), FromUtf8(root));
    auto now = Engine::IO::FS::IsNativePathWithin(candidate, root);
    REQUIRE(now);
    const bool os = WhereTheOsPutsItIsInside(candidate, root);
    CHECK_MESSAGE(now.value() == os, "'" << candidate << "' within '" << root << "': engine/io " << now.value()
                                         << ", the OS " << os);
    if (now.value() != old) {
        ++changed;
        MESSAGE("changed from the framework: '" << candidate << "' within '" << root << "': framework " << old
                                                << ", engine/io " << now.value());
    }
}

} // namespace

TEST_CASE("engine/io decides containment as the OS places files, and reports where the framework differed") {
    const ParityTree tree;
    tree.Write("root/inside/doc.json", "");
    tree.Write("rootx/doc.json", "");
    tree.Write("Case/doc.json", "");
    tree.Write("名前/doc.json", "");
    const std::vector<std::pair<std::string, std::string>> cases = {
        {tree.Native("root"), tree.Native("root")},
        {tree.Native("root/inside/doc.json"), tree.Native("root")},
        {tree.Native("root/not-yet/new.json"), tree.Native("root")},
        {tree.Native("rootx/doc.json"), tree.Native("root")},
        {tree.Native("root/../rootx/doc.json"), tree.Native("root")},
        {tree.Native("root/./inside/../inside/doc.json"), tree.Native("root")},
        {tree.Native("root"), tree.Native("root/inside")},
        {tree.Native("root/doc.json"), tree.Native("root") + "/"},
        {tree.Native("root") + "/", tree.Native("root") + "/"},
        {tree.Native("Case/doc.json"), tree.Native("case")},
        {tree.Native("名前/doc.json"), tree.Native("名前")},
        {"relative/file.json", tree.Native("root")},
        {"", tree.Native("root")},
        {tree.Native("root"), ""},
    };
    for (const auto& [candidate, root] : cases) CheckContainment(candidate, root);

    std::error_code error;
    std::filesystem::create_directory_symlink(tree.Path("root"), tree.Path("alias"), error);
    if (!error) std::filesystem::create_symlink(tree.Path("rootx/doc.json"), tree.Path("root/link.json"), error);
    if (!error) std::filesystem::create_directory_symlink(tree.Path("rootx"), tree.Path("root/dirlink"), error);
    if (error) {
        MESSAGE("symbolic links unavailable here: " << error.message());
        return;
    }
    const std::vector<std::pair<std::string, std::string>> linked = {
        {tree.Native("alias/inside/doc.json"), tree.Native("root")},
        {tree.Native("root/inside/doc.json"), tree.Native("alias")},
        {tree.Native("root/link.json"), tree.Native("root")},
        {tree.Native("root/dirlink/doc.json"), tree.Native("root")},
        {tree.Native("root/dirlink/../inside/doc.json"), tree.Native("root")},
        {tree.Native("alias/../rootx/doc.json"), tree.Native("root")},
    };
    for (const auto& [candidate, root] : linked) CheckContainment(candidate, root);
    MESSAGE(changed << " cases changed from the framework");
}

TEST_CASE("engine/io stamps a file as the framework's ProbeFileStamp did") {
    const ParityTree tree;
    tree.Write("doc.json", "{\"a\":1}");
    tree.Write("dir/keep.txt", "");
    Engine::IO::FS::NativeFileSystem files;
    Engine::IO::Helpers::ReadAllOptions anySize;
    anySize.maxBytes = (std::numeric_limits<std::size_t>::max)();
    for (const std::string relative : {"doc.json", "missing.json", "dir"}) {
        auto old = Gyo::Tools::Framework::ProbeFileStamp(tree.Path(relative));
        auto uri = Engine::IO::FS::NativeFileUri(tree.Native(relative));
        REQUIRE(uri);
        auto now = Engine::IO::Helpers::ProbeFileStamp(files, uri.value(), anySize);
        CHECK_MESSAGE(old.has_value() == now.has_value(), relative);
        if (!old || !now) continue;
        CHECK(old.value().exists == now.value().exists);
        CHECK(old.value().size == now.value().sizeBytes);
        CHECK(old.value().writeTime.count() == now.value().mtimeNs);
        CHECK(old.value().contentHash == now.value().contentHash);
    }
}
