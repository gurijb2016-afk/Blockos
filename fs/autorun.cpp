#include "report-problem.hpp"

#include <stddef.h>
#include <string.h>

namespace Blockos {

class AutoRunSystem {
private:

    static void execute_command(const char* command)
    {
        if (command == nullptr || command[0] == '\0')
            return;

        if (strncmp(command, "exec ", 5) == 0) {
            const char* program = command + 5;

            while (*program == ' ')
                ++program;

            if (*program == '\0') {
                ReportProblem(
                    ProblemLevel::WARNING,
                    "AUTORUN",
                    "Ures exec parancs"
                );
                return;
            }

            ReportProblem(
                ProblemLevel::INFO,
                "AUTORUN",
                "Program inditasi keres"
            );
        }
        else if (strncmp(command, "echo ", 5) == 0) {
            ReportProblem(
                ProblemLevel::INFO,
                "AUTORUN",
                command + 5
            );
        }
        else if (strstr(command, ".krk") != nullptr) {
            ReportProblem(
                ProblemLevel::INFO,
                "AUTORUN",
                "Kuroko script detektalva"
            );
        }
        else {
            ReportProblem(
                ProblemLevel::WARNING,
                "AUTORUN",
                "Ismeretlen autorun parancs"
            );
        }
    }

public:

    static void process_autorun()
    {
        ReportProblem(
            ProblemLevel::INFO,
            "AUTORUN",
            "Automatikus inditas kezdodik"
        );

        /*
         * A VFS-ből történő /system/autorun.sh
         * beolvasását akkor kötjük vissza,
         * amikor a jelenlegi VFS API ismert.
         */

        ReportProblem(
            ProblemLevel::INFO,
            "AUTORUN",
            "Autorun VFS kapcsolat meg nincs bekotve"
        );

        ReportProblem(
            ProblemLevel::INFO,
            "AUTORUN",
            "Automatikus inditas befejezodott"
        );
    }
};

void trigger_autorun()
{
    AutoRunSystem::process_autorun();
}

} // namespace Blockos
