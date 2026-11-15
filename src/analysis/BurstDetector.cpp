#include "analysis/BurstDetector.hpp"

#include <variant>

namespace ovc::analysis {

using platform::InputEvent;
using platform::KeyCode;
using platform::KeyEvent;
using platform::PasteEvent;

namespace {

// The text a key contributes to a pasted block, or false if the key would
// never appear inside pasted text.
bool textOf(const InputEvent& event, std::string& text)
{
    const auto* key = std::get_if<KeyEvent>(&event);
    if (key == nullptr)
        return false;
    if (key->isPrintable()) {
        text = key->text;
        return true;
    }
    if (key->is(KeyCode::Enter)) {
        text = "\n";
        return true;
    }
    if (key->is(KeyCode::Tab)) {
        text = "\t";
        return true;
    }
    return false;
}

bool allSame(const std::vector<std::string>& pieces)
{
    for (const auto& piece : pieces)
        if (piece != pieces.front())
            return false;
    return true;
}

} // namespace

BurstDetector::BurstDetector(std::size_t minimumKeys)
    : minimumKeys_(minimumKeys)
{
}

std::vector<InputEvent> BurstDetector::process(std::vector<InputEvent> batch) const
{
    std::vector<InputEvent> result;
    result.reserve(batch.size());

    std::size_t i = 0;
    while (i < batch.size()) {
        std::vector<std::string> pieces;
        std::size_t end = i;
        std::string piece;
        while (end < batch.size() && textOf(batch[end], piece)) {
            pieces.push_back(piece);
            ++end;
        }

        if (pieces.size() >= minimumKeys_ && !allSame(pieces)) {
            std::string text;
            for (const auto& p : pieces)
                text += p;
            result.emplace_back(PasteEvent{text, false});
            i = end;
        } else if (end > i) {
            for (std::size_t j = i; j < end; ++j)
                result.push_back(std::move(batch[j]));
            i = end;
        } else {
            result.push_back(std::move(batch[i]));
            ++i;
        }
    }
    return result;
}

} // namespace ovc::analysis
