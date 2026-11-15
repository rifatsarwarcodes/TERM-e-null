#pragma once

#include "dashboard/Screen.hpp"
#include "dashboard/SessionRecord.hpp"
#include "ui/EditorView.hpp"

#include <memory>

namespace ovc::dashboard {

// The final code coloured by where each part came from, with the origin
// summary and the viva focus list next to it. Selecting a focus item jumps
// to its lines.
class OriginViewScreen final : public Screen {
public:
    OriginViewScreen(std::shared_ptr<const SessionRecord> record, bool startInFocusList);

    void draw(ui::ScreenBuffer& screen) override;
    ScreenResult handleKey(const platform::KeyEvent& key) override;

private:
    void jumpToSelected();

    std::shared_ptr<const SessionRecord> record_;
    ui::EditorView view_;
    std::size_t line_ = 0;      // current code line (0-based)
    std::size_t selected_ = 0;  // focus item
    bool focusList_;            // arrows move through the focus list instead of the code
    int codeRows_ = 20;
};

} // namespace ovc::dashboard
