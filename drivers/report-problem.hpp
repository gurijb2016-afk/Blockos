#pragma once

enum class ProblemLevel {
    INFO,
    WARNING,
    CRITICAL
};

void ReportProblem(ProblemLevel level, const char* component, const char* message);
