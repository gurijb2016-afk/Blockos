#include "unit_file.hpp"
#include <string.h>

// Egyszerű, allokáció-mentes INI parser. Nincs dinamikus memória —
// minden fix méretű bufferbe megy (lásd unit_file.hpp), konzisztensen
// a kódbázis többi freestanding struktúrájával (pl. SystemdUnit
// fix tömbökkel).

namespace {

enum class Section { NONE, UNIT, SERVICE, INSTALL };

void copy_bounded(char *dst, size_t dst_size, const char *src, size_t src_len) {
    size_t n = (src_len < dst_size - 1) ? src_len : dst_size - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

// Egy sorból kivágja a whitespace-t elöl/hátul; visszaadja az új
// hosszt, *start-ot pedig a tényleges tartalom kezdetére állítja.
size_t trim(const char **start, size_t len) {
    const char *s = *start;
    while (len > 0 && (s[0] == ' ' || s[0] == '\t' || s[0] == '\r')) { s++; len--; }
    while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t' || s[len - 1] == '\r')) len--;
    *start = s;
    return len;
}

bool line_equals(const char *line, size_t len, const char *literal) {
    size_t lit_len = strlen(literal);
    return len == lit_len && memcmp(line, literal, lit_len) == 0;
}

// key=value sor: visszaadja true-t ha a sor a `key` prefixummel
// kezdődik ("Key=" formában), és beállítja value/value_len-t a "="
// utáni (whitespace-mentesített) részre.
bool match_key(const char *line, size_t len, const char *key,
               const char **value, size_t *value_len) {
    size_t key_len = strlen(key);
    if (len <= key_len) return false;
    if (memcmp(line, key, key_len) != 0) return false;
    if (line[key_len] != '=') return false;
    const char *v = line + key_len + 1;
    size_t vlen = len - key_len - 1;
    vlen = trim(&v, vlen);
    *value = v;
    *value_len = vlen;
    return true;
}

void add_dep(char deps[][init::MAX_UNIT_NAME], uint8_t *count,
             const char *value, size_t value_len) {
    if (*count >= init::MAX_UNIT_DEPS) return; // csendben eldob a limit felett
    copy_bounded(deps[*count], init::MAX_UNIT_NAME, value, value_len);
    (*count)++;
}

} // namespace

bool init::parse_unit_file(const char *text, size_t text_len,
                            const char *unit_name, UnitFile *out) {
    memset(out, 0, sizeof(*out));
    copy_bounded(out->name, MAX_UNIT_NAME, unit_name, strlen(unit_name));
    out->type = ServiceType::SIMPLE;

    Section section = Section::NONE;
    size_t pos = 0;

    while (pos < text_len) {
        // Egy sor kigyűjtése
        size_t line_start = pos;
        while (pos < text_len && text[pos] != '\n') pos++;
        const char *line = text + line_start;
        size_t line_len = pos - line_start;
        if (pos < text_len) pos++; // átlépjük a '\n'-t

        line_len = trim(&line, line_len);
        if (line_len == 0) continue;
        if (line[0] == '#' || line[0] == ';') continue; // komment sor

        if (line[0] == '[') {
            if (line_equals(line, line_len, "[Unit]")) section = Section::UNIT;
            else if (line_equals(line, line_len, "[Service]")) section = Section::SERVICE;
            else if (line_equals(line, line_len, "[Install]")) section = Section::INSTALL;
            else section = Section::NONE; // ismeretlen szekció, tartalmát figyelmen kívül hagyjuk
            continue;
        }

        const char *val;
        size_t val_len;

        if (section == Section::UNIT) {
            if (match_key(line, line_len, "Description", &val, &val_len)) {
                copy_bounded(out->description, MAX_UNIT_DESC, val, val_len);
            } else if (match_key(line, line_len, "After", &val, &val_len)) {
                add_dep(out->after, &out->after_count, val, val_len);
            } else if (match_key(line, line_len, "Requires", &val, &val_len)) {
                add_dep(out->requires_, &out->requires_count, val, val_len);
            }
        } else if (section == Section::SERVICE) {
            if (match_key(line, line_len, "ExecStart", &val, &val_len)) {
                copy_bounded(out->exec_start, MAX_UNIT_PATH, val, val_len);
                out->has_exec_start = true;
            } else if (match_key(line, line_len, "Type", &val, &val_len)) {
                if (val_len == 7 && memcmp(val, "oneshot", 7) == 0)
                    out->type = ServiceType::ONESHOT;
                else
                    out->type = ServiceType::SIMPLE;
            }
        }
        // [Install] szekció (WantedBy=) jelenleg nincs felhasználva —
        // ez az init rendszer minden talált unitot megpróbál indítani
        // függőségi sorrendben, nem "target" alapú kiválasztással.
    }

    return out->has_exec_start;
}
