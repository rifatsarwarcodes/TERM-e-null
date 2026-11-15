#pragma once

// A very small assert-style test framework.
//
//   TEST_CASE("gap buffer inserts in the middle") {
//       CHECK(buffer.size() == 3);
//       CHECK_EQ(buffer.toString(), "abc");
//   }
//
// Every TEST_CASE registers itself; tests/test_main.cpp runs them all.

#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace test {

struct Case {
    const char* name;
    std::function<void()> body;
};

inline std::vector<Case>& registry()
{
    static std::vector<Case> cases;
    return cases;
}

struct Registrar {
    Registrar(const char* name, std::function<void()> body)
    {
        registry().push_back({name, std::move(body)});
    }
};

inline int& failureCount()
{
    static int failures = 0;
    return failures;
}

inline void reportFailure(const char* file, int line, const std::string& message)
{
    ++failureCount();
    std::cerr << "    " << file << ":" << line << ": " << message << "\n";
}

template <typename T>
std::string describe(const T& value)
{
    std::ostringstream out;
    out << value;
    return out.str();
}

inline std::string describe(const std::string& value) { return "\"" + value + "\""; }
inline std::string describe(const char* value) { return "\"" + std::string(value) + "\""; }
inline std::string describe(bool value) { return value ? "true" : "false"; }

} // namespace test

#define TEST_CONCAT_INNER(a, b) a##b
#define TEST_CONCAT(a, b) TEST_CONCAT_INNER(a, b)

#define TEST_CASE(name)                                                         \
    static void TEST_CONCAT(test_body_, __LINE__)();                            \
    static test::Registrar TEST_CONCAT(test_registrar_, __LINE__)(              \
        name, TEST_CONCAT(test_body_, __LINE__));                               \
    static void TEST_CONCAT(test_body_, __LINE__)()

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition))                                                       \
            test::reportFailure(__FILE__, __LINE__, "CHECK(" #condition ")");   \
    } while (false)

#define CHECK_EQ(actual, expected)                                              \
    do {                                                                        \
        const auto& test_actual_ = (actual);                                    \
        const auto& test_expected_ = (expected);                                \
        if (!(test_actual_ == test_expected_))                                  \
            test::reportFailure(__FILE__, __LINE__,                             \
                "CHECK_EQ(" #actual ", " #expected "): got "                    \
                    + test::describe(test_actual_) + ", expected "              \
                    + test::describe(test_expected_));                          \
    } while (false)
