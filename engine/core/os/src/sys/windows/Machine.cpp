// Machine facts on Windows.
#include "engine/os/Machine.hpp"

#include "Wide.hpp"

#include <cstdint>
#include <string>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace GYO::Engine::OS::Machine {

Base::Result<std::string, OSError> HostName() {
    // The DNS host name of this machine, not the cluster's virtual name (ComputerNamePhysicalDnsHostname, as Go's
    // os.Hostname asks). A buffer too small fails with ERROR_MORE_DATA and the size it needs.
    DWORD size = 64;
    for (;;) {
        std::wstring buffer(size, L'\0');
        DWORD length = size;
        if (::GetComputerNameExW(ComputerNamePhysicalDnsHostname, buffer.data(), &length)) {
            buffer.resize(length);
            return Detail::WideToUtf8(buffer);
        }
        const DWORD error = ::GetLastError();
        if (error != ERROR_MORE_DATA || length <= size) {
            return Base::Err(MakeOSError(OSErrorCode::IOFailed, "cannot read the host name",
                                         {NativeErrorDomain::Win32, static_cast<std::int64_t>(error)}));
        }
        size = length;
    }
}

} // namespace GYO::Engine::OS::Machine
