#pragma once

#include "editor/OriginMap.hpp"
#include "editor/TextBuffer.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace ovc::editor {

// One undoable change to the text. execute() and undo() return the offset
// where the cursor should be placed afterwards.
class Command {
public:
    virtual ~Command() = default;

    virtual std::size_t execute(TextBuffer& buffer) = 0;
    virtual std::size_t undo(TextBuffer& buffer) = 0;

    // Called with the command that was executed right after this one. Returns
    // true if this command took it over, so both are undone in one step
    // (used to group consecutive typing).
    virtual bool absorb(const Command& next)
    {
        (void)next;
        return false;
    }
};

class InsertTextCommand : public Command {
public:
    InsertTextCommand(std::size_t position, std::string text, Origin origin);

    std::size_t execute(TextBuffer& buffer) override;
    std::size_t undo(TextBuffer& buffer) override;
    bool absorb(const Command& next) override;

private:
    std::size_t position_;
    std::string text_;
    Origin origin_;
};

class DeleteRangeCommand : public Command {
public:
    DeleteRangeCommand(std::size_t position, std::size_t count);

    std::size_t execute(TextBuffer& buffer) override;
    std::size_t undo(TextBuffer& buffer) override;

    const std::string& removedText() const { return removed_; }

private:
    std::size_t position_;
    std::size_t count_;
    std::string removed_;
    std::vector<OriginMap::Span> removedOrigins_; // restored on undo
};

// Several commands that are done and undone together, e.g. "cc".
class CompositeCommand : public Command {
public:
    void add(std::unique_ptr<Command> command);
    bool empty() const { return commands_.empty(); }

    std::size_t execute(TextBuffer& buffer) override;
    std::size_t undo(TextBuffer& buffer) override;

private:
    std::vector<std::unique_ptr<Command>> commands_;
};

} // namespace ovc::editor
