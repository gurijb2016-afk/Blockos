# BlockOS GNOME 42 runtime environment.
export XDG_CURRENT_DESKTOP=GNOME
export XDG_SESSION_DESKTOP=gnome
export XDG_RUNTIME_DIR=${XDG_RUNTIME_DIR:-/run/user/0}
export GSETTINGS_SCHEMA_DIR=${GSETTINGS_SCHEMA_DIR:-/System/share/glib-2.0/schemas}
export GI_TYPELIB_PATH=${GI_TYPELIB_PATH:-/System/lib/girepository-1.0}
export GJS_PATH=${GJS_PATH:-/System/share/gjs-1.0}
export GIO_MODULE_DIR=${GIO_MODULE_DIR:-/System/lib/gio/modules}
export GIO_EXTRA_MODULES=${GIO_EXTRA_MODULES:-/System/lib/gio/modules}
export GVFS_METADATA_HOME=${GVFS_METADATA_HOME:-/run/user/0/gvfs-metadata}
