#!/bin/sh
set -eu

export DISPLAY=${DISPLAY:-:0}
export XDG_SESSION_TYPE=x11
export XDG_CURRENT_DESKTOP=GNOME
export DESKTOP_SESSION=gnome-blockos
export GDMSESSION=gnome-blockos
export GDK_BACKEND=x11
export CLUTTER_BACKEND=x11
export GTK_BACKEND=x11
export XDG_CONFIG_HOME=${XDG_CONFIG_HOME:-/home/blockos/.config}
export XDG_DATA_HOME=${XDG_DATA_HOME:-/home/blockos/.local/share}
export XDG_CACHE_HOME=${XDG_CACHE_HOME:-/home/blockos/.cache}
export GSETTINGS_SCHEMA_DIR=${GSETTINGS_SCHEMA_DIR:-/System/share/glib-2.0/schemas}
export DCONF_PROFILE=${DCONF_PROFILE:-user}
export DBUS_SESSION_BUS_ADDRESS=${DBUS_SESSION_BUS_ADDRESS:-unix:path=/tmp/dbus-session}

mkdir -p "$XDG_CONFIG_HOME" "$XDG_DATA_HOME" "$XDG_CACHE_HOME"

# The X11 server must already be running on $DISPLAY.
if [ -x /System/bin/dbusd ]; then
    /System/bin/dbusd --session --address="$DBUS_SESSION_BUS_ADDRESS" &
fi

# GNOME Shell 42 uses GJS; force its X11 mode.
exec /System/bin/gnome-session --session=gnome-blockos
