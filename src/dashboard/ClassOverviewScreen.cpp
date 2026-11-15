#include "dashboard/ClassOverviewScreen.hpp"

#include "common/Files.hpp"
#include "common/Time.hpp"
#include "dashboard/MergeSort.hpp"
#include "dashboard/ReportWriter.hpp"
#include "dashboard/SessionLoader.hpp"
#include "dashboard/SessionScreen.hpp"
#include "ui/Theme.hpp"

namespace ovc::dashboard {

using platform::KeyCode;
namespace fs = std::filesystem;
namespace theme = ui::theme;

namespace {

using Row = std::shared_ptr<const SessionRecord>;

// Higher means "look at this first".
int levelRank(const SessionRecord& r)
{
    if (r.integrity.altered()) return 4;
    if (r.level == penalty::Level::Flagged) return 3;
    if (r.level == penalty::Level::Warning) return 2;
    if (r.integrity.status() == "INTERRUPTED") return 1;
    return 0;
}

struct Column {
    const char* name;
    int width;
    bool (*less)(const Row&, const Row&);
};

const Column Columns[] = {
    {"Level", 10, [](const Row& a, const Row& b) { return levelRank(*a) > levelRank(*b); }},
    {"ID", 12, [](const Row& a, const Row& b) { return a->info.studentId < b->info.studentId; }},
    {"Name", 20, [](const Row& a, const Row& b) { return a->info.studentName < b->info.studentName; }},
    {"Integrity", 13, [](const Row& a, const Row& b) { return a->integrity.status() < b->integrity.status(); }},
    {"Mins", 6, [](const Row& a, const Row& b) { return a->duration > b->duration; }},
    {"Typed", 7, [](const Row& a, const Row& b) { return a->shares.share(a->shares.typed) < b->shares.share(b->shares.typed); }},
    {"Outside", 9, [](const Row& a, const Row& b) { return a->shares.share(a->shares.outside) > b->shares.share(b->shares.outside); }},
    {"Tests", 7, [](const Row& a, const Row& b) { return a->testsPassed < b->testsPassed; }},
    {"Away", 9, [](const Row& a, const Row& b) { return a->timeAway > b->timeAway; }},
};
constexpr int ColumnCount = static_cast<int>(sizeof Columns / sizeof Columns[0]);

std::string cell(const SessionRecord& r, int column)
{
    switch (column) {
    case 0: return penalty::levelName(r.level);
    case 1: return r.info.studentId;
    case 2: return r.info.studentName;
    case 3: return r.integrity.status();
    case 4: return std::to_string(r.duration / 60000);
    case 5: return percentText(r.shares.share(r.shares.typed));
    case 6: return percentText(r.shares.share(r.shares.outside));
    case 7: return r.testsPassed < 0 ? "-" : std::to_string(r.testsPassed) + "/" + std::to_string(r.testsTotal);
    case 8: return timeutil::formatSpan(r.timeAway);
    default: return "";
    }
}

ui::Style cellStyle(const SessionRecord& r, int column)
{
    if (column == 0)
        return r.level == penalty::Level::Ok ? theme::ok() : r.level == penalty::Level::Warning ? theme::warn() : theme::bad();
    if (column == 3)
        return r.integrity.status() == "SEALED" ? theme::ok() : r.integrity.altered() ? theme::bad() : theme::warn();
    return theme::text();
}

} // namespace

ClassOverviewScreen::ClassOverviewScreen(fs::path root, const exam::ExamPackage* examinerPackage)
    : root_(std::move(root))
    , package_(examinerPackage)
{
    reload();
}

void ClassOverviewScreen::reload()
{
    rows_.clear();
    std::error_code error;
    for (const auto& entry : fs::directory_iterator(root_, error)) {
        if (!entry.is_directory() || !fs::exists(entry.path() / "session.info"))
            continue;
        rows_.push_back(std::make_shared<const SessionRecord>(SessionLoader::load(entry.path(), package_)));
    }
    sortRows();
    selected_ = 0;
}

void ClassOverviewScreen::sortRows()
{
    // Sort by ID first, then by the chosen column: merge sort is stable, so
    // rows with equal values stay in ID order.
    mergeSort(rows_, Columns[1].less);
    mergeSort(rows_, Columns[sortColumn_].less);
}

void ClassOverviewScreen::exportSummary()
{
    std::vector<SessionRecord> records;
    for (const auto& row : rows_)
        records.push_back(*row);
    const std::string name = "class_summary.txt";
    message_ = files::writeAtomically(name, ReportWriter::classSummary(records)) ? "Class summary written to " + name
                                                                                 : "Could not write " + name;
}

void ClassOverviewScreen::draw(ui::ScreenBuffer& screen)
{
    std::string title = (package_ != nullptr ? package_->title + "   " : std::string()) + root_.string() + "   "
        + std::to_string(rows_.size()) + " session(s)   sorted by " + Columns[sortColumn_].name;
    chrome::header(screen, "CLASS OVERVIEW", title);

    const ui::Rect box{0, 1, screen.width(), screen.height() - 2};
    screen.box(box, "", theme::border(), theme::title());
    const ui::Rect inner = box.inner();
    int x = inner.x + 1;
    for (int c = 0; c < ColumnCount; ++c) {
        screen.text(x, inner.y, Columns[c].name, c == sortColumn_ ? theme::heading().withForeground({221}) : theme::heading());
        x += Columns[c].width;
    }
    screen.horizontalLine(inner.x, inner.y + 1, inner.width, theme::border());

    if (rows_.empty())
        screen.text(inner.x + 1, inner.y + 2, "No sessions found in " + root_.string(), theme::dim());
    for (std::size_t i = 0; i < rows_.size(); ++i) {
        const int y = inner.y + 2 + static_cast<int>(i);
        if (y >= inner.bottom() - 2)
            break;
        x = inner.x + 1;
        for (int c = 0; c < ColumnCount; ++c) {
            screen.text(x, y, cell(*rows_[i], c), cellStyle(*rows_[i], c), Columns[c].width - 1);
            x += Columns[c].width;
        }
        if (i == selected_)
            screen.restyle(inner.x, y, inner.width, theme::selection());
    }
    screen.text(inner.x + 1, inner.bottom() - 1,
        "Integrity combines the log chains, the seal and the submitted file. Values are recalculated from each folder.",
        theme::dim(), inner.width - 2);
    chrome::keyBar(screen, "Up/Down select  Enter open  S sort column  R reload  X export summary  Q quit", message_);
}

ScreenResult ClassOverviewScreen::handleKey(const platform::KeyEvent& key)
{
    message_.clear();
    if (key.is(KeyCode::Up) && selected_ > 0) --selected_;
    else if (key.is(KeyCode::Down) && selected_ + 1 < rows_.size()) ++selected_;
    else if (key.is(KeyCode::Enter) && selected_ < rows_.size())
        return ScreenResult::push(std::make_unique<SessionScreen>(rows_[selected_]));
    else if (key.is(KeyCode::Escape) || key.isChar('q') || key.isChar('Q'))
        return ScreenResult::quit();
    else if (key.isChar('s') || key.isChar('S')) {
        sortColumn_ = (sortColumn_ + 1) % ColumnCount;
        sortRows();
    } else if (key.isChar('r') || key.isChar('R')) {
        reload();
        message_ = "Reloaded.";
    } else if (key.isChar('x') || key.isChar('X')) {
        exportSummary();
    }
    return ScreenResult::stay();
}

} // namespace ovc::dashboard
