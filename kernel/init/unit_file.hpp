#pragma once
#include <stdint.h>
#include <stddef.h>

// Valódi, fájlból olvasott systemd-stílusú unit fájl leírók.
// Formátum (INI-szerű, ahogy a rendszeres systemd .service fájljai):
//
//   [Unit]
//   Description=Hálózati alrendszer
//   After=vfs.service
//   Requires=vfs.service
//
//   [Service]
//   ExecStart=/system/bin/networkd
//   Type=simple
//
//   [Install]
//   WantedBy=multi-user.target
//
// Csak azt támogatja, amit ez az init rendszer ténylegesen fel is
// használ: Description, After, Requires, ExecStart, Type. Minden más
// kulcsot figyelmen kívül hagy (nem hiba, csak nincs hatása).

namespace init {

constexpr size_t MAX_UNIT_NAME  = 64;
constexpr size_t MAX_UNIT_DESC  = 128;
constexpr size_t MAX_UNIT_PATH  = 128;
constexpr size_t MAX_UNIT_DEPS  = 8;

enum class ServiceType : uint8_t {
    SIMPLE = 0,  // ExecStart azonnal elindul, nem várunk rá
    ONESHOT = 1, // egyszeri lefutás, utána a unit ACTIVE marad ha 0-val tért vissza
};

struct UnitFile {
    char name[MAX_UNIT_NAME];               // egyedi azonosító, pl. "network.service" (a fájlnévből)
    char description[MAX_UNIT_DESC];
    char after[MAX_UNIT_DEPS][MAX_UNIT_NAME];
    uint8_t after_count;
    char requires_[MAX_UNIT_DEPS][MAX_UNIT_NAME];
    uint8_t requires_count;
    char exec_start[MAX_UNIT_PATH];
    ServiceType type;
    bool has_exec_start;
};

// Egy unit fájl szövegtartalmának feldolgozása. `text` nem kell
// nullterminated legyen, `text_len` adja meg a hosszt. `unit_name`
// az a név, amit a rendszer erre a unitra használ (jellemzően a
// fájlnév, pl. "network.service") — ez kerül out->name-be és ez
// szolgál a függőségi hivatkozások (After=/Requires=) céljaként is.
//
// Return: true, ha talált egy értelmezhető [Service] szekciót
// ExecStart=-tal. Egy unit fájl ExecStart nélkül érvénytelen (nincs
// mit indítani), ezért false-t ad vissza.
bool parse_unit_file(const char *text, size_t text_len,
                      const char *unit_name, UnitFile *out);

} // namespace init
