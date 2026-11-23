#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "AssertTestSupport.hpp"
#include "engine/base/Error.hpp"
#include "engine/os/OSError.hpp"

#include <string>

using namespace GYO::Engine::OS;

TEST_CASE("OS error codes start at 1 and have names") {
    CHECK(static_cast<int>(OSErrorCode::NotFound) == 1);
    CHECK(std::string(ToString(OSErrorCode::NotFound)) == "NotFound");
    CHECK(std::string(ToString(OSErrorCode::InvalidEncoding)) == "InvalidEncoding");
    CHECK(std::string(ToString(OSErrorCode::Unsupported)) == "Unsupported");
    CHECK(std::string(ToString(OSErrorCode::InvalidAddress)) == "InvalidAddress");
    CHECK(std::string(ToString(OSErrorCode::AddressInUse)) == "AddressInUse");
    CHECK(std::string(ToString(OSErrorCode::Unreachable)) == "Unreachable");
    CHECK(std::string(ToString(OSErrorCode::WouldBlock)) == "WouldBlock");
}

TEST_CASE("OS error detail keeps the native code in one fixed form") {
    CHECK(NativeErrorDetail({NativeErrorDomain::Errno, 2}) == "errno=2");
    CHECK(NativeErrorDetail({NativeErrorDomain::Win32, 3}) == "win32=3");
    CHECK(NativeErrorDetail({NativeErrorDomain::Win32, 206}, 271) == "win32=206; path_bytes=271");
}

TEST_CASE("OS error converted at a module boundary keeps its code and native detail") {
    const OSError error = MakeOSError(OSErrorCode::NotFound, "cannot open file", {NativeErrorDomain::Errno, 2}, 12);
    CHECK(error.code == OSErrorCode::NotFound);
    CHECK(error.message == "cannot open file");
    CHECK(error.detail == "errno=2; path_bytes=12");
    // An outer module records the cause with Base::CauseDetail (error-handling.md rule 5).
    CHECK(GYO::Engine::Base::CauseDetail(error, "C:/a/b") == "C:/a/b; NotFound: errno=2; path_bytes=12");
}

TEST_CASE("OS error has no zero code") {
    GYO_CHECK_ASSERTS(OSError::Make(static_cast<OSErrorCode>(0), "zero"));
}
