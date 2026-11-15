#include "dashboard/SessionLoader.hpp"

#include "analysis/PasteClassifier.hpp"
#include "common/Files.hpp"
#include "crypto/Sha256.hpp"
#include "dashboard/VivaFocus.hpp"
#include "evidence/HashChain.hpp"
#include "evidence/Seal.hpp"

namespace ovc::dashboard {

using session::Event;
using session::EventType;

std::string IntegrityReport::status() const
{
    if (!eventsFound)
        return "NO LOG";
    if (altered())
        return "LOG ALTERED";
    if (!ended || !sealPresent)
        return "INTERRUPTED";
    return "SEALED";
}

const PasteInfo* SessionRecord::paste(std::uint64_t sequence) const
{
    for (const auto& info : pastes)
        if (info.sequence == sequence)
            return &info;
    return nullptr;
}

std::string SessionRecord::readFile(const std::string& relativePath) const
{
    if (relativePath.empty() || relativePath.find("..") != std::string::npos)
        return {};
    return files::read(folder / relativePath).value_or("");
}

namespace {

void checkIntegrity(SessionRecord& record, const evidence::ChainReport& events, const evidence::ChainReport& journal,
    const exam::ExamPackage* examinerPackage)
{
    IntegrityReport& integrity = record.integrity;
    integrity.eventsFound = events.fileFound;
    integrity.eventsIntact = events.intact;
    integrity.eventsBrokenAt = events.firstBrokenLine;
    integrity.eventsTruncated = events.truncatedTail;
    integrity.journalIntact = journal.intact;
    integrity.journalBrokenAt = journal.firstBrokenLine;
    integrity.journalConsistent = record.replay.consistent;
    integrity.receipt = evidence::receiptCode(events.headHash, journal.headHash);

    const session::SessionStore store(record.folder);
    if (const auto seal = evidence::Seal::read(store.sealFile())) {
        integrity.sealPresent = true;
        integrity.sealMatchesLogs = seal->eventsHead == events.headHash && seal->journalHead == journal.headHash;
    }

    for (const auto& event : record.events) {
        if (event.type() == EventType::SessionResumed)
            ++integrity.resumes;
        if (event.type() == EventType::SessionEnd) {
            integrity.ended = true;
            const auto submitted = files::read(store.submissionDir() / record.info.fileName);
            if (submitted)
                integrity.submissionMatches = crypto::Sha256::hex(*submitted) == event.getOr("submission_hash");
            else
                integrity.submissionMatches = false;
        }
        if (event.type() == EventType::SessionStart && examinerPackage != nullptr && !examinerPackage->isPractice())
            integrity.packageMatches = event.getOr("package") == examinerPackage->sourceHash;
    }
}

void collectPastes(SessionRecord& record)
{
    std::optional<timeutil::Millis> lastReturn;
    for (const auto& event : record.events) {
        if (event.type() == EventType::FocusGained && event.getInt("away_ms") >= 10'000)
            lastReturn = event.time();
        if (event.type() == EventType::Paste) {
            PasteInfo info;
            info.sequence = event.sequence();
            info.time = event.time();
            info.source = event.getOr("src");
            info.confidence = event.getOr("conf");
            info.chars = event.getInt("chars");
            info.lines = event.getInt("lines");
            info.file = event.getOr("file");
            if (lastReturn)
                info.sinceFocusReturn = event.time() - *lastReturn;

            editor::OriginKind kind = editor::OriginKind::Outside;
            if (info.source == "inside") kind = editor::OriginKind::Inside;
            else if (info.source == "self") kind = editor::OriginKind::Self;
            else if (info.source == "allowed") kind = editor::OriginKind::Allowed;
            info.retained = analysis::retention(record.replay.buffer.origins(),
                editor::Origin::paste(kind, static_cast<int>(info.sequence)), static_cast<std::size_t>(info.chars));
            record.pastes.push_back(info);
        }
        if (event.type() == EventType::PasteNote) {
            for (auto& info : record.pastes)
                if (static_cast<long long>(info.sequence) == event.getInt("paste"))
                    info.note = event.getOr("text");
        }
    }
}

void computeShares(SessionRecord& record)
{
    for (const auto& span : record.replay.buffer.origins().spans()) {
        switch (span.origin.kind) {
        case editor::OriginKind::Typed: record.shares.typed += span.length; break;
        case editor::OriginKind::Loaded: record.shares.loaded += span.length; break;
        case editor::OriginKind::Inside: record.shares.inside += span.length; break;
        case editor::OriginKind::Self: record.shares.self += span.length; break;
        case editor::OriginKind::Allowed: record.shares.allowed += span.length; break;
        case editor::OriginKind::Outside: record.shares.outside += span.length; break;
        }
    }
}

void recomputePenalties(SessionRecord& record)
{
    penalty::PenaltyEngine engine(record.policy);
    int loggedPoints = 0;
    for (const auto& event : record.events) {
        engine.onEvent(event);
        if (event.type() == EventType::RuleHit)
            loggedPoints += static_cast<int>(event.getInt("points"));
    }
    record.hits = engine.hits();
    record.pointsByRule = engine.pointsByRule();
    record.totalPoints = engine.total();
    record.level = engine.level();
    record.integrity.pointsMatchLog = loggedPoints == engine.total();
}

void computeTimes(SessionRecord& record)
{
    timeutil::Millis start = 0;
    timeutil::Millis last = 0;
    for (const auto& event : record.events) {
        if (event.type() == EventType::SessionStart)
            start = event.time();
        last = std::max(last, event.time());
        if (event.type() == EventType::FocusGained)
            record.timeAway += event.getInt("away_ms");
        if (event.type() == EventType::Test) {
            record.testsPassed = static_cast<int>(event.getInt("passed"));
            record.testsTotal = static_cast<int>(event.getInt("total"));
        }
    }
    if (start == 0)
        start = record.info.startedAt;
    record.duration = last > start ? last - start : 0;
}

} // namespace

SessionRecord SessionLoader::load(const std::filesystem::path& folder, const exam::ExamPackage* examinerPackage)
{
    SessionRecord record;
    record.folder = folder;
    const session::SessionStore store(folder);
    if (const auto info = store.readInfo())
        record.info = *info;
    record.policy = examinerPackage != nullptr ? examinerPackage->policy : penalty::PenaltyPolicy{};

    const evidence::ChainReport events = evidence::readChain(store.eventsLog());
    const evidence::ChainReport journal = evidence::readChain(store.journalLog());
    for (const auto& body : events.bodies)
        if (auto event = Event::decode(body))
            record.events.push_back(std::move(*event));
    record.journal = evidence::JournalReplayer::parse(journal.bodies);
    record.replay = evidence::JournalReplayer::replay(record.journal);

    checkIntegrity(record, events, journal, examinerPackage);
    collectPastes(record);
    computeShares(record);
    recomputePenalties(record);
    computeTimes(record);
    record.focus = VivaFocusBuilder::build(record);
    return record;
}

} // namespace ovc::dashboard
