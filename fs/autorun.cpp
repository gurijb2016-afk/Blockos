#include "fs/vfs.hpp"
#include "kernel/elf_loader.hpp"
#include "kernel/scheduler.hpp"
#include "kernel/allocator.hpp"
#include "report-problem.cpp"

namespace Blockos {

class AutoRunSystem {
private:
    // Segédfunkció egy sor beolvasására a parancsfájlból
    static size_t read_line(VFS::Node* node, size_t offset, char* line_buf, size_t max_len) {
        char ch;
        size_t bytes_read = 0;
        size_t current_offset = offset;

        while (bytes_read < max_len - 1) {
            // Beolvasunk 1 bájtot a VFS-ből a te fájlkezelőd segítségével [source: 2]
            size_t ret = VFS::read(node, &ch, 1, current_offset);
            if (ret <= 0) break; // Fájl vége vagy hiba

            current_offset++;

            if (ch == '\n' || ch == '\r') {
                if (bytes_read > 0) break; // Sor vége
                continue; // Üres sorok átugrása
            }

            line_buf[bytes_read++] = ch;
        }

        line_buf[bytes_read] = '\0';
        return bytes_read;
    }

    // Elindít egy ELF binárist háttérfolyamatként (Kthread vagy Ring 3 taszk) [source: 2]
    static void execute_program(const char* path) {
        ReportProblem(ProblemLevel::INFO, "AUTORUN", "Program inditasa: ");
        
        // Ellenőrizzük, létezik-e az ELF fájl a VFS-ben [source: 2]
        VFS::Node* exe_node = VFS::lookup(path);
        if (!exe_node) {
            ReportProblem(ProblemLevel::WARNING, "AUTORUN", "A program nem talalhato.");
            return;
        }

        // Az elf_loader.cpp-n keresztül betöltjük a memóriába [source: 2]
        // És létrehozunk egy új taszkot az ütemezőben (scheduler.cpp) [source: 2]
        Task* new_task = ElfLoader::load_and_create_task(exe_node);
        if (new_task) {
            Scheduler::enqueue_task(new_task);
        } else {
            ReportProblem(ProblemLevel::CRITICAL, "AUTORUN", "Sikertelen ELF betoltes.");
        }
    }

public:
    // Az automatikus indítási folyamat fő vezérlője
    static void process_autorun() {
        ReportProblem(ProblemLevel::INFO, "AUTORUN", "Automatikus inditasi folyamat kezdődik...");

        // 1. Megkeressük a konfigurációs fájlt a gyökérben vagy a system mappában [source: 2]
        const char* autorun_path = "/system/autorun.sh";
        VFS::Node* script_node = VFS::lookup(autorun_path);

        if (!script_node) {
            ReportProblem(ProblemLevel::WARNING, "AUTORUN", "Nincs /system/autorun.sh. Kihagyas.");
            return;
        }

        char line_buffer[256];
        size_t file_offset = 0;
        size_t line_length = 0;

        // 2. Sorról sorra beolvassuk a szkript tartalmát
        while ((line_length = read_line(script_node, file_offset, line_buffer, sizeof(line_buffer))) > 0) {
            file_offset += line_length + 1; // Léptetjük az offsetet (a \n miatt +1)

            // Kommentek és üres sorok átugrása
            if (line_buffer[0] == '#' || line_buffer[0] == '\0') {
                continue;
            }

            // Egyszerű parancsértelmezés (pl. ha a sor "exec /examples/shell") [source: 2]
            if (strncmp(line_buffer, "exec ", 5) == 0) {
                const char* target_bin = line_buffer + 5;
                execute_program(target_bin);
            } 
            // Itt adhatsz hozzá Kuroko script futtatást is, ha a sor ".krk" fájlra mutat [source: 2]
            else if (strstr(line_buffer, ".krk") != nullptr) {
                ReportProblem(ProblemLevel::INFO, "AUTORUN", "Kuroko script detektalva, atadas az interpreternek.");
                // Kuroko::interpret_file(line_buffer); [source: 2]
            }
        }
        
        ReportProblem(ProblemLevel::INFO, "AUTORUN", "Automatikus inditas befejezodott.");
    }
};

// Külsőleg hívható C interfész a kernel_main() számára [source: 2]
void trigger_autorun() {
    AutoRunSystem::process_autorun();
}

} // namespace Blockos
