#include "engine/io/helpers/WriteFileAtomically.hpp"

#include <chrono>
#include <random>
#include <string>

namespace Engine::IO::Helpers {

    namespace {

        // The suffix of the atomic save this helper replaced (os-subsystem D56): a time and a random number, so
        // that two writers of one file do not share a temporary file.
        std::string TemporarySuffix() {
            const auto time = static_cast<unsigned long long>(
                std::chrono::steady_clock::now().time_since_epoch().count());
            std::random_device random;
            return ".tmp." + std::to_string(time) + "." + std::to_string(random());
        }

    } // namespace

    IoResult<void> WriteFileAtomically(FS::IFileSystem& fs, const Path::Uri& uri, Base::ConstSpan<std::byte> data) {
        Path::Uri temporary = uri;
        temporary.path = Path::Path::FromNormalized(uri.path.Str() + TemporarySuffix());
        auto opened = OpenWriteTruncate(fs, temporary);
        if (!opened) return Base::Err(std::move(opened.error()));
        auto written = WriteAllToStream(*opened.value(), data);
        auto closed = opened.value()->Close();
        opened.value().reset();
        if (!written || !closed) {
            (void)fs.Remove(temporary, {});
            return Base::Err(!written ? std::move(written.error()) : std::move(closed.error()));
        }
        auto moved = fs.Move(temporary, uri);
        if (!moved) {
            (void)fs.Remove(temporary, {});
            return Base::Err(std::move(moved.error()));
        }
        return {};
    }

} // namespace Engine::IO::Helpers
