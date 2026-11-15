#include "app/StudentScreen.hpp"

#include "common/Strings.hpp"
#include "ui/TextWrap.hpp"
#include "ui/Theme.hpp"

#include <algorithm>

namespace ovc::app {

using ui::Rect;
using ui::ScreenBuffer;
namespace theme = ui::theme;

namespace {

constexpr int MaxPanelLines = 4;
constexpr const char* Hints = "F1 help  F2 problem  F5 compile  F6 run  F7 test  Ctrl+S save  Ctrl+N note  :q end exam";

ui::Style modeStyle(const editor::Editor& editor)
{
    if (editor.readOnly())
        return theme::modeLocked();
    switch (editor.modeId()) {
    case editor::ModeId::Insert: return theme::modeInsert();
    case editor::ModeId::Normal: return theme::modeNormal();
    case editor::ModeId::Prompt: return theme::modeCommand();
    }
    return theme::modeNormal();
}

} // namespace

void StudentScreen::draw(ScreenBuffer& screen, const StudentScreenState& state)
{
    screen.clear();
    const int width = screen.width();
    const int height = screen.height();
    if (width < 20 || height < 8) {
        screen.text(0, 0, "Window too small", theme::warn());
        return;
    }
    drawTopBar(screen, state);

    int bottom = height - 1; // command line row
    if (!state.notice.empty()) {
        --bottom;
        screen.fill({0, bottom, width, 1}, theme::notice());
        screen.text(1, bottom, "● " + state.notice, theme::notice(), width - 2);
    }

    int mainBottom = bottom;
    if (state.panel != nullptr && !state.panel->empty()) {
        const int lines = std::min<int>(static_cast<int>(state.panel->lines.size()), MaxPanelLines);
        const int panelHeight = std::max(lines, 1) + 2;
        mainBottom = bottom - panelHeight;
        drawPanel(screen, {0, mainBottom, width, panelHeight}, *state.panel);
    }

    const bool problemVisible = state.showProblem && width >= 96 && state.package != nullptr;
    const int problemWidth = problemVisible ? std::clamp(width / 3, 34, 48) : 0;
    const Rect editorBox{0, 1, width - problemWidth, mainBottom - 1};
    const std::string title = state.fileName + (state.editor->modified() ? " [+]" : "");
    screen.box(editorBox, title, theme::border(), theme::title());
    const bool editorHasCursor = state.overlay == nullptr && !state.editor->promptActive();
    editorView_.draw(screen, editorBox.inner(), *state.editor, editorHasCursor);
    if (problemVisible)
        drawProblem(screen, {editorBox.right(), 1, problemWidth, mainBottom - 1}, *state.package);

    drawBottomLine(screen, height - 1, *state.editor);
    if (state.overlay != nullptr)
        drawOverlay(screen, *state.overlay);
}

void StudentScreen::drawTopBar(ScreenBuffer& screen, const StudentScreenState& state)
{
    const int width = screen.width();
    screen.fill({0, 0, width, 1}, theme::topBar());
    int x = screen.text(0, 0, " OV_C ", theme::brand());
    std::string left = "  " + state.fileName;
    if (state.package != nullptr)
        left += "   " + state.package->title;
    screen.text(x, 0, left, theme::topBar(), width / 2);

    const editor::Editor& editor = *state.editor;
    const auto position = editor.cursorPosition();
    const std::string mode = std::string(" ") + (editor.readOnly() ? "LOCKED" : editor.modeName()) + " ";
    const std::string pending = editor.pendingKeys();
    const std::string where = "  Ln " + std::to_string(position.line + 1) + ", Col "
        + std::to_string(position.column + 1) + (pending.empty() ? "" : "  " + pending) + "  ";
    const std::string time = " " + state.timeText + " ";

    x = width - static_cast<int>(strings::displayWidth(mode + where + time));
    x += screen.text(x, 0, mode, modeStyle(editor));
    x += screen.text(x, 0, where, theme::topBar());
    screen.text(x, 0, time, state.timeShort ? theme::badChip() : theme::topBar());
}

void StudentScreen::drawProblem(ScreenBuffer& screen, const Rect& area, const exam::ExamPackage& package)
{
    screen.box(area, "PROBLEM  (F2 hide)", theme::border(), theme::title());
    const Rect inner = area.inner();
    std::vector<std::pair<std::string, ui::Style>> lines;
    lines.emplace_back(package.title, theme::heading());
    lines.emplace_back("", theme::text());
    const std::string problem = package.problem.empty() ? "No problem statement in this exam package." : package.problem;
    for (const auto& line : ui::wrapText(problem, inner.width - 1))
        lines.emplace_back(line, theme::text());
    lines.emplace_back("", theme::text());
    lines.emplace_back("Sample tests: " + std::to_string(package.tests.size()) + "   (F7 or :test)", theme::dim());
    lines.emplace_back("Time per run: " + std::to_string(package.runTimeLimitSeconds) + " s", theme::dim());
    if (!package.allowed.empty())
        lines.emplace_back("Allowed snippets: " + std::to_string(package.allowed.size()), theme::dim());
    lines.emplace_back("Exam rules: :rules   Full text: :problem", theme::dim());

    for (int row = 0; row < inner.height && row < static_cast<int>(lines.size()); ++row)
        screen.text(inner.x + 1, inner.y + row, lines[static_cast<std::size_t>(row)].first,
            lines[static_cast<std::size_t>(row)].second, inner.width - 1);
}

void StudentScreen::drawPanel(ScreenBuffer& screen, const Rect& area, const ResultPanel& panel)
{
    screen.box(area, panel.title, theme::border(), theme::title());
    const Rect inner = area.inner();
    for (int row = 0; row < inner.height && row < static_cast<int>(panel.lines.size()); ++row) {
        const auto& line = panel.lines[static_cast<std::size_t>(row)];
        screen.text(inner.x + 1, inner.y + row, line.first, line.second, inner.width - 1);
    }
}

void StudentScreen::drawBottomLine(ScreenBuffer& screen, int y, const editor::Editor& editor)
{
    const int width = screen.width();
    if (editor.promptActive()) {
        const auto& prompt = editor.prompt();
        const std::string text = prompt.masked() ? std::string(strings::displayWidth(prompt.text()), '*') : prompt.text();
        int x = screen.text(0, y, prompt.label(), theme::heading(), width);
        x += screen.text(x, y, text, theme::commandLine(), width - x);
        screen.setCursor(std::min(x, width - 1), y);
        return;
    }
    if (!editor.message().empty())
        screen.text(0, y, editor.message(), theme::warn(), width);
    else
        screen.text(0, y, Hints, theme::dim(), width);
}

void StudentScreen::drawOverlay(ScreenBuffer& screen, const Overlay& overlay)
{
    const int width = std::min(screen.width() - 4, 96);
    const int maxHeight = screen.height() - 4;
    const int textWidth = width - 4;

    std::vector<std::string> wrapped;
    for (const auto& line : overlay.lines) {
        const auto pieces = ui::wrapText(line, textWidth);
        if (pieces.empty())
            wrapped.emplace_back();
        wrapped.insert(wrapped.end(), pieces.begin(), pieces.end());
    }
    const int height = std::min(maxHeight, static_cast<int>(wrapped.size()) + 4);
    overlayRows_ = std::max(1, height - 4);
    const Rect area{(screen.width() - width) / 2, (screen.height() - height) / 2, width, height};

    screen.fill(area, theme::text());
    screen.box(area, overlay.title, theme::title(), theme::title());
    const std::size_t first = std::min(overlay.scroll, wrapped.empty() ? 0 : wrapped.size() - 1);
    for (int row = 0; row < overlayRows_; ++row) {
        const std::size_t index = first + static_cast<std::size_t>(row);
        if (index >= wrapped.size())
            break;
        screen.text(area.x + 2, area.y + 1 + row, wrapped[index], theme::text(), textWidth);
    }
    screen.text(area.x + 2, area.bottom() - 2, overlay.footer, theme::dim(), textWidth);
    screen.hideCursor();
}

} // namespace ovc::app
