#include "app/StudentSession.hpp"

#include "common/Files.hpp"
#include "common/Strings.hpp"
#include "crypto/Sha256.hpp"

#include <cstdio>

namespace ovc::app {

using session::Event;
using session::EventType;

namespace {

std::vector<std::string> allowedTexts(const exam::ExamPackage& package)
{
    std::vector<std::string> texts;
    for (const auto& snippet : package.allowed)
        texts.push_back(snippet.text);
    return texts;
}

std::uint64_t sequenceOf(const std::string& body)
{
    const auto event = Event::decode(body);
    if (event)
        return event->sequence();
    std::uint64_t sequence = 0;
    strings::parseUnsigned(body.substr(0, body.find('|')), sequence);
    return sequence;
}

} // namespace

StudentSession::StudentSession(const exam::ExamPackage& package, session::SessionStore store)
    : package_(package)
    , store_(std::move(store))
    , classifier_(allowedTexts(package))
{
}

StudentSession::~StudentSession()
{
    if (buffer_ != nullptr) {
        if (journal_)
            buffer_->removeObserver(journal_.get());
        buffer_->removeObserver(&removed_);
    }
}

std::optional<std::string> StudentSession::open(editor::Editor& editor, const session::SessionInfo& identity,
    const std::filesystem::path& sourceFile)
{
    const session::SessionState state = store_.state();
    if (state == session::SessionState::Sealed)
        return "This session was already submitted and sealed.";

    session::SessionInfo info = identity;
    if (state == session::SessionState::Interrupted) {
        const auto saved = store_.readInfo();
        if (!saved)
            return "The session folder exists but session.info cannot be read.";
        info = *saved;
        resumed_ = true;
    } else {
        if (!store_.create())
            return "Cannot create the session folder " + store_.folder().string();
        if (!store_.writeInfo(info))
            return "Cannot write " + store_.infoFile().string();
    }

    if (!eventsLog_.open(store_.eventsLog()) || !journalLog_.open(store_.journalLog()))
        return "Cannot open the session logs in " + store_.folder().string();
    droppedBytes_ = eventsLog_.droppedBytes() + journalLog_.droppedBytes();

    const std::uint64_t lastEvent = eventsLog_.lastBody().empty() ? 0 : sequenceOf(eventsLog_.lastBody());
    exam_ = std::make_unique<session::ExamSession>(info, store_, bus_, lastEvent,
        session::SessionClock(info.startedAt, info.timeLimit));

    recorder_ = std::make_unique<evidence::EvidenceRecorder>(eventsLog_);
    penalties_ = std::make_unique<penalty::PenaltyEngine>(package_.policy, [this](Event event) {
        if (live_)
            exam_->publish(std::move(event));
    });
    bus_.subscribe(recorder_.get());
    bus_.subscribe(penalties_.get());

    const std::uint64_t lastEdit = journalLog_.lastBody().empty() ? 0 : sequenceOf(journalLog_.lastBody());
    journal_ = std::make_unique<evidence::JournalWriter>(
        journalLog_, [this] { return exam_->clock().now(); }, lastEdit);

    if (resumed_) {
        restoreDocument(editor);
        replayEvents();
        attach(editor);
        live_ = true;

        Event resumedEvent(EventType::SessionResumed);
        resumedEvent.set("dropped_bytes", static_cast<long long>(droppedBytes_));
        publish(resumedEvent);

        // The file may have been edited by another program while OV_C was closed.
        const auto onDisk = files::read(sourceFile);
        if (onDisk && *onDisk != editor.buffer().text()) {
            Event changed(EventType::FileChangedOutside);
            changed.set("when", std::string("while closed"))
                .set("disk_hash", crypto::Sha256::hex(*onDisk))
                .set("editor_hash", crypto::Sha256::hex(editor.buffer().text()));
            publish(changed);
        }
    } else {
        attach(editor);
        live_ = true;
        const std::string content = files::read(sourceFile).value_or("");
        editor.loadText(content, editor::Origin::loaded()); // journaled as loaded text

        Event start(EventType::SessionStart);
        start.set("student", info.studentId)
            .set("name", info.studentName)
            .set("exam", info.examCode)
            .set("package", package_.isPractice() ? std::string("practice") : package_.sourceHash)
            .set("file", info.fileName)
            .set("file_hash", crypto::Sha256::hex(content))
            .set("time_limit_min", static_cast<long long>(info.timeLimit / 60000));
        publish(start);
    }
    return std::nullopt;
}

void StudentSession::attach(editor::Editor& editor)
{
    buffer_ = &editor.buffer();
    buffer_->addObserver(journal_.get());
    buffer_->addObserver(&removed_);
}

void StudentSession::restoreDocument(editor::Editor& editor)
{
    const evidence::ChainReport report = evidence::readChain(store_.journalLog());
    const auto entries = evidence::JournalReplayer::parse(report.bodies);
    const evidence::ReplayResult replay = evidence::JournalReplayer::replay(entries);

    // Rebuild the buffer span by span so every character keeps its origin.
    std::size_t offset = 0;
    const std::string text = replay.buffer.text();
    for (const auto& span : replay.buffer.origins().spans()) {
        editor.buffer().insert(offset, text.substr(offset, span.length), span.origin);
        offset += span.length;
    }
    editor.moveCursorTo(0);
}

void StudentSession::replayEvents()
{
    // Bring the penalty engine and the paste list back to where they were.
    const evidence::ChainReport report = evidence::readChain(store_.eventsLog());
    for (const auto& body : report.bodies) {
        const auto event = Event::decode(body);
        if (!event)
            continue;
        penalties_->onEvent(*event);
        if (event->type() == EventType::Paste) {
            PasteRecord record;
            record.id = event->sequence();
            record.source = event->getOr("src");
            record.length = static_cast<std::size_t>(event->getInt("chars"));
            editor::OriginKind kind = editor::OriginKind::Outside;
            if (record.source == "inside") kind = editor::OriginKind::Inside;
            else if (record.source == "self") kind = editor::OriginKind::Self;
            else if (record.source == "allowed") kind = editor::OriginKind::Allowed;
            record.origin = editor::Origin::paste(kind, static_cast<int>(record.id));
            pastes_.push_back(record);
        }
    }
}

analysis::PasteSource StudentSession::classify(const std::string& pasted, const editor::TextBuffer& buffer) const
{
    return classifier_.classify(pasted, buffer.text() + "\n" + removed_.text());
}

std::optional<PasteRecord> StudentSession::lastPaste() const
{
    if (pastes_.empty())
        return std::nullopt;
    return pastes_.back();
}

std::string StudentSession::retainedSummary(const editor::TextBuffer& buffer) const
{
    std::string summary;
    for (const auto& paste : pastes_) {
        if (paste.source != "outside" || paste.length == 0)
            continue;
        char item[48];
        std::snprintf(item, sizeof item, "%llu:%.2f", static_cast<unsigned long long>(paste.id),
            analysis::retention(buffer.origins(), paste.origin, paste.length));
        summary += (summary.empty() ? "" : ",") + std::string(item);
    }
    return summary;
}

evidence::Seal StudentSession::seal()
{
    evidence::Seal seal;
    seal.eventsHead = eventsLog_.headHash();
    seal.journalHead = journalLog_.headHash();
    seal.receipt = evidence::receiptCode(seal.eventsHead, seal.journalHead);
    seal.sealedAt = exam_->clock().now();
    seal.write(store_.sealFile());
    return seal;
}

} // namespace ovc::app
