#include "ui/CppHighlighter.hpp"

#include "ui/Theme.hpp"

#include <cctype>
#include <string_view>

namespace ovc::ui {

namespace {

bool isKeyword(std::string_view word)
{
    static const char* const keywords[] = {
        "alignas", "auto", "bool", "break", "case", "catch", "char", "class", "const", "constexpr",
        "continue", "default", "delete", "do", "double", "else", "enum", "explicit", "extern", "false",
        "float", "for", "friend", "if", "inline", "int", "long", "namespace", "new", "noexcept",
        "nullptr", "operator", "override", "private", "protected", "public", "return", "short",
        "signed", "sizeof", "static", "struct", "switch", "template", "this", "throw", "true", "try",
        "typedef", "typename", "union", "unsigned", "using", "virtual", "void", "volatile", "while",
    };
    for (const char* keyword : keywords)
        if (word == keyword)
            return true;
    return false;
}

bool isIdentifierChar(char c)
{
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

} // namespace

std::vector<HighlightSpan> CppHighlighter::highlight(const std::string& line, bool& inBlockComment) const
{
    std::vector<HighlightSpan> spans;
    std::size_t i = 0;
    const std::size_t n = line.size();

    if (inBlockComment) {
        const std::size_t end = line.find("*/");
        const std::size_t stop = end == std::string::npos ? n : end + 2;
        spans.push_back({0, stop, theme::comment()});
        if (end == std::string::npos)
            return spans;
        inBlockComment = false;
        i = stop;
    }

    // Preprocessor lines are coloured as a whole (comments after them aside).
    const std::size_t firstChar = line.find_first_not_of(" \t");
    if (i == 0 && firstChar != std::string::npos && line[firstChar] == '#') {
        const std::size_t comment = line.find("//");
        const std::size_t stop = comment == std::string::npos ? n : comment;
        spans.push_back({firstChar, stop - firstChar, theme::preprocessor()});
        if (comment != std::string::npos)
            spans.push_back({comment, n - comment, theme::comment()});
        return spans;
    }

    while (i < n) {
        const char c = line[i];
        if (c == '/' && i + 1 < n && line[i + 1] == '/') {
            spans.push_back({i, n - i, theme::comment()});
            break;
        }
        if (c == '/' && i + 1 < n && line[i + 1] == '*') {
            const std::size_t end = line.find("*/", i + 2);
            if (end == std::string::npos) {
                spans.push_back({i, n - i, theme::comment()});
                inBlockComment = true;
                break;
            }
            spans.push_back({i, end + 2 - i, theme::comment()});
            i = end + 2;
            continue;
        }
        if (c == '"' || c == '\'') {
            std::size_t j = i + 1;
            while (j < n && line[j] != c) {
                if (line[j] == '\\')
                    ++j;
                ++j;
            }
            const std::size_t stop = j < n ? j + 1 : n;
            spans.push_back({i, stop - i, theme::stringLiteral()});
            i = stop;
            continue;
        }
        if (std::isdigit(static_cast<unsigned char>(c)) && (i == 0 || !isIdentifierChar(line[i - 1]))) {
            std::size_t j = i;
            while (j < n && (isIdentifierChar(line[j]) || line[j] == '.'))
                ++j;
            spans.push_back({i, j - i, theme::number()});
            i = j;
            continue;
        }
        if (isIdentifierChar(c)) {
            std::size_t j = i;
            while (j < n && isIdentifierChar(line[j]))
                ++j;
            if (isKeyword(std::string_view(line).substr(i, j - i)))
                spans.push_back({i, j - i, theme::keyword()});
            i = j;
            continue;
        }
        ++i;
    }
    return spans;
}

} // namespace ovc::ui
