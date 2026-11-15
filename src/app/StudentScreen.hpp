#pragma once

#include "editor/Editor.hpp"
#include "exam/ExamPackage.hpp"
#include "ui/EditorView.hpp"
#include "ui/ScreenBuffer.hpp"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace ovc::app {

// The box under the editor: compile errors, test results or program output.
struct ResultPanel {
    std::string title;
    std::vector<std::pair<std::string, ui::Style>> lines;
    bool empty() const { return title.empty(); }
};

// A window drawn over everything: rules, problem, help, full output, receipt.
struct Overlay {
    std::string title;
    std::vector<std::string> lines;
    std::string footer = "Esc to close   Up/Down to scroll";
    std::size_t scroll = 0;
    bool endsProgram = false; // the receipt: closing it ends the program
};

// Everything the student screen shows, gathered by the app each frame.
struct StudentScreenState {
    const editor::Editor* editor = nullptr;
    const exam::ExamPackage* package = nullptr;
    std::string fileName;
    std::string timeText;     // "41:07 left" or "no time limit"
    bool timeShort = false;   // under the last warning
    bool showProblem = true;
    const ResultPanel* panel = nullptr;
    std::string notice;
    const Overlay* overlay = nullptr;
};

// Lays out and draws the student's screen:
//   top bar | editor + problem panel | result panel | notice | command line
class StudentScreen {
public:
    void draw(ui::ScreenBuffer& screen, const StudentScreenState& state);

    // Rows the overlay can show at the current size (for page scrolling).
    int overlayRows() const { return overlayRows_; }

private:
    void drawTopBar(ui::ScreenBuffer& screen, const StudentScreenState& state);
    void drawProblem(ui::ScreenBuffer& screen, const ui::Rect& area, const exam::ExamPackage& package);
    void drawPanel(ui::ScreenBuffer& screen, const ui::Rect& area, const ResultPanel& panel);
    void drawBottomLine(ui::ScreenBuffer& screen, int y, const editor::Editor& editor);
    void drawOverlay(ui::ScreenBuffer& screen, const Overlay& overlay);

    ui::EditorView editorView_;
    int overlayRows_ = 10;
};

} // namespace ovc::app
