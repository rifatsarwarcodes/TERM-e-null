#include "exam/PackageParser.hpp"

#include "common/Files.hpp"
#include "common/Strings.hpp"
#include "crypto/Sha256.hpp"

#include <cstdio>
#include <functional>
#include <map>

namespace ovc::exam {

namespace {

using strings::trim;

constexpr const char* BlockStart = "<<<";
constexpr const char* BlockEnd = ">>>";

bool isSectionHeader(const std::string& line)
{
    const std::string trimmed = trim(line);
    return trimmed.size() >= 2 && trimmed.front() == '[' && trimmed.back() == ']';
}

std::string sectionName(const std::string& line)
{
    const std::string trimmed = trim(line);
    return strings::toLower(trim(trimmed.substr(1, trimmed.size() - 2)));
}

// Integer policy and exam settings, looked up by key.
using IntSetter = std::function<void(ExamPackage&, int)>;

const std::map<std::string, IntSetter>& policyKeys()
{
    static const std::map<std::string, IntSetter> keys = {
        {"small_paste_chars", [](ExamPackage& p, int v) { p.policy.smallPasteChars = v; }},
        {"small_paste_lines", [](ExamPackage& p, int v) { p.policy.smallPasteLines = v; }},
        {"large_paste_chars", [](ExamPackage& p, int v) { p.policy.largePasteChars = v; }},
        {"large_paste_lines", [](ExamPackage& p, int v) { p.policy.largePasteLines = v; }},
        {"large_paste_points", [](ExamPackage& p, int v) { p.policy.largePastePoints = v; }},
        {"repeat_count", [](ExamPackage& p, int v) { p.policy.repeatCount = v; }},
        {"repeat_window_min", [](ExamPackage& p, int v) { p.policy.repeatWindowMinutes = v; }},
        {"repeat_points", [](ExamPackage& p, int v) { p.policy.repeatPoints = v; }},
        {"focus_return_sec", [](ExamPackage& p, int v) { p.policy.focusReturnSeconds = v; }},
        {"focus_return_points", [](ExamPackage& p, int v) { p.policy.focusReturnPoints = v; }},
        {"unchanged_points", [](ExamPackage& p, int v) { p.policy.unchangedPoints = v; }},
        {"passcode_fail_limit", [](ExamPackage& p, int v) { p.policy.passcodeFailLimit = v; }},
        {"warning_at", [](ExamPackage& p, int v) { p.policy.warningAt = v; }},
        {"flagged_at", [](ExamPackage& p, int v) { p.policy.flaggedAt = v; }},
    };
    return keys;
}

class Parser {
public:
    Parser(const std::string& text, const std::filesystem::path& base)
        : lines_(strings::splitLines(text))
        , base_(base)
    {
    }

    ParseResult run()
    {
        while (index_ < lines_.size()) {
            const std::string& line = lines_[index_];
            if (isSectionHeader(line)) {
                section_ = sectionName(line);
                ++index_;
                if (section_ == "problem")
                    readProblem();
                else if (strings::startsWith(section_, "test"))
                    startTest();
                continue;
            }
            const std::string trimmed = trim(line);
            if (trimmed.empty() || trimmed[0] == '#') {
                ++index_;
                continue;
            }
            readEntry(trimmed);
        }
        return std::move(result_);
    }

private:
    void error(const std::string& message)
    {
        result_.errors.push_back("line " + std::to_string(index_ + 1) + ": " + message);
    }

    void readProblem()
    {
        std::vector<std::string> body;
        while (index_ < lines_.size() && !isSectionHeader(lines_[index_]))
            body.push_back(lines_[index_++]);
        while (!body.empty() && strings::isBlank(body.back()))
            body.pop_back();
        while (!body.empty() && strings::isBlank(body.front()))
            body.erase(body.begin());
        result_.package.problem = strings::join(body, "\n");
    }

    void startTest()
    {
        TestCase test;
        test.name = section_;
        result_.package.tests.push_back(test);
    }

    // Reads "key <<<" ... ">>>" starting at the current line.
    std::string readBlock()
    {
        const std::size_t startLine = index_;
        ++index_;
        std::vector<std::string> body;
        while (index_ < lines_.size() && trim(lines_[index_]) != BlockEnd)
            body.push_back(lines_[index_++]);
        if (index_ >= lines_.size()) {
            index_ = startLine;
            error("text block is missing its closing >>>");
            index_ = lines_.size();
            return strings::join(body, "\n");
        }
        ++index_; // skip ">>>"
        return strings::join(body, "\n");
    }

    void readEntry(const std::string& trimmed)
    {
        if (strings::endsWith(trimmed, BlockStart)) {
            const std::string key = strings::toLower(trim(trimmed.substr(0, trimmed.size() - 3)));
            const std::string block = readBlock();
            applyBlock(key, block);
            return;
        }
        const std::size_t equals = trimmed.find('=');
        if (equals == std::string::npos) {
            error("expected 'key = value'");
            ++index_;
            return;
        }
        const std::string key = strings::toLower(trim(trimmed.substr(0, equals)));
        const std::string value = trim(trimmed.substr(equals + 1));
        applyValue(key, value);
        ++index_;
    }

    void applyBlock(const std::string& key, const std::string& block)
    {
        if (strings::startsWith(section_, "test") && !result_.package.tests.empty()) {
            TestCase& test = result_.package.tests.back();
            if (key == "input") test.input = block;
            else if (key == "output") test.expectedOutput = block;
            else error("unknown test block '" + key + "'");
        } else if (section_ == "allowed" && key == "snippet") {
            result_.package.allowed.push_back({"snippet " + std::to_string(result_.package.allowed.size() + 1), block});
        } else {
            error("text block '" + key + "' is not allowed in [" + section_ + "]");
        }
    }

    void applyValue(const std::string& key, const std::string& value)
    {
        ExamPackage& package = result_.package;
        long long number = 0;
        const bool isNumber = strings::parseInt(value, number);

        if (section_ == "exam") {
            if (key == "code") package.code = value;
            else if (key == "title") package.title = value;
            else if (key == "sessions") package.sessionsRoot = value;
            else if (key == "compiler") package.compiler = value;
            else if (key == "flags") package.compilerFlags = value;
            else if (key == "time_limit" && isNumber) package.timeLimitMinutes = static_cast<int>(number);
            else if (key == "run_time_limit" && isNumber) package.runTimeLimitSeconds = static_cast<int>(number);
            else if (key == "passcode") {
                package.passcode = crypto::PasscodeHash::parse(value);
                if (!package.passcode)
                    error("passcode must be a hash made by 'ovc --setup'");
            } else error("unknown or invalid setting '" + key + "'");
        } else if (strings::startsWith(section_, "test") && !package.tests.empty()) {
            TestCase& test = package.tests.back();
            if (key == "input") test.input = strings::replaceAll(value, "\\n", "\n");
            else if (key == "output") test.expectedOutput = strings::replaceAll(value, "\\n", "\n");
            else error("unknown test setting '" + key + "'");
        } else if (section_ == "allowed" && key == "file") {
            const auto content = files::read(base_ / value);
            if (content)
                package.allowed.push_back({value, *content});
            else
                error("allowed file not found: " + value);
        } else if (section_ == "policy") {
            double ratio = 0;
            if (key == "unchanged_ratio" && strings::parseDouble(value, ratio)) {
                package.policy.unchangedRatio = ratio;
                return;
            }
            const auto& keys = policyKeys();
            const auto found = keys.find(key);
            if (found != keys.end() && isNumber)
                found->second(package, static_cast<int>(number));
            else
                error("unknown or invalid policy value '" + key + "'");
        } else {
            error("'" + key + "' is outside a known section");
        }
    }

    std::vector<std::string> lines_;
    std::filesystem::path base_;
    std::size_t index_ = 0;
    std::string section_;
    ParseResult result_;
};

std::string block(const std::string& key, const std::string& text)
{
    return key + " " + BlockStart + "\n" + text + (text.empty() ? "" : "\n") + BlockEnd + "\n";
}

} // namespace

ParseResult PackageParser::parse(const std::string& text, const std::filesystem::path& baseDirectory)
{
    return Parser(text, baseDirectory).run();
}

std::string PackageParser::serialize(const ExamPackage& package)
{
    const penalty::PenaltyPolicy& p = package.policy;
    std::string out = "# OV_C exam package. Edit with care; see docs/exam-package.md.\n\n[exam]\n";
    out += "code = " + package.code + "\n";
    out += "title = " + package.title + "\n";
    out += "time_limit = " + std::to_string(package.timeLimitMinutes) + "\n";
    out += "sessions = " + package.sessionsRoot + "\n";
    out += "compiler = " + package.compiler + "\n";
    out += "flags = " + package.compilerFlags + "\n";
    out += "run_time_limit = " + std::to_string(package.runTimeLimitSeconds) + "\n";
    if (package.passcode)
        out += "passcode = " + package.passcode->encode() + "\n";

    out += "\n[problem]\n" + package.problem + "\n";

    for (std::size_t i = 0; i < package.tests.size(); ++i) {
        out += "\n[test " + std::to_string(i + 1) + "]\n";
        out += block("input", package.tests[i].input);
        out += block("output", package.tests[i].expectedOutput);
    }

    if (!package.allowed.empty()) {
        out += "\n[allowed]\n";
        for (const auto& snippet : package.allowed)
            out += block("snippet", snippet.text);
    }

    char ratio[16];
    std::snprintf(ratio, sizeof ratio, "%.2f", p.unchangedRatio);
    out += "\n[policy]\n";
    out += "small_paste_chars = " + std::to_string(p.smallPasteChars) + "\n";
    out += "small_paste_lines = " + std::to_string(p.smallPasteLines) + "\n";
    out += "large_paste_chars = " + std::to_string(p.largePasteChars) + "\n";
    out += "large_paste_lines = " + std::to_string(p.largePasteLines) + "\n";
    out += "large_paste_points = " + std::to_string(p.largePastePoints) + "\n";
    out += "repeat_count = " + std::to_string(p.repeatCount) + "\n";
    out += "repeat_window_min = " + std::to_string(p.repeatWindowMinutes) + "\n";
    out += "repeat_points = " + std::to_string(p.repeatPoints) + "\n";
    out += "focus_return_sec = " + std::to_string(p.focusReturnSeconds) + "\n";
    out += "focus_return_points = " + std::to_string(p.focusReturnPoints) + "\n";
    out += "unchanged_ratio = " + std::string(ratio) + "\n";
    out += "unchanged_points = " + std::to_string(p.unchangedPoints) + "\n";
    out += "passcode_fail_limit = " + std::to_string(p.passcodeFailLimit) + "\n";
    out += "warning_at = " + std::to_string(p.warningAt) + "\n";
    out += "flagged_at = " + std::to_string(p.flaggedAt) + "\n";
    return out;
}

ParseResult loadPackage(const std::filesystem::path& path)
{
    const auto text = files::read(path);
    if (!text) {
        ParseResult result;
        result.errors.push_back("cannot read " + path.string());
        return result;
    }
    ParseResult result = PackageParser::parse(*text, path.parent_path());
    result.package.sourceFile = path;
    result.package.sourceHash = crypto::Sha256::hex(*text);
    return result;
}

} // namespace ovc::exam
