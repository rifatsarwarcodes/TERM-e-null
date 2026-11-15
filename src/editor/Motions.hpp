#pragma once

#include "editor/TextBuffer.hpp"

#include <cstddef>

// Cursor motions as pure functions: given the buffer and an offset, return
// the offset the motion lands on. Used by both the modes and the operators
// (d, c, y), and easy to test on their own.
namespace ovc::editor::motions {

std::size_t left(const TextBuffer& buffer, std::size_t offset, std::size_t count);
std::size_t right(const TextBuffer& buffer, std::size_t offset, std::size_t count); // stays on the line

std::size_t lineStart(const TextBuffer& buffer, std::size_t offset);
std::size_t lineEnd(const TextBuffer& buffer, std::size_t offset);
std::size_t firstNonBlank(const TextBuffer& buffer, std::size_t offset);

// Moves `lines` up (negative) or down (positive), trying to keep the column.
std::size_t vertical(const TextBuffer& buffer, std::size_t offset, long lines, std::size_t preferredColumn);

std::size_t wordForward(const TextBuffer& buffer, std::size_t offset, std::size_t count);
std::size_t wordBackward(const TextBuffer& buffer, std::size_t offset, std::size_t count);
std::size_t wordEnd(const TextBuffer& buffer, std::size_t offset, std::size_t count);

// The leading spaces/tabs of the line containing offset.
std::string indentationOf(const TextBuffer& buffer, std::size_t offset);

} // namespace ovc::editor::motions
