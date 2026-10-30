#include <doctest/doctest.h>

#include "RetroFPS/Pvp/AddressText.hpp"

namespace {
using fps::pvp::FilterAddressText;
} // namespace

TEST_CASE("The address filter keeps the characters of a Gateway address") {
    CHECK(FilterAddressText("127.0.0.1:8080") == "127.0.0.1:8080");
    CHECK(FilterAddressText("[::1]:8080") == "[::1]:8080");
    CHECK(FilterAddressText("gateway-01.lan:80") == "gateway-01.lan:80");
    CHECK_MESSAGE(FilterAddressText("127 0/0\t1!") == "127001", "other ASCII is dropped");
}

// v8-tool-ui-ime-observe, Windows log 2026-10-30: in hiragana mode the
// Japanese IME commits full-width forms (U+FF0E, U+FF10, U+FF11, U+FF1A) and
// the ideographic full stop (U+3002); the filter used to drop them, so ':'
// could not be typed.
TEST_CASE("The address filter turns what an input method commits into ASCII") {
    CHECK_MESSAGE(FilterAddressText("\xEF\xBC\x91\xEF\xBC\x92\xEF\xBC\x97\xEF\xBC\x8E\xEF\xBC\x90\xEF\xBC\x8E\xEF\xBC\x90"
                                    "\xEF\xBC\x8E\xEF\xBC\x91\xEF\xBC\x9A\xEF\xBC\x98\xEF\xBC\x90\xEF\xBC\x98\xEF\xBC\x90") ==
                      "127.0.0.1:8080",
                  "full-width digits, full stops and colon become ASCII");
    CHECK_MESSAGE(FilterAddressText("127\xE3\x80\x82" "0") == "127.0", "the ideographic full stop becomes '.'");
    CHECK_MESSAGE(FilterAddressText("\xEF\xBC\xBB\xEF\xBC\x9A\xEF\xBC\x9A\xEF\xBC\x91\xEF\xBC\xBD") == "[::1]",
                  "full-width brackets become ASCII");
    CHECK_MESSAGE(FilterAddressText("\xEF\xBD\x87\xEF\xBC\xA1\xEF\xBC\x8D") == "gA-", "full-width letters and hyphen become ASCII");
    CHECK_MESSAGE(FilterAddressText("\xEF\xBC\x81\xE3\x81\x82") == "", "a converted character outside the set and kana are dropped");
    CHECK_MESSAGE(FilterAddressText("1\xEF\xBC") == "1", "a cut multi-byte sequence is dropped");
    CHECK_MESSAGE(FilterAddressText("\xEF" "12") == "12", "a broken sequence drops only its own byte");
}
