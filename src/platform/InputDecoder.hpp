#pragma once

#include "platform/InputEvent.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace ovc::platform {

// Turns the raw bytes a terminal sends into events. Handles UTF-8, control
// keys, CSI/SS3 escape sequences (arrows, Home/End, function keys with
// modifiers), focus reports (ESC [ I / ESC [ O) and bracketed paste
// (ESC [ 200 ~ ... ESC [ 201 ~).
//
// Input may arrive split at any byte; incomplete sequences are kept until the
// next feed(). A lone ESC is only reported by flushPending(), which the caller
// uses when no more bytes arrive shortly after it.
class InputDecoder {
public:
    void feed(std::string_view bytes, std::vector<InputEvent>& out);

    // Emits anything still pending (a lone ESC becomes the Escape key).
    void flushPending(std::vector<InputEvent>& out);

    bool hasPending() const { return !pending_.empty() && !inPaste_; }
    bool inPaste() const { return inPaste_; }

private:
    // Tries to decode one event from the start of pending_. Returns the number
    // of bytes used, or 0 if more bytes are needed.
    std::size_t decodeOne(std::vector<InputEvent>& out);
    std::size_t decodeEscape(std::vector<InputEvent>& out);
    std::size_t decodeCsi(std::vector<InputEvent>& out);
    std::size_t decodeUtf8(std::vector<InputEvent>& out);
    void consumePaste(std::vector<InputEvent>& out);

    std::string pending_;
    bool inPaste_ = false;
    std::string pasteText_;
};

} // namespace ovc::platform
