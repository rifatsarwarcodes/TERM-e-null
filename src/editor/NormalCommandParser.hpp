#pragma once

#include <cstddef>
#include <string>

namespace ovc::editor {

// A complete Normal-mode command such as "3dw", "dd", "5j", "gg" or "p".
struct NormalCommand {
    std::size_t count = 1;  // count before and after the operator, multiplied
    bool hasCount = false;
    char op = 0;            // 'd', 'c', 'y' or 0
    std::string motion;     // "h" "j" "k" "l" "w" "b" "e" "0" "^" "$" "G" "gg", or "line" for dd/cc/yy
    char action = 0;        // stand-alone command such as 'x', 'p', 'i', 'u', ':' (0 if a motion)
};

enum class ParseStatus { Pending, Complete, Invalid };

// State machine for the grammar  [count] ( action | motion | operator [count] (motion | operator) )
// Keys are fed one at a time; after Complete or Invalid the parser must be
// reset before the next command.
class NormalCommandParser {
public:
    ParseStatus feed(char key);
    const NormalCommand& command() const { return command_; }
    void reset();

    // Text typed so far, e.g. "3d", shown in the status bar.
    const std::string& pendingKeys() const { return keys_; }

private:
    enum class State { Start, AfterOperator, AfterG };

    ParseStatus finishMotion(const std::string& motion);
    std::size_t takeCount(std::string& digits) const;

    State state_ = State::Start;
    std::string firstCount_;
    std::string secondCount_;
    NormalCommand command_;
    std::string keys_;
};

} // namespace ovc::editor
