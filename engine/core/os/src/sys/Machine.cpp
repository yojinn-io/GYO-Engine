// Machine facts on macOS and Linux.
#include "engine/os/Machine.hpp"

#include "../common/Paths.hpp"

#include <array>
#include <cerrno>
#include <cstring>
#include <string>

#include <unistd.h>

namespace GYO::Engine::OS::Machine {

Base::Result<std::string, OSError> HostName() {
    // POSIX caps a host name at HOST_NAME_MAX (64 on Linux, 255 on macOS); one more byte keeps the end even where
    // gethostname truncates without one.
    std::array<char, 257> buffer{};
    if (::gethostname(buffer.data(), buffer.size() - 1) != 0) {
        const int error = errno;
        return Base::Err(
            MakeOSError(OSErrorCode::IOFailed, "cannot read the host name", {NativeErrorDomain::Errno, error}));
    }
    std::string name(buffer.data(), ::strnlen(buffer.data(), buffer.size()));
    if (!Detail::IsValidUtf8(name)) {
        return Base::Err(Detail::RefusedArgument(OSErrorCode::InvalidEncoding, "host name is not valid UTF-8",
                                                 name.size()));
    }
    return name;
}

} // namespace GYO::Engine::OS::Machine
