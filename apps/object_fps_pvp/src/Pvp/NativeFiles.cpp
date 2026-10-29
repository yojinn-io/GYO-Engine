#include "RetroFPS/Pvp/NativeFiles.hpp"

#include "engine/base/Error.hpp"
#include "engine/io/fs/NativeFileSystem.hpp"
#include "engine/io/helpers/ReadAllBytes.hpp"
#include "engine/io/stream/IStream.hpp"
#include "engine/os/Files.hpp"

#include <cstddef>
#include <stdexcept>
#include <streambuf>
#include <utility>

namespace fps::pvp {
namespace {

using Engine::IO::FS::NativeFileSystem;

// The file URI of a path, or the reason there is none (no UTF-8 form, no absolute form).
std::optional<Engine::IO::Path::Uri> UriOf(const std::filesystem::path& path, std::string& why) {
    auto utf8 = Engine::Os::Files::PathToUtf8(path);
    if (!utf8) {
        why = Engine::Base::Describe(utf8.error());
        return std::nullopt;
    }
    auto uri = Engine::IO::FS::NativeFileUri(utf8.value());
    if (!uri) {
        why = Engine::Base::Describe(uri.error());
        return std::nullopt;
    }
    return std::move(uri.value());
}

// A path for a message, in UTF-8 (path::string() would use the ANSI code page on Windows and may throw).
std::string Display(const std::filesystem::path& path) {
    auto utf8 = Engine::Os::Files::PathToUtf8(path);
    return utf8 ? std::move(utf8.value()) : std::string("(a path with no UTF-8 form)");
}

} // namespace

std::optional<std::vector<char>> ReadNativeFile(const std::filesystem::path& path, NativeReadFailure& failure,
                                                const std::uint64_t maxBytes) {
    failure = {};
    const auto uri = UriOf(path, failure.detail);
    if (!uri) return std::nullopt;
    NativeFileSystem files;
    const auto status = files.Stat(*uri);
    if (!status) {
        failure.detail = Engine::Base::Describe(status.error());
        return std::nullopt;
    }
    if (status.value().sizeBytes > maxBytes) {
        failure.tooLarge = true;
        return std::nullopt;
    }
    Engine::IO::Helpers::ReadAllOptions options;
    options.maxBytes = (std::numeric_limits<std::size_t>::max)();
    auto bytes = Engine::IO::Helpers::ReadAllBytes(files, *uri, options);
    if (!bytes) {
        failure.detail = Engine::Base::Describe(bytes.error());
        return std::nullopt;
    }
    std::vector<char> out(bytes.value().size());
    for (std::size_t index = 0; index < out.size(); ++index) out[index] = static_cast<char>(bytes.value()[index]);
    return out;
}

std::optional<bool> NativePathExists(const std::filesystem::path& path) {
    std::string why;
    const auto uri = UriOf(path, why);
    if (!uri) return std::nullopt;
    NativeFileSystem files;
    const auto exists = files.Exists(*uri);
    if (!exists) return std::nullopt;
    return exists.value();
}

void CreateNativeDirectories(const std::filesystem::path& directory) {
    std::string why;
    if (const auto uri = UriOf(directory, why)) {
        NativeFileSystem files;
        const auto created = files.CreateDirectories(*uri);
        if (created) return;
        why = Engine::Base::Describe(created.error());
    }
    throw std::runtime_error("Cannot create directory: " + Display(directory) + " (" + why + ")");
}

// Buffers the stream's characters and hands them to the engine/io stream on
// sync (flush), when full and on Close. A failure is kept: every later write
// and the Close fail, so the ostream reports it as std::ofstream did.
class NativeTextFile::Buffer final : public std::streambuf {
public:
    explicit Buffer(std::unique_ptr<Engine::IO::Stream::IStream> stream)
        : stream_(std::move(stream)), opened_(stream_ != nullptr) {
        setp(data_, data_ + sizeof data_);
    }
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    // As std::ofstream: true once opened until a write or a close fails, also after a good Close.
    [[nodiscard]] bool Good() const noexcept { return opened_ && !failed_; }
    [[nodiscard]] bool Open() const noexcept { return stream_ != nullptr; }

    // A second Close fails, as std::ofstream's does.
    bool Close() {
        if (!stream_) {
            failed_ = true;
            return false;
        }
        const bool flushed = sync() == 0;
        const bool closed = static_cast<bool>(stream_->Close());
        stream_.reset();
        if (!flushed || !closed) failed_ = true;
        return !failed_;
    }

protected:
    int_type overflow(const int_type character) override {
        if (sync() != 0) return traits_type::eof();
        if (!traits_type::eq_int_type(character, traits_type::eof())) {
            *pptr() = traits_type::to_char_type(character);
            pbump(1);
        }
        return traits_type::not_eof(character);
    }

    int sync() override {
        if (!stream_ || failed_) return -1;
        const auto pending = static_cast<std::size_t>(pptr() - pbase());
        if (pending > 0 && !stream_->Write(pbase(), pending)) failed_ = true;
        setp(data_, data_ + sizeof data_);
        return failed_ ? -1 : 0;
    }

private:
    std::unique_ptr<Engine::IO::Stream::IStream> stream_;
    bool opened_;
    char data_[16384];
    bool failed_ = false;
};

namespace {

std::unique_ptr<Engine::IO::Stream::IStream> OpenText(const std::filesystem::path& path) {
    std::string why;
    const auto uri = UriOf(path, why);
    if (!uri) return nullptr;
    using Engine::IO::Stream::FileOpenMode;
    NativeFileSystem files;
    auto stream = files.Open(*uri, FileOpenMode::Write | FileOpenMode::Truncate | FileOpenMode::CreateIfMissing |
                                       FileOpenMode::Text);
    if (!stream) return nullptr;
    return std::move(stream.value());
}

} // namespace

NativeTextFile::NativeTextFile(const std::filesystem::path& path)
    : buffer_(std::make_unique<Buffer>(OpenText(path))), stream_(buffer_.get()) {
    if (!buffer_->Good()) stream_.setstate(std::ios::badbit);
}

// Flushes what is left, as std::ofstream's destructor does; a failure has no one to report to.
NativeTextFile::~NativeTextFile() {
    if (buffer_ && buffer_->Open()) static_cast<void>(buffer_->Close());
}

NativeTextFile::operator bool() const noexcept { return buffer_->Good() && static_cast<bool>(stream_); }

bool NativeTextFile::Close() {
    const bool good = static_cast<bool>(*this);
    const bool closed = buffer_->Close();
    if (!closed) stream_.setstate(std::ios::badbit);
    return good && closed;
}

} // namespace fps::pvp
