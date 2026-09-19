#include <cairo.h>
#include <stdio.h>

int main(void) {
    cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 32, 32);
    cairo_t *cr = cairo_create(s);
    cairo_set_source_rgb(cr, 0.1, 0.2, 0.3);
    cairo_paint(cr);
    cairo_destroy(cr);
    cairo_status_t st = cairo_surface_status(s);
    cairo_surface_destroy(s);
    printf("cairo=%s\n", cairo_status_to_string(st));
    return st == CAIRO_STATUS_SUCCESS ? 0 : 1;
}
