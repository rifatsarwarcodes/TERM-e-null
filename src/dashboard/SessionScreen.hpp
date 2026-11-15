#pragma once

#include "dashboard/Screen.hpp"
#include "dashboard/SessionRecord.hpp"

#include <memory>
#include <string>

namespace ovc::dashboard {

// The main examiner view of one session: an integrity bar and five panels
// (event timeline, compile/run/test history, event detail, penalty breakdown
// and status). From here the examiner opens the code origin view, the viva
// focus list, the replay, saved outputs and the report.
class SessionScreen final : public Screen {
public:
    explicit SessionScreen(std::shared_ptr<const SessionRecord> record);

    void draw(ui::ScreenBuffer& screen) override;
    ScreenResult handleKey(const platform::KeyEvent& key) override;

private:
    void drawIntegrity(ui::ScreenBuffer& screen);
    void drawTimeline(ui::ScreenBuffer& screen, const ui::Rect& area);
    void drawHistory(ui::ScreenBuffer& screen, const ui::Rect& area);
    void drawDetail(ui::ScreenBuffer& screen, const ui::Rect& area);
    void drawPenalty(ui::ScreenBuffer& screen, const ui::Rect& area);
    void drawStatus(ui::ScreenBuffer& screen, const ui::Rect& area);
    ScreenResult openSelected();
    ScreenResult openOutputs();
    void exportReport();
    void checkReceipt(const std::string& typed);

    std::shared_ptr<const SessionRecord> record_;
    std::size_t selected_ = 0;
    std::size_t top_ = 0;
    int timelineRows_ = 10;
    std::string message_;
    bool typingReceipt_ = false;
    std::string receiptInput_;
    std::string receiptCheck_; // result of the last check, shown in the bar
};

} // namespace ovc::dashboard
