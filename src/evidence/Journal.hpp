#pragma once

#include "common/Time.hpp"
#include "editor/Origin.hpp"
#include "editor/TextBuffer.hpp"
#include "evidence/HashChain.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ovc::evidence {

// One primitive change to the student's file.
struct JournalEntry {
    enum class Kind { Insert, Erase };

    std::uint64_t sequence = 0;
    timeutil::Millis time = 0;
    Kind kind = Kind::Insert;
    std::size_t position = 0;
    std::string text;      // inserted text, or the text that was removed
    editor::Origin origin; // inserts only

    // "seq|time|INS|pos=..;origin=..;text=..." or "...|DEL|pos=..;len=..;text=..."
    std::string encode() const;
    static std::optional<JournalEntry> decode(std::string_view body);
};

// Listens to the text buffer and writes every insert and erase, including
// those made by undo and redo, to the chained journal.
class JournalWriter final : public editor::IEditObserver {
public:
    using Clock = std::function<timeutil::Millis()>;

    JournalWriter(ChainedLogWriter& log, Clock clock, std::uint64_t lastSequence);

    void onInsert(std::size_t position, const std::string& text, const editor::Origin& origin) override;
    void onErase(std::size_t position, const std::string& removed) override;

    std::uint64_t lastSequence() const { return lastSequence_; }

private:
    void write(JournalEntry entry);

    ChainedLogWriter& log_;
    Clock clock_;
    std::uint64_t lastSequence_;
};

// The document rebuilt from the journal, with the time each character was
// inserted (used for the viva focus list and the replay screen).
struct ReplayResult {
    editor::TextBuffer buffer;
    std::vector<timeutil::Millis> insertedAt; // one entry per byte of buffer
    std::size_t applied = 0;
    bool consistent = true; // every erase removed the text it says it removed
    std::optional<std::uint64_t> firstMismatch;
};

class JournalReplayer {
public:
    static std::vector<JournalEntry> parse(const std::vector<std::string>& bodies);
    // Applies entries in order; stops after `limit` entries if given.
    static ReplayResult replay(const std::vector<JournalEntry>& entries,
        std::optional<std::size_t> limit = std::nullopt);
};

} // namespace ovc::evidence
