#include "platform/InputDecoder.hpp"

#include "common/Strings.hpp"

#include <algorithm>

namespace ovc::platform {

namespace {

constexpr std::string_view PasteEnd = "\x1b[201~";

KeyEvent withModifiers(KeyEvent key, int modifierParam)
{
    // xterm encodes modifiers as 1 + (shift ? 1 : 0) + (alt ? 2 : 0) + (ctrl ? 4 : 0).
    if (modifierParam > 1) {
        const int bits = modifierParam - 1;
        key.shift = (bits & 1) != 0;
        key.alt = (bits & 2) != 0;
        key.ctrl = (bits & 4) != 0;
    }
    return key;
}

KeyEvent keyForTilde(int number)
{
    switch (number) {
    case 1:
    case 7: return KeyEvent::special(KeyCode::Home);
    case 2: return KeyEvent::special(KeyCode::Insert);
    case 3: return KeyEvent::special(KeyCode::Delete);
    case 4:
    case 8: return KeyEvent::special(KeyCode::End);
    case 5: return KeyEvent::special(KeyCode::PageUp);
    case 6: return KeyEvent::special(KeyCode::PageDown);
    case 11: return KeyEvent::special(KeyCode::F1);
    case 12: return KeyEvent::special(KeyCode::F2);
    case 13: return KeyEvent::special(KeyCode::F3);
    case 14: return KeyEvent::special(KeyCode::F4);
    case 15: return KeyEvent::special(KeyCode::F5);
    case 17: return KeyEvent::special(KeyCode::F6);
    case 18: return KeyEvent::special(KeyCode::F7);
    case 19: return KeyEvent::special(KeyCode::F8);
    case 20: return KeyEvent::special(KeyCode::F9);
    case 21: return KeyEvent::special(KeyCode::F10);
    case 23: return KeyEvent::special(KeyCode::F11);
    case 24: return KeyEvent::special(KeyCode::F12);
    default: return KeyEvent::special(KeyCode::Escape);
    }
}

bool keyForLetter(char final, KeyEvent& key)
{
    switch (final) {
    case 'A': key = KeyEvent::special(KeyCode::Up); return true;
    case 'B': key = KeyEvent::special(KeyCode::Down); return true;
    case 'C': key = KeyEvent::special(KeyCode::Right); return true;
    case 'D': key = KeyEvent::special(KeyCode::Left); return true;
    case 'H': key = KeyEvent::special(KeyCode::Home); return true;
    case 'F': key = KeyEvent::special(KeyCode::End); return true;
    case 'P': key = KeyEvent::special(KeyCode::F1); return true;
    case 'Q': key = KeyEvent::special(KeyCode::F2); return true;
    case 'R': key = KeyEvent::special(KeyCode::F3); return true;
    case 'S': key = KeyEvent::special(KeyCode::F4); return true;
    case 'Z': key = KeyEvent::special(KeyCode::BackTab); return true;
    default: return false;
    }
}

std::size_t utf8Length(unsigned char lead)
{
    if (lead < 0x80) return 1;
    if ((lead & 0xE0) == 0xC0) return 2;
    if ((lead & 0xF0) == 0xE0) return 3;
    if ((lead & 0xF8) == 0xF0) return 4;
    return 1; // invalid lead byte: treat as a single byte
}

std::string normaliseNewlines(const std::string& text)
{
    std::string result;
    result.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\r') {
            result.push_back('\n');
            if (i + 1 < text.size() && text[i + 1] == '\n')
                ++i;
        } else {
            result.push_back(text[i]);
        }
    }
    return result;
}

} // namespace

void InputDecoder::feed(std::string_view bytes, std::vector<InputEvent>& out)
{
    pending_.append(bytes.data(), bytes.size());
    while (!pending_.empty()) {
        if (inPaste_) {
            consumePaste(out);
            if (inPaste_)
                return; // waiting for the end marker
            continue;
        }
        const std::size_t used = decodeOne(out);
        if (used == 0)
            return;
        pending_.erase(0, used);
    }
}

void InputDecoder::flushPending(std::vector<InputEvent>& out)
{
    if (inPaste_)
        return;
    if (pending_ == "\x1b") {
        out.emplace_back(KeyEvent::special(KeyCode::Escape));
    } else if (!pending_.empty()) {
        // An unknown or broken sequence: report its bytes as plain characters.
        for (char c : pending_)
            if (static_cast<unsigned char>(c) >= 0x20)
                out.emplace_back(KeyEvent::character(std::string(1, c)));
    }
    pending_.clear();
}

std::size_t InputDecoder::decodeOne(std::vector<InputEvent>& out)
{
    const auto byte = static_cast<unsigned char>(pending_[0]);
    switch (byte) {
    case 0x1B: return decodeEscape(out);
    case '\r':
    case '\n':
        out.emplace_back(KeyEvent::special(KeyCode::Enter));
        // Treat CR LF as a single Enter.
        return (byte == '\r' && pending_.size() > 1 && pending_[1] == '\n') ? 2 : 1;
    case '\t': out.emplace_back(KeyEvent::special(KeyCode::Tab)); return 1;
    case 0x7F:
    case 0x08: out.emplace_back(KeyEvent::special(KeyCode::Backspace)); return 1;
    case 0x00: out.emplace_back(KeyEvent::control(' ')); return 1;
    default: break;
    }
    if (byte < 0x20) {
        out.emplace_back(KeyEvent::control(static_cast<char>('a' + byte - 1)));
        return 1;
    }
    return decodeUtf8(out);
}

std::size_t InputDecoder::decodeUtf8(std::vector<InputEvent>& out)
{
    const std::size_t length = utf8Length(static_cast<unsigned char>(pending_[0]));
    if (pending_.size() < length)
        return 0;
    out.emplace_back(KeyEvent::character(pending_.substr(0, length)));
    return length;
}

std::size_t InputDecoder::decodeEscape(std::vector<InputEvent>& out)
{
    if (pending_.size() < 2)
        return 0; // a lone ESC, or the start of a sequence: wait
    const char second = pending_[1];

    if (second == '[')
        return decodeCsi(out);

    if (second == 'O') {
        if (pending_.size() < 3)
            return 0;
        KeyEvent key;
        if (keyForLetter(pending_[2], key))
            out.emplace_back(key);
        return 3;
    }

    if (second == 0x1B) {
        out.emplace_back(KeyEvent::special(KeyCode::Escape));
        return 1;
    }

    // ESC followed by a character is Alt + that character.
    const std::size_t length = utf8Length(static_cast<unsigned char>(second));
    if (pending_.size() < 1 + length)
        return 0;
    KeyEvent key = KeyEvent::character(pending_.substr(1, length));
    key.alt = true;
    out.emplace_back(key);
    return 1 + length;
}

std::size_t InputDecoder::decodeCsi(std::vector<InputEvent>& out)
{
    // ESC [ parameters (digits and ';') then one final byte in 0x40-0x7E.
    std::size_t index = 2;
    while (index < pending_.size()) {
        const auto c = static_cast<unsigned char>(pending_[index]);
        if (c >= 0x40 && c <= 0x7E)
            break;
        ++index;
    }
    if (index >= pending_.size())
        return 0;

    const char final = pending_[index];
    const std::string params = pending_.substr(2, index - 2);
    const std::size_t used = index + 1;
    const auto fields = strings::split(params, ';');
    long long first = 0;
    long long modifier = 1;
    if (!fields.empty() && !fields[0].empty())
        strings::parseInt(fields[0], first);
    if (fields.size() > 1)
        strings::parseInt(fields[1], modifier);

    if (final == '~') {
        if (first == 200) {
            inPaste_ = true; // feed() collects the text until the end marker
            pasteText_.clear();
            return used;
        }
        if (first == 201)
            return used; // stray end marker
        out.emplace_back(withModifiers(keyForTilde(static_cast<int>(first)), static_cast<int>(modifier)));
        return used;
    }
    if (final == 'I') {
        out.emplace_back(FocusEvent{true});
        return used;
    }
    if (final == 'O') {
        out.emplace_back(FocusEvent{false});
        return used;
    }
    KeyEvent key;
    if (keyForLetter(final, key)) {
        out.emplace_back(withModifiers(key, static_cast<int>(modifier)));
        return used;
    }
    return used; // unsupported sequence: ignore it
}

void InputDecoder::consumePaste(std::vector<InputEvent>& out)
{
    const std::size_t end = pending_.find(PasteEnd);
    if (end == std::string::npos) {
        // Keep a few bytes back in case the end marker is split across reads.
        const std::size_t keep = std::min(pending_.size(), PasteEnd.size() - 1);
        pasteText_ += pending_.substr(0, pending_.size() - keep);
        pending_.erase(0, pending_.size() - keep);
        return;
    }
    pasteText_ += pending_.substr(0, end);
    pending_.erase(0, end + PasteEnd.size());
    inPaste_ = false;
    out.emplace_back(PasteEvent{normaliseNewlines(pasteText_)});
    pasteText_.clear();
}

} // namespace ovc::platform
