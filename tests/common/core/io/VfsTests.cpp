#include "doctest/doctest.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "engine/io/fs/Vfs.hpp"
#include "engine/io/stream/MemoryStream.hpp"

using namespace Engine::IO;

namespace {

// Characterization of the only production branch on an error code: the VFS
// read overlay treats NotFound as "try the next mount" and stops on anything
// else. Stat, read Open and Exists share one scripted outcome; every other
// operation reports NotSupported.
class ScriptedFileSystem final : public FS::IFileSystem {
public:
    explicit ScriptedFileSystem(std::string name) : name_(std::move(name)) {}

    void Found() { error_.reset(); }
    void Fails(IoErrorCode code, std::string message) {
        error_ = FS::IoError::Make(code, std::move(message));
    }
    int Calls() const noexcept { return calls_; }

    const char* Name() const noexcept override { return name_.c_str(); }

    FS::IoResult<FS::FileInfo> Stat(const Path::Uri&) override {
        ++calls_;
        if (error_) return Engine::Base::Err(*error_);
        FS::FileInfo info;
        info.type = FS::FileType::Regular;
        info.backend = name_;
        return info;
    }

    FS::IoResult<std::unique_ptr<Stream::IStream>> Open(const Path::Uri&, Stream::FileOpenMode) override {
        ++calls_;
        if (error_) return Engine::Base::Err(*error_);
        std::vector<std::byte> bytes(name_.size());
        for (std::size_t i = 0; i < name_.size(); ++i) bytes[i] = static_cast<std::byte>(name_[i]);
        return std::make_unique<Stream::MemoryStream>(std::move(bytes), Stream::MemoryStream::Options{});
    }
    FS::IoResult<bool> Exists(const Path::Uri&) override {
        ++calls_;
        if (error_) return Engine::Base::Err(*error_);
        return true;
    }
    FS::IoResult<void> CreateDirectories(const Path::Uri&) override {
        return Engine::Base::Err(Unsupported());
    }
    FS::IoResult<void> Remove(const Path::Uri&, const FS::RemoveOptions&) override {
        return Engine::Base::Err(Unsupported());
    }
    FS::IoResult<void> Move(const Path::Uri&, const Path::Uri&) override {
        return Engine::Base::Err(Unsupported());
    }
    FS::IoResult<void> Copy(const Path::Uri&, const Path::Uri&) override {
        return Engine::Base::Err(Unsupported());
    }
    FS::IoResult<std::vector<FS::DirectoryEntry>> List(const Path::Uri&, const FS::ListOptions&) override {
        return Engine::Base::Err(Unsupported());
    }
    FS::IoResult<std::string> ToNativePathString(const Path::Uri&) override {
        return Engine::Base::Err(Unsupported());
    }
    FS::FileSystemCapabilities Capabilities() const noexcept override { return {}; }
    FS::IoResult<std::unique_ptr<FS::DirectoryIterator>> Iterate(const Path::Uri&, const FS::ListOptions&) override {
        return Engine::Base::Err(Unsupported());
    }
    FS::IoResult<std::unique_ptr<FS::IFileWatcher>> CreateWatcher() override {
        return Engine::Base::Err(Unsupported());
    }

private:
    static FS::IoError Unsupported() {
        return FS::IoError::Make(IoErrorCode::NotSupported, "ScriptedFileSystem: not scripted");
    }

    std::string name_;
    std::optional<FS::IoError> error_;
    int calls_ = 0;
};

struct OverlayFixture {
    std::shared_ptr<ScriptedFileSystem> high = std::make_shared<ScriptedFileSystem>("high");
    std::shared_ptr<ScriptedFileSystem> low = std::make_shared<ScriptedFileSystem>("low");
    FS::Vfs vfs;
    Path::Uri uri = Path::ParseUriLoose("asset://textures/x.png");

    OverlayFixture() {
        REQUIRE(vfs.Mount(MakeMount("high", 10, high)));
        REQUIRE(vfs.Mount(MakeMount("low", 0, low)));
    }

    static FS::MountPoint MakeMount(std::string name, std::int32_t priority,
                                    std::shared_ptr<ScriptedFileSystem> fs) {
        FS::MountPoint mount;
        mount.name = name;
        mount.priority = priority;
        mount.readOnly = true;
        mount.mountUri = Path::ParseUriLoose("asset://");
        mount.rootUri = Path::ParseUriLoose("file:///" + name);
        mount.fs = std::move(fs);
        return mount;
    }
};

} // namespace

TEST_CASE("Vfs: Stat falls through NotFound to the next mount") {
    OverlayFixture f;
    f.high->Fails(IoErrorCode::NotFound, "high: missing");
    f.low->Found();

    const auto info = f.vfs.Stat(f.uri);
    REQUIRE(info);
    CHECK(info.value().backend == "low");
    CHECK(f.high->Calls() == 1);
    CHECK(f.low->Calls() == 1);
}

TEST_CASE("Vfs: Stat stops at the first error that is not NotFound") {
    OverlayFixture f;
    f.high->Fails(IoErrorCode::PermissionDenied, "high: denied");
    f.low->Found();

    const auto info = f.vfs.Stat(f.uri);
    REQUIRE_FALSE(info);
    CHECK(info.error().code == IoErrorCode::PermissionDenied);
    CHECK(info.error().message == "high: denied");
    CHECK(f.low->Calls() == 0);
}

TEST_CASE("Vfs: Stat reports the last NotFound when every mount misses") {
    OverlayFixture f;
    f.high->Fails(IoErrorCode::NotFound, "high: missing");
    f.low->Fails(IoErrorCode::NotFound, "low: missing");

    const auto info = f.vfs.Stat(f.uri);
    REQUIRE_FALSE(info);
    CHECK(info.error().code == IoErrorCode::NotFound);
    CHECK(info.error().message == "low: missing");
    CHECK(f.high->Calls() == 1);
    CHECK(f.low->Calls() == 1);
}

TEST_CASE("Vfs: read Open follows the same NotFound overlay rule") {
    SUBCASE("falls through NotFound") {
        OverlayFixture f;
        f.high->Fails(IoErrorCode::NotFound, "high: missing");
        f.low->Found();
        auto stream = f.vfs.Open(f.uri, Stream::FileOpenMode::Read);
        REQUIRE(stream);
        auto size = stream.value()->Size();
        REQUIRE(size);
        CHECK(size.value() == 3); // "low"
        CHECK(f.low->Calls() == 1);
    }
    SUBCASE("stops on another error") {
        OverlayFixture f;
        f.high->Fails(IoErrorCode::ReadFailed, "high: broken");
        f.low->Found();
        const auto stream = f.vfs.Open(f.uri, Stream::FileOpenMode::Read);
        REQUIRE_FALSE(stream);
        CHECK(stream.error().code == IoErrorCode::ReadFailed);
        CHECK(f.low->Calls() == 0);
    }
    SUBCASE("reports the last NotFound") {
        OverlayFixture f;
        f.high->Fails(IoErrorCode::NotFound, "high: missing");
        f.low->Fails(IoErrorCode::NotFound, "low: missing");
        const auto stream = f.vfs.Open(f.uri, Stream::FileOpenMode::Read);
        REQUIRE_FALSE(stream);
        CHECK(stream.error().code == IoErrorCode::NotFound);
        CHECK(stream.error().message == "low: missing");
    }
}

TEST_CASE("Vfs: Exists treats NotFound as absent and stops on another error") {
    SUBCASE("NotFound then found") {
        OverlayFixture f;
        f.high->Fails(IoErrorCode::NotFound, "high: missing");
        f.low->Found();
        const auto exists = f.vfs.Exists(f.uri);
        REQUIRE(exists);
        CHECK(exists.value());
    }
    SUBCASE("every mount misses") {
        OverlayFixture f;
        f.high->Fails(IoErrorCode::NotFound, "high: missing");
        f.low->Fails(IoErrorCode::NotFound, "low: missing");
        const auto exists = f.vfs.Exists(f.uri);
        REQUIRE(exists);
        CHECK_FALSE(exists.value());
    }
    SUBCASE("another error is returned") {
        OverlayFixture f;
        f.high->Fails(IoErrorCode::PermissionDenied, "high: denied");
        f.low->Found();
        const auto exists = f.vfs.Exists(f.uri);
        REQUIRE_FALSE(exists);
        CHECK(exists.error().code == IoErrorCode::PermissionDenied);
        CHECK(f.low->Calls() == 0);
    }
}
