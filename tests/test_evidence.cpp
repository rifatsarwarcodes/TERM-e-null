#include "TestFramework.hpp"

#include "common/Files.hpp"
#include "editor/Editor.hpp"
#include "evidence/HashChain.hpp"
#include "evidence/Journal.hpp"
#include "evidence/Seal.hpp"

#include <filesystem>
#include <string>

using namespace ovc;
using namespace ovc::evidence;
namespace fs = std::filesystem;

namespace {

fs::path freshFile(const std::string& name)
{
    const fs::path path = fs::temp_directory_path() / ("ovc_test_" + name);
    fs::remove(path);
    return path;
}

void writeLines(const fs::path& path, int count)
{
    ChainedLogWriter writer;
    writer.open(path);
    for (int i = 1; i <= count; ++i)
        writer.append(std::to_string(i) + "|0|SAVE|n=" + std::to_string(i));
}

} // namespace

TEST_CASE("chain: an untouched log verifies")
{
    const fs::path path = freshFile("chain_ok.log");
    writeLines(path, 5);
    const ChainReport report = readChain(path);
    CHECK(report.fileFound);
    CHECK(report.intact);
    CHECK_EQ(report.bodies.size(), std::size_t(5));
    CHECK(!report.truncatedTail);
}

TEST_CASE("chain: editing one line is caught at that line")
{
    const fs::path path = freshFile("chain_edit.log");
    writeLines(path, 5);
    std::string content = *files::read(path);
    content.replace(content.find("n=3"), 3, "n=9");
    files::writeAtomically(path, content);
    const ChainReport report = readChain(path);
    CHECK(!report.intact);
    CHECK(report.firstBrokenLine && *report.firstBrokenLine == 3);
}

TEST_CASE("chain: deleting a line is caught")
{
    const fs::path path = freshFile("chain_delete.log");
    writeLines(path, 4);
    std::string content = *files::read(path);
    const std::size_t second = content.find('\n') + 1;
    const std::size_t third = content.find('\n', second) + 1;
    content.erase(second, third - second);
    files::writeAtomically(path, content);
    const ChainReport report = readChain(path);
    CHECK(!report.intact);
    CHECK(report.firstBrokenLine && *report.firstBrokenLine == 2);
}

TEST_CASE("chain: a crash-cut last line is trimmed on reopen and the chain continues")
{
    const fs::path path = freshFile("chain_crash.log");
    writeLines(path, 3);
    {
        std::ofstream out(path, std::ios::app | std::ios::binary);
        out << "4|0|SAVE|n=4|h=12ab"; // cut off while writing
    }
    const ChainReport before = readChain(path);
    CHECK(before.truncatedTail);
    CHECK(before.intact);

    ChainedLogWriter writer;
    writer.open(path);
    CHECK(writer.droppedBytes() > 0);
    writer.append("4|0|SESSION_RESUMED|");
    const ChainReport after = readChain(path);
    CHECK(after.intact);
    CHECK(!after.truncatedTail);
    CHECK_EQ(after.bodies.size(), std::size_t(4));
}

TEST_CASE("chain: reopening continues from the last hash")
{
    const fs::path path = freshFile("chain_resume.log");
    writeLines(path, 2);
    ChainedLogWriter writer;
    writer.open(path);
    CHECK_EQ(writer.lineCount(), std::size_t(2));
    writer.append("3|0|SAVE|n=3");
    CHECK(readChain(path).intact);
}

TEST_CASE("journal: entries round trip")
{
    JournalEntry insert;
    insert.sequence = 7;
    insert.time = 99;
    insert.position = 12;
    insert.text = "a;b|c\nd";
    insert.origin = editor::Origin::paste(editor::OriginKind::Outside, 24);
    const auto decoded = JournalEntry::decode(insert.encode());
    CHECK(decoded.has_value());
    CHECK_EQ(decoded->text, insert.text);
    CHECK(decoded->origin == insert.origin);
    CHECK_EQ(decoded->position, std::size_t(12));
}

TEST_CASE("journal: replay rebuilds the editor's text and origins")
{
    const fs::path path = freshFile("journal.log");
    ChainedLogWriter log;
    log.open(path);
    timeutil::Millis now = 1000;
    JournalWriter journal(log, [&] { return now += 10; }, 0);

    editor::Editor editor;
    editor.buffer().addObserver(&journal);
    for (char c : std::string("int x;"))
        editor.handleKey(platform::KeyEvent::character(std::string(1, c)));
    editor.insertPaste("\nint y;", editor::Origin::paste(editor::OriginKind::Outside, 5));
    editor.handleKey(platform::KeyEvent::special(platform::KeyCode::Backspace));
    editor.undo();

    const ChainReport report = readChain(path);
    CHECK(report.intact);
    const auto entries = JournalReplayer::parse(report.bodies);
    const ReplayResult result = JournalReplayer::replay(entries);
    CHECK(result.consistent);
    CHECK_EQ(result.buffer.text(), editor.buffer().text());
    CHECK(result.buffer.origins().at(8) == editor::Origin::paste(editor::OriginKind::Outside, 5));
    CHECK_EQ(result.insertedAt.size(), result.buffer.size());

    const ReplayResult partial = JournalReplayer::replay(entries, 3);
    CHECK_EQ(partial.buffer.text(), std::string("int"));
}

TEST_CASE("journal: replay notices an erase that does not match")
{
    std::vector<JournalEntry> entries(2);
    entries[0].sequence = 1;
    entries[0].text = "hello";
    entries[1].sequence = 2;
    entries[1].kind = JournalEntry::Kind::Erase;
    entries[1].text = "xy";
    const ReplayResult result = JournalReplayer::replay(entries);
    CHECK(!result.consistent);
    CHECK(result.firstMismatch && *result.firstMismatch == 2);
}

TEST_CASE("seal: receipt format, matching and file round trip")
{
    const std::string code = receiptCode(std::string(64, 'a'), std::string(64, 'b'));
    CHECK_EQ(code.size(), std::size_t(12));
    CHECK_EQ(code[4], '-');
    CHECK(receiptsMatch(code, "  " + std::string(code).erase(4, 1)));
    CHECK(!receiptsMatch(code, "0000-0000-00"));
    CHECK(receiptCode("x", "y") != receiptCode("x", "z"));

    const fs::path path = freshFile("seal.txt");
    Seal seal{std::string(64, 'a'), std::string(64, 'b'), code, 42};
    CHECK(seal.write(path));
    const auto read = Seal::read(path);
    CHECK(read && read->receipt == code && read->sealedAt == 42);
}
