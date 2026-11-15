#pragma once

#include "analysis/PasteClassifier.hpp"
#include "editor/Editor.hpp"
#include "evidence/EvidenceRecorder.hpp"
#include "evidence/HashChain.hpp"
#include "evidence/Journal.hpp"
#include "evidence/Seal.hpp"
#include "exam/ExamPackage.hpp"
#include "penalty/PenaltyEngine.hpp"
#include "session/ExamSession.hpp"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace ovc::app {

struct PasteRecord {
    std::uint64_t id = 0;
    editor::Origin origin;
    std::size_t length = 0;
    std::string source; // inside / self / allowed / outside
};

// The exam side of a student's sitting. Opens a new session or resumes an
// interrupted one, wires the event bus to the recorder and the penalty
// engine, journals the editor's buffer, and seals everything at the end.
class StudentSession {
public:
    StudentSession(const exam::ExamPackage& package, session::SessionStore store);
    StudentSession(const StudentSession&) = delete;
    StudentSession& operator=(const StudentSession&) = delete;
    ~StudentSession();

    // Starts (or resumes) the session and puts the document into `editor`.
    // Returns an error message if the session cannot be opened.
    std::optional<std::string> open(editor::Editor& editor, const session::SessionInfo& identity,
        const std::filesystem::path& sourceFile);

    bool resumed() const { return resumed_; }
    session::ExamSession& exam() { return *exam_; }
    const session::ExamSession& exam() const { return *exam_; }
    const session::SessionStore& store() const { return store_; }
    const penalty::PenaltyEngine& penalties() const { return *penalties_; }
    const exam::ExamPackage& package() const { return package_; }

    session::Event publish(session::Event event) { return exam_->publish(std::move(event)); }
    std::uint64_t nextSequence() const { return exam_->nextSequence(); }
    void subscribe(session::IEventListener* listener) { bus_.subscribe(listener); }

    analysis::PasteSource classify(const std::string& pasted, const editor::TextBuffer& buffer) const;
    void rememberPaste(const PasteRecord& record) { pastes_.push_back(record); }
    std::optional<PasteRecord> lastPaste() const;
    // "24:0.93,31:0.40": how much of each outside paste is still unchanged.
    std::string retainedSummary(const editor::TextBuffer& buffer) const;

    // Writes seal.txt from the current heads of both logs.
    evidence::Seal seal();

private:
    void attach(editor::Editor& editor);
    void restoreDocument(editor::Editor& editor);
    void replayEvents();

    const exam::ExamPackage& package_;
    session::SessionStore store_;
    session::EventBus bus_;
    evidence::ChainedLogWriter eventsLog_;
    evidence::ChainedLogWriter journalLog_;
    std::unique_ptr<evidence::EvidenceRecorder> recorder_;
    std::unique_ptr<penalty::PenaltyEngine> penalties_;
    std::unique_ptr<session::ExamSession> exam_;
    std::unique_ptr<evidence::JournalWriter> journal_;
    analysis::RemovedTextHistory removed_;
    analysis::PasteClassifier classifier_;
    std::vector<PasteRecord> pastes_;
    editor::TextBuffer* buffer_ = nullptr;
    bool live_ = false;
    bool resumed_ = false;
    std::size_t droppedBytes_ = 0;
};

} // namespace ovc::app
