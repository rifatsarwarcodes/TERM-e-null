#pragma once

#include "editor/Editor.hpp"
#include "editor/TextBuffer.hpp"
#include "ui/CppHighlighter.hpp"
#include "ui/ScreenBuffer.hpp"

#include <cstddef>
#include <optional>
#include <string>

namespace ovc::ui {

struct EditorViewOptions {
    bool syntaxHighlight = true;
    bool showOrigins = false;        // colour text by where it came from
    bool highlightCurrentLine = true;
    std::string searchPattern;       // matches get a highlighted background
};

// Draws a document into a rectangle: line numbers, syntax or origin colours,
// search matches and the cursor. Keeps its own scroll position so the cursor
// stays visible.
class EditorView {
public:
    void draw(ScreenBuffer& screen, const Rect& area, const editor::Editor& editor, bool showCursor);

    // Draws any buffer, for example the rebuilt final code in the dashboard.
    // `cursorOffset` decides the current line and the scroll position.
    void drawBuffer(ScreenBuffer& screen, const Rect& area, const editor::TextBuffer& buffer,
        std::optional<std::size_t> cursorOffset, bool showCursor, const EditorViewOptions& options);

    std::size_t topLine() const { return topLine_; }
    void scrollToLine(std::size_t line) { topLine_ = line; }

    static Style originStyle(editor::OriginKind kind);

private:
    void keepVisible(std::size_t cursorLine, std::size_t cursorDisplayColumn, int textRows, int textColumns);

    std::size_t topLine_ = 0;
    std::size_t leftColumn_ = 0;
    CppHighlighter highlighter_;
};

// Column on screen for a byte offset inside a line (tabs are 4 wide).
std::size_t displayColumn(const std::string& line, std::size_t byteColumn);

} // namespace ovc::ui
