#include "unit_registry.hpp"
#include "../../fs/vfs.hpp"
#include <string.h>

namespace {

bool starts_with(const char *s, const char *prefix) {
    size_t plen = strlen(prefix);
    return strncmp(s, prefix, plen) == 0;
}

bool ends_with(const char *s, const char *suffix) {
    size_t slen = strlen(s);
    size_t suflen = strlen(suffix);
    if (suflen > slen) return false;
    return strcmp(s + (slen - suflen), suffix) == 0;
}

// A fájlnévből (teljes path) kivágja az utolsó '/' utáni részt, ez
// lesz a unit neve (pl. "/system/services/network.service" ->
// "network.service").
const char *basename_of(const char *path) {
    const char *last_slash = nullptr;
    for (const char *p = path; *p; p++) {
        if (*p == '/') last_slash = p;
    }
    return last_slash ? last_slash + 1 : path;
}

} // namespace

uint32_t init::UnitRegistry::scan(const char *dir_prefix,
                                   const char **bad_units_out,
                                   uint32_t *bad_units_count,
                                   uint32_t bad_units_capacity) {
    count = 0;
    if (bad_units_count) *bad_units_count = 0;

    size_t total = vfs::count_files();
    for (size_t i = 0; i < total; i++) {
        const char *name = vfs::name_at(i);
        if (!name) continue;
        if (!starts_with(name, dir_prefix)) continue;
        if (!ends_with(name, ".service")) continue;

        uint32_t out_size = 0;
        const uint8_t *data = vfs::read_file(name, &out_size);
        if (!data) continue;

        if (count >= MAX_UNITS) break; // csendben eldob a limit felett

        const char *unit_name = basename_of(name);
        UnitFile parsed;
        bool ok = parse_unit_file((const char *)data, out_size, unit_name, &parsed);

        if (ok) {
            units[count++] = parsed;
        } else if (bad_units_out && bad_units_count && *bad_units_count < bad_units_capacity) {
            bad_units_out[(*bad_units_count)++] = unit_name;
        }
    }
    return count;
}

init::UnitFile *init::UnitRegistry::find(const char *name) {
    for (uint32_t i = 0; i < count; i++) {
        if (strcmp(units[i].name, name) == 0) return &units[i];
    }
    return nullptr;
}
