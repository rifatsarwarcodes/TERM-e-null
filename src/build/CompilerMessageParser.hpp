#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace ovc::build {

enum class Severity { Error, Warning, Note };

struct CompilerMessage {
    std::string file;
    int line = 0;
    int column = 0;
    Severity severity = Severity::Error;
    std::string text;
};

// Reads g++/clang++ output line by line and picks out messages of the form
//   path/to/main.cpp:12:5: error: expected ';' before 'return'
// Windows paths with a drive letter ("C:\lab\main.cpp:12:5: ...") work too.
class CompilerMessageParser {
public:
    static std::vector<CompilerMessage> parse(std::string_view output);
};

const char* severityName(Severity severity);

} // namespace ovc::build
