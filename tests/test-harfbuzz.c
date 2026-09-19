#include <stdio.h>
#include <hb.h>

int main(void) {
    printf("HarfBuzz %s\n", hb_version_string());
    hb_buffer_t *b = hb_buffer_create();
    if (!b) return 10;
    hb_buffer_add_utf8(b, "BlockOS", -1, 0, -1);
    hb_buffer_guess_segment_properties(b);
    hb_buffer_clear_contents(b);
    hb_buffer_destroy(b);
    return 0;
}
