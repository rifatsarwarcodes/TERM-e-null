#include "TestFramework.hpp"

#include "common/Strings.hpp"
#include "common/Time.hpp"

using namespace ovc;

TEST_CASE("strings: trim and prefixes")
{
    CHECK_EQ(strings::trim("  hello \n"), std::string("hello"));
    CHECK_EQ(strings::trimRight("abc  \t"), std::string("abc"));
    CHECK(strings::startsWith("--examiner", "--"));
    CHECK(!strings::startsWith("-", "--"));
    CHECK(strings::endsWith("main.cpp", ".cpp"));
}

TEST_CASE("strings: split keeps empty parts")
{
    const auto parts = strings::split("a,,b", ',');
    CHECK_EQ(parts.size(), std::size_t(3));
    CHECK_EQ(parts[1], std::string(""));
}

TEST_CASE("strings: splitLines handles CRLF and trailing newline")
{
    const auto lines = strings::splitLines("one\r\ntwo\nthree\n");
    CHECK_EQ(lines.size(), std::size_t(3));
    CHECK_EQ(lines[0], std::string("one"));
    CHECK_EQ(lines[2], std::string("three"));
    CHECK_EQ(strings::countLines("a\nb\n"), std::size_t(2));
    CHECK_EQ(strings::countLines("a\nb"), std::size_t(2));
}

TEST_CASE("strings: number parsing rejects junk")
{
    std::uint64_t value = 0;
    CHECK(strings::parseUnsigned("1240", value));
    CHECK_EQ(value, std::uint64_t(1240));
    CHECK(!strings::parseUnsigned("12a", value));
    CHECK(!strings::parseUnsigned("", value));
    long long signedValue = 0;
    CHECK(strings::parseInt("-15", signedValue));
    CHECK_EQ(signedValue, -15LL);
    double ratio = 0;
    CHECK(strings::parseDouble("0.80", ratio));
    CHECK(ratio > 0.79 && ratio < 0.81);
}

TEST_CASE("strings: display width counts code points")
{
    CHECK_EQ(strings::displayWidth("abc"), std::size_t(3));
    CHECK_EQ(strings::displayWidth("┌─┐"), std::size_t(3));
    std::string text = "a─";
    strings::popCodePoint(text);
    CHECK_EQ(text, std::string("a"));
}

TEST_CASE("time: duration formatting")
{
    CHECK_EQ(timeutil::formatDuration(65 * 1000), std::string("01:05"));
    CHECK_EQ(timeutil::formatDuration(3725 * 1000), std::string("1:02:05"));
    CHECK_EQ(timeutil::formatDuration(-5), std::string("00:00"));
    CHECK_EQ(timeutil::formatSpan(240 * 1000), std::string("4 min"));
}
