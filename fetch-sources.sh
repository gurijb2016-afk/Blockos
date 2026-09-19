#!/bin/sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
. "$ROOT/config/environment.sh"
. "$ROOT/ports/gnome42/versions.env"

fetch() {
  name="$1"; url="$2"; out="$GNOME_SOURCES/$name.tar.xz"
  [ -s "$out" ] && { echo "[fetch] $name already present"; return; }
  echo "[fetch] $name"
  if command -v curl >/dev/null 2>&1; then curl -L --fail --retry 3 -o "$out" "$url"; else wget -O "$out" "$url"; fi
}

fetch libffi-${LIBFFI_VERSION} "https://github.com/libffi/libffi/releases/download/v${LIBFFI_VERSION}/libffi-${LIBFFI_VERSION}.tar.gz"
fetch pixman-${PIXMAN_VERSION} "https://cairographics.org/releases/pixman-${PIXMAN_VERSION}.tar.gz"
fetch cairo-${CAIRO_VERSION} "https://cairographics.org/releases/cairo-${CAIRO_VERSION}.tar.xz"
fetch freetype-${FREETYPE_VERSION} "https://download.savannah.gnu.org/releases/freetype/freetype-${FREETYPE_VERSION}.tar.xz"
fetch fontconfig-${FONTCONFIG_VERSION} "https://www.freedesktop.org/software/fontconfig/release/fontconfig-${FONTCONFIG_VERSION}.tar.xz"
fetch harfbuzz-${HARFBUZZ_VERSION} "https://github.com/harfbuzz/harfbuzz/releases/download/${HARFBUZZ_VERSION}/harfbuzz-${HARFBUZZ_VERSION}.tar.xz"
fetch fribidi-${FRIBIDI_VERSION} "https://github.com/fribidi/fribidi/releases/download/v${FRIBIDI_VERSION}/fribidi-${FRIBIDI_VERSION}.tar.xz"
fetch glib-${GLIB_VERSION} "https://download.gnome.org/sources/glib/2.72/glib-${GLIB_VERSION}.tar.xz"
fetch pango-${PANGO_VERSION} "https://download.gnome.org/sources/pango/1.50/pango-${PANGO_VERSION}.tar.xz"
fetch gdk-pixbuf-${GDKPIXBUF_VERSION} "https://download.gnome.org/sources/gdk-pixbuf/2.42/gdk-pixbuf-${GDKPIXBUF_VERSION}.tar.xz"
fetch gtk+-${GTK3_VERSION} "https://download.gnome.org/sources/gtk+/3.24/gtk+-${GTK3_VERSION}.tar.xz"
fetch gtk-${GTK4_VERSION} "https://download.gnome.org/sources/gtk/4.6/gtk-${GTK4_VERSION}.tar.xz"
fetch dbus-${DBUS_VERSION} "https://dbus.freedesktop.org/releases/dbus/dbus-${DBUS_VERSION}.tar.xz"
fetch dconf-${DCONF_VERSION} "https://download.gnome.org/sources/dconf/0.40/dconf-${DCONF_VERSION}.tar.xz"
fetch gsettings-desktop-schemas-${GSETTINGS_DESKTOP_SCHEMAS_VERSION} "https://download.gnome.org/sources/gsettings-desktop-schemas/42/gsettings-desktop-schemas-${GSETTINGS_DESKTOP_SCHEMAS_VERSION}.tar.xz"
fetch gvfs-${GVFS_VERSION} "https://download.gnome.org/sources/gvfs/1.50/gvfs-${GVFS_VERSION}.tar.xz"
fetch gnome-desktop-${GNOME_DESKTOP_VERSION} "https://download.gnome.org/sources/gnome-desktop/42/gnome-desktop-${GNOME_DESKTOP_VERSION}.tar.xz"
fetch gjs-${GJS_VERSION} "https://download.gnome.org/sources/gjs/1.72/gjs-${GJS_VERSION}.tar.xz"
fetch mutter-${MUTTER_VERSION} "https://download.gnome.org/sources/mutter/42/mutter-${MUTTER_VERSION}.tar.xz"
fetch gnome-session-${GNOME_SESSION_VERSION} "https://download.gnome.org/sources/gnome-session/42/gnome-session-${GNOME_SESSION_VERSION}.tar.xz"
fetch gnome-settings-daemon-${GNOME_SETTINGS_DAEMON_VERSION} "https://download.gnome.org/sources/gnome-settings-daemon/42/gnome-settings-daemon-${GNOME_SETTINGS_DAEMON_VERSION}.tar.xz"
fetch gnome-shell-${GNOME_SHELL_VERSION} "https://download.gnome.org/sources/gnome-shell/42/gnome-shell-${GNOME_SHELL_VERSION}.tar.xz"

echo "[fetch] complete: $GNOME_SOURCES"
