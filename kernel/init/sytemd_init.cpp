#include "kernel/systemd_parser.hpp"
#include "kernel/systemd_graph.hpp"
#include "fs/vfs.hpp"
#include "report-problem.cpp"

namespace Blockos {

class SystemdInit {
public:
    static bool bootstrap_services() {
        ReportProblem(ProblemLevel::INFO, "SYSTEMD", "Szolgaltatas-graf felepitese...");

        // Megnyitjuk a systemd konfigurációs fájlt a VFS-en keresztül [source: 2]
        VFS::Node* config_node = VFS::lookup("/system/services.target");
        if (!config_node) {
            ReportProblem(ProblemLevel::WARNING, "SYSTEMD", "A /system/services.target nem talalhato.");
            return false;
        }

        // A te meglévő parsered feldolgozza a fájlt [source: 2]
        if (!SystemdParser::parse_target(config_node)) {
            ReportProblem(ProblemLevel::CRITICAL, "SYSTEMD", "Szintaktikai hiba a configban.");
            return false;
        }

        // A gráf elindítja a szolgáltatásokat a függőségek alapján (pl. udevd -> knetworkd -> gui) [source: 2]
        SystemdGraph::resolve_and_start();
        return true;
    }
};

void trigger_systemd_boot() {
    SystemdInit::bootstrap_services();
}

} // namespace Blockos
