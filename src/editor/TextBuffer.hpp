#pragma once

#include "editor/GapBuffer.hpp"
#include "editor/OriginMap.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace ovc::editor {

// Receives every primitive change made to a TextBuffer. The edit journal and
// paste analysis listen here, so they see undo/redo changes as well.
class IEditObserver {
public:
    virtual ~IEditObserver() = default;
    virtual void onInsert(std::size_t position, const std::string& text, const Origin& origin) = 0;
    virtual void onErase(std::size_t position, const std::string& removed) = 0;
};

struct TextPosition {
    std::size_t line = 0;
    std::size_t column = 0; // byte offset inside the line
};

// The document being edited: bytes in a gap buffer, a lazily rebuilt index
// of line starts, and the origin of every character.
class TextBuffer {
public:
    void insert(std::size_t position, std::string_view text, const Origin& origin);
    // Removes count bytes and returns them.
    std::string erase(std::size_t position, std::size_t count);

    std::size_t size() const { return chars_.size(); }
    bool empty() const { return chars_.empty(); }
    char at(std::size_t position) const { return chars_.at(position); }
    std::string text() const { return chars_.toString(); }
    std::string substr(std::size_t position, std::size_t count) const;

    // Lines. A buffer always has at least one (possibly empty) line.
    std::size_t lineCount() const;
    std::size_t lineStart(std::size_t line) const;
    std::size_t lineLength(std::size_t line) const; // without the newline
    std::string line(std::size_t line) const;

    TextPosition positionOf(std::size_t offset) const;
    std::size_t offsetOf(std::size_t line, std::size_t column) const; // clamps

    const OriginMap& origins() const { return origins_; }
    std::vector<OriginMap::Span> originsIn(std::size_t position, std::size_t count) const;

    void addObserver(IEditObserver* observer);
    void removeObserver(IEditObserver* observer);

private:
    void ensureLineIndex() const;

    GapBuffer chars_;
    OriginMap origins_;
    std::vector<IEditObserver*> observers_;

    mutable std::vector<std::size_t> lineStarts_{0};
    mutable bool lineIndexDirty_ = false;
};

} // namespace ovc::editor
