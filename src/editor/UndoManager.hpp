#pragma once

#include "editor/Command.hpp"

#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

namespace ovc::editor {

// Runs commands and keeps the undo and redo stacks.
class UndoManager {
public:
    explicit UndoManager(std::size_t limit = 2000);

    // Executes the command and records it. Returns the new cursor offset.
    std::size_t execute(std::unique_ptr<Command> command, TextBuffer& buffer);

    std::optional<std::size_t> undo(TextBuffer& buffer);
    std::optional<std::size_t> redo(TextBuffer& buffer);

    // The next command starts a new undo step even if it could be merged.
    void closeGroup() { groupOpen_ = false; }

    bool canUndo() const { return !undoStack_.empty(); }
    bool canRedo() const { return !redoStack_.empty(); }

private:
    std::size_t limit_;
    std::vector<std::unique_ptr<Command>> undoStack_;
    std::vector<std::unique_ptr<Command>> redoStack_;
    bool groupOpen_ = false;
};

} // namespace ovc::editor
