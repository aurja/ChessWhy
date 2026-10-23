#include "chesswhy/digest.hpp"

#include <cstddef>
#include <span>
#include <string>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

namespace {

[[nodiscard]] std::string digest(std::string_view text) {
    return chesswhy::sha256_hex(std::as_bytes(std::span(text.data(), text.size())));
}

} // namespace

TEST_CASE("SHA-256 matches FIPS 180-4 vectors", "[unit]") {
    CHECK(digest("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK(digest("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(digest("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") ==
          "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");

    const std::string million_a(1'000'000, 'a');
    CHECK(digest(million_a) == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}
