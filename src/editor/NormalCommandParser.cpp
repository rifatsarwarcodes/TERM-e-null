#include "editor/NormalCommandParser.hpp"

#include <cstring>

namespace ovc::editor {

namespace {

bool isMotionKey(char key)
{
    return key != 0 && std::strchr("hjklwbe0^$G", key) != nullptr;
}

bool isOperatorKey(char key)
{
    return key == 'd' || key == 'c' || key == 'y';
}

bool isActionKey(char key)
{
    return key != 0 && std::strchr("xXpPiaAIoOunNDC:/", key) != nullptr;
}

} // namespace

void NormalCommandParser::reset()
{
    state_ = State::Start;
    firstCount_.clear();
    secondCount_.clear();
    command_ = NormalCommand{};
    keys_.clear();
}

std::size_t NormalCommandParser::takeCount(std::string& digits) const
{
    if (digits.empty())
        return 1;
    std::size_t value = 0;
    for (char c : digits)
        value = value * 10 + static_cast<std::size_t>(c - '0');
    return value == 0 ? 1 : value;
}

ParseStatus NormalCommandParser::finishMotion(const std::string& motion)
{
    command_.motion = motion;
    command_.hasCount = !firstCount_.empty() || !secondCount_.empty();
    command_.count = takeCount(firstCount_) * takeCount(secondCount_);
    return ParseStatus::Complete;
}

ParseStatus NormalCommandParser::feed(char key)
{
    keys_.push_back(key);
    switch (state_) {
    case State::Start:
        if ((key >= '1' && key <= '9') || (key == '0' && !firstCount_.empty())) {
            firstCount_.push_back(key);
            return ParseStatus::Pending;
        }
        if (key == 'g') {
            state_ = State::AfterG;
            return ParseStatus::Pending;
        }
        if (isOperatorKey(key)) {
            command_.op = key;
            state_ = State::AfterOperator;
            return ParseStatus::Pending;
        }
        if (isMotionKey(key))
            return finishMotion(std::string(1, key));
        if (isActionKey(key)) {
            command_.action = key;
            command_.hasCount = !firstCount_.empty();
            command_.count = takeCount(firstCount_);
            return ParseStatus::Complete;
        }
        return ParseStatus::Invalid;

    case State::AfterOperator:
        if ((key >= '1' && key <= '9') || (key == '0' && !secondCount_.empty())) {
            secondCount_.push_back(key);
            return ParseStatus::Pending;
        }
        if (key == command_.op)
            return finishMotion("line"); // dd, cc, yy
        if (key == 'g') {
            state_ = State::AfterG;
            return ParseStatus::Pending;
        }
        if (isMotionKey(key))
            return finishMotion(std::string(1, key));
        return ParseStatus::Invalid;

    case State::AfterG:
        if (key == 'g')
            return finishMotion("gg");
        return ParseStatus::Invalid;
    }
    return ParseStatus::Invalid;
}

} // namespace ovc::editor
