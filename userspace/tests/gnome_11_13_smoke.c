#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <gio/gio.h>

int main(void) {
    GError *error = NULL;
    GSettings *s = g_settings_new("org.gnome.desktop.interface");
    if (!s) {
        fprintf(stderr, "GSettings init failed\n");
        return 2;
    }
    GVariant *v = g_settings_get_value(s, "color-scheme");
    if (!v) {
        fprintf(stderr, "GSettings read failed\n");
        g_object_unref(s);
        return 3;
    }
    g_print("GSettings: %s\n", g_variant_print(v, TRUE));
    g_variant_unref(v);

    GFile *home = g_file_new_for_path("/home");
    GFileInfo *info = g_file_query_info(home, "standard::type", G_FILE_QUERY_INFO_NONE, NULL, &error);
    if (!info) {
        g_printerr("GIO /home query failed: %s\n", error ? error->message : "unknown");
        if (error) g_error_free(error);
        g_object_unref(home);
        g_object_unref(s);
        return 4;
    }
    g_print("GIO /home type=%u\n", (unsigned)g_file_info_get_file_type(info));
    g_object_unref(info);
    g_object_unref(home);
    g_object_unref(s);
    return 0;
}
