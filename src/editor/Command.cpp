#include "editor/Command.hpp"

namespace ovc::editor {

InsertTextCommand::InsertTextCommand(std::size_t position, std::string text, Origin origin)
    : position_(position)
    , text_(std::move(text))
    , origin_(origin)
{
}

std::size_t InsertTextCommand::execute(TextBuffer& buffer)
{
    buffer.insert(position_, text_, origin_);
    return position_ + text_.size();
}

std::size_t InsertTextCommand::undo(TextBuffer& buffer)
{
    buffer.erase(position_, text_.size());
    return position_;
}

bool InsertTextCommand::absorb(const Command& next)
{
    const auto* insert = dynamic_cast<const InsertTextCommand*>(&next);
    if (insert == nullptr)
        return false;
    // Only plain typing is grouped; a new line starts a new undo step.
    const bool bothTyped = origin_.kind == OriginKind::Typed && insert->origin_.kind == OriginKind::Typed;
    const bool continues = insert->position_ == position_ + text_.size();
    const bool lineBreak = insert->text_.find('\n') != std::string::npos
        || (!text_.empty() && text_.back() == '\n');
    if (!bothTyped || !continues || lineBreak)
        return false;
    text_ += insert->text_;
    return true;
}

DeleteRangeCommand::DeleteRangeCommand(std::size_t position, std::size_t count)
    : position_(position)
    , count_(count)
{
}

std::size_t DeleteRangeCommand::execute(TextBuffer& buffer)
{
    removedOrigins_ = buffer.originsIn(position_, count_);
    removed_ = buffer.erase(position_, count_);
    return position_;
}

std::size_t DeleteRangeCommand::undo(TextBuffer& buffer)
{
    // Put the text back piece by piece so each part gets its old origin.
    std::size_t offset = 0;
    for (const auto& span : removedOrigins_) {
        buffer.insert(position_ + offset, removed_.substr(offset, span.length), span.origin);
        offset += span.length;
    }
    return position_;
}

void CompositeCommand::add(std::unique_ptr<Command> command)
{
    commands_.push_back(std::move(command));
}

std::size_t CompositeCommand::execute(TextBuffer& buffer)
{
    std::size_t cursor = 0;
    for (auto& command : commands_)
        cursor = command->execute(buffer);
    return cursor;
}

std::size_t CompositeCommand::undo(TextBuffer& buffer)
{
    std::size_t cursor = 0;
    for (auto it = commands_.rbegin(); it != commands_.rend(); ++it)
        cursor = (*it)->undo(buffer);
    return cursor;
}

} // namespace ovc::editor
