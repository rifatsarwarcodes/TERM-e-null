#include "ui/EditorView.hpp"

#include "editor/KmpSearch.hpp"
#include "ui/Theme.hpp"

#include <algorithm>
#include <vector>

namespace ovc::ui {

namespace {

constexpr std::size_t TabWidth = 4;

std::size_t codePointLength(unsigned char lead)
{
    if ((lead & 0xE0) == 0xC0) return 2;
    if ((lead & 0xF0) == 0xE0) return 3;
    if ((lead & 0xF8) == 0xF0) return 4;
    return 1;
}

int digitCount(std::size_t value)
{
    int digits = 1;
    while (value >= 10) {
        value /= 10;
        ++digits;
    }
    return digits;
}

} // namespace

std::size_t displayColumn(const std::string& line, std::size_t byteColumn)
{
    std::size_t column = 0;
    std::size_t i = 0;
    while (i < byteColumn && i < line.size()) {
        if (line[i] == '\t') {
            column += TabWidth - column % TabWidth;
            ++i;
        } else {
            ++column;
            i += codePointLength(static_cast<unsigned char>(line[i]));
        }
    }
    return column;
}

Style EditorView::originStyle(editor::OriginKind kind)
{
    switch (kind) {
    case editor::OriginKind::Typed: return theme::originTyped();
    case editor::OriginKind::Loaded: return theme::originLoaded();
    case editor::OriginKind::Inside: return theme::originInside();
    case editor::OriginKind::Self: return theme::originSelf();
    case editor::OriginKind::Allowed: return theme::originAllowed();
    case editor::OriginKind::Outside: return theme::originOutside();
    }
    return theme::text();
}

void EditorView::draw(ScreenBuffer& screen, const Rect& area, const editor::Editor& editor, bool showCursor)
{
    EditorViewOptions options;
    options.searchPattern = editor.searchPattern();
    drawBuffer(screen, area, editor.buffer(), editor.cursor(), showCursor, options);
}

void EditorView::keepVisible(std::size_t cursorLine, std::size_t cursorDisplayColumn, int textRows, int textColumns)
{
    const auto rows = static_cast<std::size_t>(std::max(textRows, 1));
    const auto columns = static_cast<std::size_t>(std::max(textColumns, 1));
    if (cursorLine < topLine_)
        topLine_ = cursorLine;
    else if (cursorLine >= topLine_ + rows)
        topLine_ = cursorLine - rows + 1;
    if (cursorDisplayColumn < leftColumn_)
        leftColumn_ = cursorDisplayColumn;
    else if (cursorDisplayColumn >= leftColumn_ + columns)
        leftColumn_ = cursorDisplayColumn - columns + 1;
}

void EditorView::drawBuffer(ScreenBuffer& screen, const Rect& area, const editor::TextBuffer& buffer,
    std::optional<std::size_t> cursorOffset, bool showCursor, const EditorViewOptions& options)
{
    if (area.empty())
        return;
    const std::size_t lineCount = buffer.lineCount();
    const int gutter = std::max(3, digitCount(lineCount)) + 1;
    const int originBar = options.showOrigins ? 2 : 0;
    const int textX = area.x + gutter + 1 + originBar;
    const int textColumns = area.right() - textX;

    std::size_t cursorLine = 0;
    std::size_t cursorDisplay = 0;
    if (cursorOffset) {
        const auto position = buffer.positionOf(*cursorOffset);
        cursorLine = position.line;
        cursorDisplay = displayColumn(buffer.line(cursorLine), position.column);
        keepVisible(cursorLine, cursorDisplay, area.height, textColumns);
    }
    topLine_ = std::min(topLine_, lineCount - 1);

    // Block comments can start above the visible area.
    bool inBlockComment = false;
    if (options.syntaxHighlight && !options.showOrigins)
        for (std::size_t line = 0; line < topLine_; ++line)
            highlighter_.highlight(buffer.line(line), inBlockComment);

    const editor::KmpSearch matcher(options.searchPattern);

    for (int row = 0; row < area.height; ++row) {
        const int y = area.y + row;
        screen.fill({area.x, y, area.width, 1}, theme::text());
        const std::size_t lineIndex = topLine_ + static_cast<std::size_t>(row);
        if (lineIndex >= lineCount)
            continue;

        const bool current = cursorOffset && lineIndex == cursorLine && options.highlightCurrentLine;
        const std::string number = std::to_string(lineIndex + 1);
        screen.text(area.x + gutter - static_cast<int>(number.size()), y, number,
            current ? theme::currentLineNumber() : theme::lineNumber());

        const std::string line = buffer.line(lineIndex);
        std::vector<Style> styles(line.size(), theme::text());

        if (options.showOrigins) {
            const std::size_t start = buffer.lineStart(lineIndex);
            std::size_t offset = 0;
            bool anyPasted = false;
            for (const auto& span : buffer.originsIn(start, line.size())) {
                for (std::size_t i = 0; i < span.length && offset + i < styles.size(); ++i)
                    styles[offset + i] = originStyle(span.origin.kind);
                anyPasted = anyPasted || span.origin.kind == editor::OriginKind::Outside;
                offset += span.length;
            }
            if (!line.empty()) {
                const auto first = buffer.origins().at(start);
                const Style bar = originStyle(anyPasted ? editor::OriginKind::Outside : first.kind);
                if (anyPasted || first.kind != editor::OriginKind::Typed)
                    screen.text(area.x + gutter + 1, y, "▌", bar);
            }
        } else if (options.syntaxHighlight) {
            for (const auto& span : highlighter_.highlight(line, inBlockComment))
                for (std::size_t i = span.start; i < span.start + span.length && i < styles.size(); ++i)
                    styles[i] = span.style;
        }

        if (!options.searchPattern.empty())
            for (std::size_t match : matcher.findAll(line))
                for (std::size_t i = match; i < match + options.searchPattern.size() && i < styles.size(); ++i)
                    styles[i] = theme::searchMatch();

        // Draw the characters, expanding tabs and skipping horizontally
        // scrolled columns.
        std::size_t column = 0;
        std::size_t i = 0;
        while (i < line.size()) {
            const std::size_t length = std::min(codePointLength(static_cast<unsigned char>(line[i])), line.size() - i);
            const std::size_t width = line[i] == '\t' ? TabWidth - column % TabWidth : 1;
            for (std::size_t w = 0; w < width; ++w) {
                const std::size_t col = column + w;
                if (col >= leftColumn_ && col < leftColumn_ + static_cast<std::size_t>(std::max(textColumns, 0))) {
                    const int x = textX + static_cast<int>(col - leftColumn_);
                    screen.put(x, y, line[i] == '\t' ? std::string_view(" ") : std::string_view(line).substr(i, length), styles[i]);
                }
            }
            column += width;
            i += length;
        }

        if (current)
            screen.setBackground(textX, y, textColumns, theme::currentLineBackground());
    }

    if (cursorOffset && showCursor && cursorLine >= topLine_) {
        const int cursorY = area.y + static_cast<int>(cursorLine - topLine_);
        const int cursorX = textX + static_cast<int>(cursorDisplay - leftColumn_);
        if (cursorY < area.bottom() && cursorX < area.right())
            screen.setCursor(cursorX, cursorY);
    }
}

} // namespace ovc::ui
