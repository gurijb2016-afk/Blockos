#include <iostream>
#include <string>
#include <ctime>

// Problémák súlyossági szintjei
enum class ProblemLevel {
    INFO,
    WARNING,
    CRITICAL,
    KERNEL_PANIC
};

class ProblemReporter {
private:
    // Segédfunkció az aktuális időbélyeg lekéréséhez (szimulált/kernel idő)
    std::string getCurrentTimestamp() {
        std::time_code now = std::time(nullptr);
        char buf[20];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
        return std::string(buf);
    }

    // A processzor végleges leállítása Kernel Panic esetén
    void haltSystem() {
        std::cerr << "\n[KERNEL] A rendszer biztonsági okokból leállt (HALT).\n";
        // Valódi kernel környezetben itt assembly kód állna:
        // asm volatile("cli; hlt");
        std::exit(EXIT_FAILURE);
    }

public:
    // Probléma jelentése fix attribútumokkal
    void reportProblem(ProblemLevel level, const std::string& subsystem, const std::string& message) {
        std::string levelStr;
        std::string prefix = "[" + getCurrentTimestamp() + "] ";

        switch (level) {
            case ProblemLevel::INFO:
                levelStr = "[INFO] ";
                std::cout << prefix << levelStr << "(" << subsystem << "): " << message << "\n";
                break;
            
            case ProblemLevel::WARNING:
                levelStr = "[WARNING] ";
                std::cout << prefix << levelStr << "(" << subsystem << "): " << message << "\n";
                break;

            case ProblemLevel::CRITICAL:
                levelStr = "[CRITICAL ERROR] ";
                std::cerr << prefix << levelStr << "(" << subsystem << "): " << message << "\n";
                break;

            case ProblemLevel::KERNEL_PANIC:
                levelStr = "[KERNEL PANIC] ";
                std::cerr << "\n==================================================\n";
                std::cerr << prefix << levelStr << "Alrendszer: " << subsystem << "\n";
                std::cerr << "Hibaüzenet: " << message << "\n";
                std::cerr << "==================================================\n";
                haltSystem();
                break;
        }
    }
};

// Példa a modul használatára
int main() {
    ProblemReporter reporter;

    // 1. Normál információs naplózás
    reporter.reportProblem(ProblemLevel::INFO, "MemoryManager", "A memória inicializálása sikeresen befejeződött.");

    // 2. Figyelmeztetés küldése
    reporter.reportProblem(ProblemLevel::WARNING, "DiskDriver", "Lassú válaszidő a 0. számú szektor olvasásakor.");

    // 3. Kritikus hiba, de a rendszer még futhat
    reporter.reportProblem(ProblemLevel::CRITICAL, "NetworkStack", "Nem sikerült IP-címet kérni a DHCP szervertől.");

    // 4. Végzetes hiba - Kernel Panic (leállítja a futást)
    reporter.reportProblem(ProblemLevel::KERNEL_PANIC, "Scheduler", "NullPointer hiba a taszkok ütemezése közben!");

    // Ide már nem jut el a kód
    return 0;
}
