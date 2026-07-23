#include "init_system.hpp"
#include "dependency_graph.hpp"
#include "../scheduler_preempt.hpp"
#include "../../fs/vfs.hpp"
#include <efi.h>
#include <efilib.h>
#include <string.h>

namespace {

const wchar_t *state_name(init::UnitState s) {
    switch (s) {
        case init::UnitState::INACTIVE: return L"INACTIVE";
        case init::UnitState::STARTING: return L"STARTING";
        case init::UnitState::ACTIVE:   return L"ACTIVE";
        case init::UnitState::FAILED:   return L"FAILED";
        case init::UnitState::SKIPPED:  return L"SKIPPED";
    }
    return L"?";
}

} // namespace

bool init::InitSystem::boot(const char *dir_prefix) {
    for (uint32_t i = 0; i < MAX_UNITS; i++) runtime[i] = UnitRuntime{};

    const char *bad_units[MAX_UNITS];
    uint32_t bad_count = 0;
    registry.scan(dir_prefix, bad_units, &bad_count, MAX_UNITS);

    for (uint32_t i = 0; i < bad_count; i++) {
        CHAR16 buf[160];
        UnicodeSPrint(buf, sizeof(buf),
            (CHAR16 *)L"init: skipping invalid unit file (no ExecStart=): %a\n",
            bad_units[i]);
        Print(buf);
    }

    if (registry.count == 0) {
        Print((CHAR16 *)L"init: no valid unit files found\n");
        return false;
    }

    TopoResult topo;
    topo_sort(registry, &topo);

    if (topo.has_cycle) {
        for (uint32_t i = 0; i < topo.cyclic_count; i++) {
            uint32_t idx = topo.cyclic[i];
            runtime[idx].state = UnitState::SKIPPED;
            CHAR16 buf[160];
            UnicodeSPrint(buf, sizeof(buf),
                (CHAR16 *)L"init: %a is part of a dependency cycle, skipping\n",
                registry.units[idx].name);
            Print(buf);
        }
    }

    bool all_ok = !topo.has_cycle;

    for (uint32_t k = 0; k < topo.order_count; k++) {
        uint32_t idx = topo.order[k];
        UnitFile &u = registry.units[idx];

        // Ha bármelyik Requires= függősége nem ACTIVE, ezt a unitot
        // kihagyjuk (SKIPPED), nem próbáljuk elindítani.
        bool deps_ok = true;
        for (uint8_t d = 0; d < u.requires_count; d++) {
            UnitFile *dep = registry.find(u.requires_[d]);
            if (!dep) continue; // phantom függőség, nincs mit ellenőrizni
            uint32_t dep_idx = (uint32_t)(dep - registry.units);
            if (runtime[dep_idx].state != UnitState::ACTIVE) {
                deps_ok = false;
                break;
            }
        }

        if (!deps_ok) {
            runtime[idx].state = UnitState::SKIPPED;
            all_ok = false;
            CHAR16 buf[160];
            UnicodeSPrint(buf, sizeof(buf),
                (CHAR16 *)L"init: %a skipped (a required dependency did not start)\n",
                u.name);
            Print(buf);
            continue;
        }

        runtime[idx].state = UnitState::STARTING;

        uint32_t elf_size = 0;
        const uint8_t *elf_data = vfs::read_file(u.exec_start, &elf_size);
        if (!elf_data) {
            runtime[idx].state = UnitState::FAILED;
            all_ok = false;
            CHAR16 buf[160];
            UnicodeSPrint(buf, sizeof(buf),
                (CHAR16 *)L"init: %a FAILED (ExecStart binary not found: %a)\n",
                u.name, u.exec_start);
            Print(buf);
            continue;
        }

        int pid = preempt_create_process_from_elf(elf_data, elf_size);
        if (pid < 0) {
            runtime[idx].state = UnitState::FAILED;
            all_ok = false;
            CHAR16 buf[160];
            UnicodeSPrint(buf, sizeof(buf),
                (CHAR16 *)L"init: %a FAILED (process creation failed for %a)\n",
                u.name, u.exec_start);
            Print(buf);
            continue;
        }

        runtime[idx].pid = pid;
        runtime[idx].state = UnitState::ACTIVE;
        CHAR16 buf[160];
        UnicodeSPrint(buf, sizeof(buf),
            (CHAR16 *)L"init: %a started (pid=%d)\n", u.name, pid);
        Print(buf);
    }

    return all_ok;
}

void init::InitSystem::print_summary() const {
    Print((CHAR16 *)L"--- init summary ---\n");
    for (uint32_t i = 0; i < registry.count; i++) {
        CHAR16 buf[160];
        UnicodeSPrint(buf, sizeof(buf), (CHAR16 *)L"  %a: %s (pid=%d)\n",
            registry.units[i].name, state_name(runtime[i].state), runtime[i].pid);
        Print(buf);
    }
}
