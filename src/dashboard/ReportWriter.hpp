#pragma once

#include "dashboard/SessionRecord.hpp"

#include <string>
#include <vector>

namespace ovc::dashboard {

// Plain text reports for the examiner: one per student, and a class summary.
class ReportWriter {
public:
    static std::string sessionReport(const SessionRecord& record);
    static std::string classSummary(const std::vector<SessionRecord>& records);
};

std::string percentText(double ratio);

} // namespace ovc::dashboard
