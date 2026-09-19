#!/bin/sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
. "$ROOT/config/environment.sh"
. "$ROOT/ports/gnome42/versions.env"

JOBS="${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)}"
export JOBS

run() { echo; echo "========== $* =========="; "$@"; }

# Low-level graphics/text libraries first.
run "$ROOT/tools/build-package.sh" libffi-${LIBFFI_VERSION} autotools
run "$ROOT/tools/build-package.sh" pixman-${PIXMAN_VERSION} meson
run "$ROOT/tools/build-package.sh" cairo-${CAIRO_VERSION} meson -Dtests=disabled -Dgtk_doc=false
run "$ROOT/tools/build-package.sh" freetype-${FREETYPE_VERSION} autotools --without-harfbuzz --without-brotli
run "$ROOT/tools/build-package.sh" fontconfig-${FONTCONFIG_VERSION} autotools --disable-docs
run "$ROOT/tools/build-package.sh" harfbuzz-${HARFBUZZ_VERSION} meson -Dtests=disabled -Ddocs=disabled -Dutilities=disabled
run "$ROOT/tools/build-package.sh" fribidi-${FRIBIDI_VERSION} meson -Ddocs=false -Dtests=false

# GLib stack.
run "$ROOT/tools/build-package.sh" glib-${GLIB_VERSION} meson -Dtests=false -Dman=false -Dgtk_doc=false -Dlibmount=disabled -Dselinux=disabled -Dxattr=false
run "$ROOT/tools/build-package.sh" pango-${PANGO_VERSION} meson -Dbuild-tests=false -Dbuild-documentation=false
run "$ROOT/tools/build-package.sh" gdk-pixbuf-${GDKPIXBUF_VERSION} meson -Dtests=false -Dinstalled_tests=false -Ddocs=false

# GTK 3 and GTK 4 are both retained because GNOME 42 still has GTK3 consumers.
run "$ROOT/tools/build-package.sh" gtk+-${GTK3_VERSION} meson -Dtests=false -Dwayland_backend=false -Dx11_backend=true -Dcloudproviders=false -Dcolord=false -Dcups=false
run "$ROOT/tools/build-package.sh" gtk-${GTK4_VERSION} meson -Dbuild-tests=false -Dbuild-documentation=false -Dx11-backend=true -Dwayland-backend=false -Dvulkan=false

# Desktop services.
run "$ROOT/tools/build-package.sh" dbus-${DBUS_VERSION} meson -Dtests=false -Ddocs=false -Dapparmor=false -Dselinux=false -Dsystemd=disabled -Dlaunchd=disabled
run "$ROOT/tools/build-package.sh" dconf-${DCONF_VERSION} meson -Dbash_completion=false -Dman=false -Dgtk_doc=false
run "$ROOT/tools/build-package.sh" gsettings-desktop-schemas-${GSETTINGS_DESKTOP_SCHEMAS_VERSION} meson
run "$ROOT/tools/build-package.sh" gvfs-${GVFS_VERSION} meson -Dsystemduserunitdir=no -Dgudev=false -Dudisks2=false -Dgoa=false -Dgphoto2=false -Dafc=false -Dgoogle=false -Dmtp=false -Dbluray=false -Darchive=false
run "$ROOT/tools/build-package.sh" gnome-desktop-${GNOME_DESKTOP_VERSION} meson -Ddesktop_docs=false

# GNOME runtime pieces. These are intentionally last because they consume nearly all
# of the preceding ABI and graphics stack.
run "$ROOT/tools/build-package.sh" gjs-${GJS_VERSION} meson -Dbuild_tests=false -Dskip_dbus_tests=true
run "$ROOT/tools/build-package.sh" mutter-${MUTTER_VERSION} meson -Dtests=false -Dwayland=false -Dxwayland=false -Dpipewire=false -Dremote-desktop=false -Dnative_backend=false -Dprofiler=false
run "$ROOT/tools/build-package.sh" gnome-session-${GNOME_SESSION_VERSION} meson -Dman=false
run "$ROOT/tools/build-package.sh" gnome-settings-daemon-${GNOME_SETTINGS_DAEMON_VERSION} meson -Dtests=false
run "$ROOT/tools/build-package.sh" gnome-shell-${GNOME_SHELL_VERSION} meson -Dtests=false -Dextensions_tool=false

# Merge the staged /system tree into the configured BlockOS sysroot.
mkdir -p "$BLOCKOS_SYSROOT/System"
cp -a "$GNOME_OUT/System/." "$BLOCKOS_SYSROOT/System/"

echo
echo "GNOME 42 dependency build completed."
echo "Staged rootfs: $GNOME_OUT"
echo "Sysroot:       $BLOCKOS_SYSROOT"
echo "Run ./verify.sh before attempting a QEMU boot."
