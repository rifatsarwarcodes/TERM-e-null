#include "editor/UndoManager.hpp"

namespace ovc::editor {

UndoManager::UndoManager(std::size_t limit)
    : limit_(limit)
{
}

std::size_t UndoManager::execute(std::unique_ptr<Command> command, TextBuffer& buffer)
{
    const std::size_t cursor = command->execute(buffer);
    redoStack_.clear();

    if (groupOpen_ && !undoStack_.empty() && undoStack_.back()->absorb(*command))
        return cursor;

    undoStack_.push_back(std::move(command));
    if (undoStack_.size() > limit_)
        undoStack_.erase(undoStack_.begin());
    groupOpen_ = true;
    return cursor;
}

std::optional<std::size_t> UndoManager::undo(TextBuffer& buffer)
{
    if (undoStack_.empty())
        return std::nullopt;
    auto command = std::move(undoStack_.back());
    undoStack_.pop_back();
    const std::size_t cursor = command->undo(buffer);
    redoStack_.push_back(std::move(command));
    groupOpen_ = false;
    return cursor;
}

std::optional<std::size_t> UndoManager::redo(TextBuffer& buffer)
{
    if (redoStack_.empty())
        return std::nullopt;
    auto command = std::move(redoStack_.back());
    redoStack_.pop_back();
    const std::size_t cursor = command->execute(buffer);
    undoStack_.push_back(std::move(command));
    groupOpen_ = false;
    return cursor;
}

} // namespace ovc::editor
