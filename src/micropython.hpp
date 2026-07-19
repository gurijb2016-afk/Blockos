#pragma once
#include <stdint.h>
#include <stddef.h>

// A MicroPython interpreter magjának C-stílusú exportált függvényei
extern "C" {
    // Inicializálja a MicroPython runtime-ot és a belső szemétgyűjtőt (Garbage Collector)
    void mp_init(void);
    
    // Leállítja az interpretert és felszabadítja az erőforrásokat
    void mp_deinit(void);
    
    // Kijelöli a MicroPython belső dinamikus memóriaterületét (Heap)
    void gc_init(void *start, void *end);
    
    // Végrehajt egy memóriában lévő nyers Python forráskódot
    int mp_execute_from_str(const char *src);
    
    // Beépített MicroPython típusok a stringkezeléshez és kivételekhez
    typedef struct _mp_obj_t *mp_obj_t;
    void mp_obj_print_exception(void (*print_func)(void*, const char*), void *env, mp_obj_t exc);
}

class BlockOsMicroPython {
private:
    __attribute__((aligned(4096))) uint8_t python_heap[256 * 1024]; // 256 KB izolált RAM a Python szkripteknek
    bool is_initialized;

public:
    BlockOsMicroPython() : is_initialized(false) {}
    
    void boot_interpreter();
    bool run_script(const char* source_code);
    void shutdown_interpreter();
};
