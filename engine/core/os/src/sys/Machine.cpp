// Machine facts on macOS and Linux.
#include "engine/os/Machine.hpp"

#include "../common/Paths.hpp"

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <string>

#include <unistd.h>

namespace GYO::Engine::OS::Machine {

namespace {

// The longest host name POSIX systems give (HOST_NAME_MAX: 64 on Linux, 255 on macOS), with its end.
constexpr std::size_t kHostNameBytes = 256;

} // namespace

Base::Result<std::string, OSError> HostName() {
    // One more byte than gethostname is given keeps an end even where it truncates without one.
    std::array<char, kHostNameBytes + 1> buffer{};
    if (::gethostname(buffer.data(), buffer.size() - 1) != 0) {
        const int error = errno;
        return Base::Err(
            MakeOSError(OSErrorCode::IOFailed, "cannot read the host name", {NativeErrorDomain::Errno, error}));
    }
    std::string name(buffer.data(), ::strnlen(buffer.data(), buffer.size()));
    if (!Detail::IsValidUtf8(name)) {
        return Base::Err(OSError::Make(OSErrorCode::InvalidEncoding, "host name is not valid UTF-8"));
    }
    return name;
}

} // namespace GYO::Engine::OS::Machine
