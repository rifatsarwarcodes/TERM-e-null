#include "TestFramework.hpp"

#include <cstring>

// Runs every registered test case. An optional argument filters test names:
//   ovc_tests sha256
int main(int argc, char** argv)
{
    const char* filter = argc > 1 ? argv[1] : nullptr;
    int ran = 0;
    int failedCases = 0;

    for (const auto& testCase : test::registry()) {
        if (filter && std::strstr(testCase.name, filter) == nullptr)
            continue;
        const int before = test::failureCount();
        try {
            testCase.body();
        } catch (const std::exception& error) {
            test::reportFailure(__FILE__, __LINE__, std::string("uncaught exception: ") + error.what());
        }
        ++ran;
        if (test::failureCount() != before) {
            ++failedCases;
            std::cerr << "FAILED: " << testCase.name << "\n";
        }
    }

    std::cout << ran - failedCases << "/" << ran << " test cases passed\n";
    return failedCases == 0 ? 0 : 1;
}
