#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "AssertTestSupport.hpp"
#include "engine/base/Error.hpp"
#include "engine/os/OsError.hpp"

#include <string>

using namespace Engine::Os;

TEST_CASE("Os error codes start at 1 and have names") {
    CHECK(static_cast<int>(OsErrorCode::NotFound) == 1);
    CHECK(std::string(ToString(OsErrorCode::NotFound)) == "NotFound");
    CHECK(std::string(ToString(OsErrorCode::InvalidEncoding)) == "InvalidEncoding");
    CHECK(std::string(ToString(OsErrorCode::Unsupported)) == "Unsupported");
}

TEST_CASE("Os error detail keeps the native code in one fixed form") {
    CHECK(NativeErrorDetail({NativeErrorDomain::Errno, 2}) == "errno=2");
    CHECK(NativeErrorDetail({NativeErrorDomain::Win32, 3}) == "win32=3");
    CHECK(NativeErrorDetail({NativeErrorDomain::Win32, 206}, 271) == "win32=206; path_bytes=271");
}

TEST_CASE("Os error converted at a module boundary keeps its code and native detail") {
    const OsError error = MakeOsError(OsErrorCode::NotFound, "cannot open file", {NativeErrorDomain::Errno, 2}, 12);
    CHECK(error.code == OsErrorCode::NotFound);
    CHECK(error.message == "cannot open file");
    CHECK(error.detail == "errno=2; path_bytes=12");
    // An outer module records the cause with Base::CauseDetail (error-handling.md rule 5).
    CHECK(Engine::Base::CauseDetail(error, "C:/a/b") == "C:/a/b; NotFound: errno=2; path_bytes=12");
}

TEST_CASE("Os error has no zero code") {
    GYO_CHECK_ASSERTS(OsError::Make(static_cast<OsErrorCode>(0), "zero"));
}
