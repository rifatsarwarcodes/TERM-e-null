#pragma once

#include "dashboard/Screen.hpp"
#include "dashboard/SessionRecord.hpp"
#include "exam/ExamPackage.hpp"

#include <filesystem>
#include <memory>
#include <vector>

namespace ovc::dashboard {

// One row per student session found under a folder. Columns can be sorted
// (merge sort); Enter opens the session.
class ClassOverviewScreen final : public Screen {
public:
    ClassOverviewScreen(std::filesystem::path root, const exam::ExamPackage* examinerPackage);

    void draw(ui::ScreenBuffer& screen) override;
    ScreenResult handleKey(const platform::KeyEvent& key) override;

    std::size_t rowCount() const { return rows_.size(); }

private:
    void reload();
    void sortRows();
    void exportSummary();

    std::filesystem::path root_;
    const exam::ExamPackage* package_;
    std::vector<std::shared_ptr<const SessionRecord>> rows_;
    std::size_t selected_ = 0;
    int sortColumn_ = 0; // index into the column list
    std::string message_;
};

} // namespace ovc::dashboard
