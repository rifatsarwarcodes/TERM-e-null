#pragma once

#include <string>
#include <string_view>

namespace ovc::editor {

// Where a piece of text came from.
enum class OriginKind {
    Typed,   // typed by the student
    Loaded,  // was already in the file when the session started
    Inside,  // pasted with the editor's own p / P command
    Self,    // pasted through the terminal, but already seen in this session
    Allowed, // pasted, matches a snippet the teacher allowed
    Outside, // pasted from anywhere else
};

struct Origin {
    OriginKind kind = OriginKind::Typed;
    int pasteId = 0; // number of the PASTE event, 0 for typed/loaded text

    static Origin typed() { return {OriginKind::Typed, 0}; }
    static Origin loaded() { return {OriginKind::Loaded, 0}; }
    static Origin paste(OriginKind kind, int id) { return {kind, id}; }

    bool isPaste() const
    {
        return kind == OriginKind::Inside || kind == OriginKind::Self || kind == OriginKind::Allowed
            || kind == OriginKind::Outside;
    }

    // Compact form used in the edit journal: "T", "L", "O#24", "S#3"...
    std::string encode() const;
    static Origin decode(std::string_view text);

    friend bool operator==(const Origin& a, const Origin& b)
    {
        return a.kind == b.kind && a.pasteId == b.pasteId;
    }
    friend bool operator!=(const Origin& a, const Origin& b) { return !(a == b); }
};

const char* originKindName(OriginKind kind);

} // namespace ovc::editor
