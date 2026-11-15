#pragma once

#include "dashboard/Screen.hpp"
#include "dashboard/SessionRecord.hpp"
#include "evidence/Journal.hpp"
#include "ui/EditorView.hpp"

#include <memory>

namespace ovc::dashboard {

// Steps through the edit journal so the examiner can watch the code being
// written, and jump straight to the moments that matter (pastes, compiles,
// tests, returns to the window).
class ReplayScreen final : public Screen {
public:
    explicit ReplayScreen(std::shared_ptr<const SessionRecord> record);

    void draw(ui::ScreenBuffer& screen) override;
    ScreenResult handleKey(const platform::KeyEvent& key) override;

private:
    void moveTo(std::size_t position);
    void jumpToEvent(bool forward);
    timeutil::Millis currentTime() const;

    std::shared_ptr<const SessionRecord> record_;
    std::size_t position_ = 0; // number of journal entries applied
    evidence::ReplayResult state_;
    ui::EditorView view_;
};

} // namespace ovc::dashboard
