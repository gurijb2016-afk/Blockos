#include <stdio.h>
#include <pango/pango.h>
#include <pango/pangocairo.h>

int main(void) {
    PangoFontMap *map = pango_cairo_font_map_get_default();
    if (!map) return 10;
    PangoContext *ctx = pango_font_map_create_context(map);
    if (!ctx) return 11;
    PangoLayout *layout = pango_layout_new(ctx);
    if (!layout) return 12;
    pango_layout_set_text(layout, "BlockOS GNOME 42", -1);
    int w = 0, h = 0;
    pango_layout_get_pixel_size(layout, &w, &h);
    printf("Pango %s layout=%dx%d\n", pango_version_string(), w, h);
    g_object_unref(layout);
    g_object_unref(ctx);
    return 0;
}
