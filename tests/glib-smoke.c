#include <glib.h>
#include <gio/gio.h>
#include <stdio.h>

int main(void) {
    GMainLoop *loop = g_main_loop_new(NULL, FALSE);
    gchar *s = g_strdup("BlockOS GNOME 42 GLib/GObject/GIO OK");
    GBytes *b = g_bytes_new(s, g_strlen(s));
    GObject *obj = g_object_new(G_TYPE_OBJECT, NULL);
    GFile *f = g_file_new_for_path("/System/etc");
    gboolean native = g_file_is_native(f);
    printf("%s; bytes=%zu; native=%d; loop=%p; obj=%p\n",
           (char *)s, g_bytes_get_size(b), native, (void *)loop, (void *)obj);
    g_object_unref(f);
    g_object_unref(obj);
    g_bytes_unref(b);
    g_free(s);
    g_main_loop_unref(loop);
    return 0;
}
