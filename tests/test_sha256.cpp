#include "TestFramework.hpp"

#include "crypto/Sha256.hpp"

#include <string>

using ovc::crypto::Sha256;

// Test values from the NIST examples for FIPS 180-4.

TEST_CASE("sha256: empty input")
{
    CHECK_EQ(Sha256::hex(""),
        std::string("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
}

TEST_CASE("sha256: abc")
{
    CHECK_EQ(Sha256::hex("abc"),
        std::string("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
}

TEST_CASE("sha256: two block message")
{
    CHECK_EQ(Sha256::hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
        std::string("248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"));
}

TEST_CASE("sha256: one million a's fed in pieces")
{
    Sha256 hasher;
    const std::string chunk(1000, 'a');
    for (int i = 0; i < 1000; ++i)
        hasher.update(chunk);
    CHECK_EQ(hasher.hexDigest(),
        std::string("cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"));
}

TEST_CASE("sha256: reset allows reuse")
{
    Sha256 hasher;
    hasher.update("something else");
    hasher.hexDigest();
    hasher.reset();
    hasher.update("abc");
    CHECK_EQ(hasher.hexDigest(),
        std::string("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
}
