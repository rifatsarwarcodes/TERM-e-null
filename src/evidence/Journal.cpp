#include "evidence/Journal.hpp"

#include "common/Escape.hpp"
#include "common/Strings.hpp"

namespace ovc::evidence {

std::string JournalEntry::encode() const
{
    std::string body = std::to_string(sequence) + "|" + std::to_string(time) + "|";
    body += kind == Kind::Insert ? "INS|" : "DEL|";
    body += "pos=" + std::to_string(position);
    if (kind == Kind::Insert)
        body += ";origin=" + escape::encode(origin.encode());
    else
        body += ";len=" + std::to_string(text.size());
    body += ";text=" + escape::encode(text);
    return body;
}

std::optional<JournalEntry> JournalEntry::decode(std::string_view body)
{
    const std::size_t first = escape::findUnescaped(body, '|');
    const std::size_t second = escape::findUnescaped(body, '|', first + 1);
    const std::size_t third = escape::findUnescaped(body, '|', second + 1);
    if (first == std::string_view::npos || second == std::string_view::npos || third == std::string_view::npos)
        return std::nullopt;

    JournalEntry entry;
    long long time = 0;
    if (!strings::parseUnsigned(body.substr(0, first), entry.sequence)
        || !strings::parseInt(body.substr(first + 1, second - first - 1), time))
        return std::nullopt;
    entry.time = time;

    const std::string_view kind = body.substr(second + 1, third - second - 1);
    if (kind == "INS")
        entry.kind = Kind::Insert;
    else if (kind == "DEL")
        entry.kind = Kind::Erase;
    else
        return std::nullopt;

    bool havePosition = false;
    bool haveText = false;
    std::string_view rest = body.substr(third + 1);
    while (!rest.empty()) {
        const std::size_t end = escape::findUnescaped(rest, ';');
        const std::string_view pair = rest.substr(0, end);
        const std::size_t equals = escape::findUnescaped(pair, '=');
        if (equals != std::string_view::npos) {
            const std::string_view key = pair.substr(0, equals);
            const std::string value = escape::decode(pair.substr(equals + 1));
            std::uint64_t number = 0;
            if (key == "pos" && strings::parseUnsigned(value, number)) {
                entry.position = static_cast<std::size_t>(number);
                havePosition = true;
            } else if (key == "origin") {
                entry.origin = editor::Origin::decode(value);
            } else if (key == "text") {
                entry.text = value;
                haveText = true;
            }
        }
        if (end == std::string_view::npos)
            break;
        rest.remove_prefix(end + 1);
    }
    if (!havePosition || !haveText)
        return std::nullopt;
    return entry;
}

JournalWriter::JournalWriter(ChainedLogWriter& log, Clock clock, std::uint64_t lastSequence)
    : log_(log)
    , clock_(std::move(clock))
    , lastSequence_(lastSequence)
{
}

void JournalWriter::onInsert(std::size_t position, const std::string& text, const editor::Origin& origin)
{
    JournalEntry entry;
    entry.kind = JournalEntry::Kind::Insert;
    entry.position = position;
    entry.text = text;
    entry.origin = origin;
    write(std::move(entry));
}

void JournalWriter::onErase(std::size_t position, const std::string& removed)
{
    JournalEntry entry;
    entry.kind = JournalEntry::Kind::Erase;
    entry.position = position;
    entry.text = removed;
    write(std::move(entry));
}

void JournalWriter::write(JournalEntry entry)
{
    entry.sequence = ++lastSequence_;
    entry.time = clock_();
    log_.append(entry.encode());
}

std::vector<JournalEntry> JournalReplayer::parse(const std::vector<std::string>& bodies)
{
    std::vector<JournalEntry> entries;
    entries.reserve(bodies.size());
    for (const auto& body : bodies)
        if (auto entry = JournalEntry::decode(body))
            entries.push_back(std::move(*entry));
    return entries;
}

ReplayResult JournalReplayer::replay(const std::vector<JournalEntry>& entries, std::optional<std::size_t> limit)
{
    ReplayResult result;
    const std::size_t count = limit ? std::min(*limit, entries.size()) : entries.size();
    for (std::size_t i = 0; i < count; ++i) {
        const JournalEntry& entry = entries[i];
        const std::size_t size = result.buffer.size();
        if (entry.kind == JournalEntry::Kind::Insert) {
            if (entry.position > size) {
                result.consistent = false;
                if (!result.firstMismatch)
                    result.firstMismatch = entry.sequence;
                continue;
            }
            result.buffer.insert(entry.position, entry.text, entry.origin);
            result.insertedAt.insert(result.insertedAt.begin() + static_cast<std::ptrdiff_t>(entry.position),
                entry.text.size(), entry.time);
        } else {
            const std::size_t length = entry.text.size();
            if (entry.position + length > size || result.buffer.substr(entry.position, length) != entry.text) {
                result.consistent = false;
                if (!result.firstMismatch)
                    result.firstMismatch = entry.sequence;
                if (entry.position + length > size)
                    continue;
            }
            result.buffer.erase(entry.position, length);
            const auto begin = result.insertedAt.begin() + static_cast<std::ptrdiff_t>(entry.position);
            result.insertedAt.erase(begin, begin + static_cast<std::ptrdiff_t>(length));
        }
        ++result.applied;
    }
    return result;
}

} // namespace ovc::evidence
