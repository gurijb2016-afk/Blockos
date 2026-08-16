#!/usr/bin/env bash
set -Eeuo pipefail

# ============================================================
# BlockOS rootfs builder
# ============================================================

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOTFS="${ROOTFS:-$ROOT_DIR/rootfs}"
BUILD="${BUILD:-$ROOT_DIR/build}"
SRC="${SRC:-$ROOT_DIR/src}"
USERSpace="${USERSPACE:-$ROOT_DIR/userspace}"
APPS="${APPS:-$ROOT_DIR/apps}"
CONFIG="${CONFIG:-$ROOT_DIR/config}"
SERVICES="${SERVICES:-$ROOT_DIR/services}"

log() {
    printf '[BlockOS] %s\n' "$*"
}

die() {
    printf '[BlockOS][ERROR] %s\n' "$*" >&2
    exit 1
}

copy_if_exists() {
    local source="$1"
    local destination="$2"

    if [[ -e "$source" ]]; then
        mkdir -p "$destination"
        cp -a "$source" "$destination/"
        log "Installed: $source -> $destination/"
    fi
}

copy_tree_if_exists() {
    local source="$1"
    local destination="$2"

    if [[ -d "$source" ]]; then
        mkdir -p "$destination"
        cp -a "$source"/. "$destination"/
        log "Installed tree: $source -> $destination"
    fi
}

# ------------------------------------------------------------
# Clean/create rootfs
# ------------------------------------------------------------

log "Creating BlockOS rootfs..."

rm -rf "$ROOTFS"

mkdir -p \
    "$ROOTFS/system/bin" \
    "$ROOTFS/system/lib" \
    "$ROOTFS/system/config" \
    "$ROOTFS/system/services" \
    "$ROOTFS/system/firmware" \
    "$ROOTFS/bin" \
    "$ROOTFS/devices" \
    "$ROOTFS/proc" \
    "$ROOTFS/tmp" \
    "$ROOTFS/home" \
    "$ROOTFS/apps" \
    "$ROOTFS/init" \
    "$ROOTFS/boot" \
    "$ROOTFS/etc" \
    "$ROOTFS/var/log" \
    "$ROOTFS/var/cache" \
    "$ROOTFS/var/state" \
    "$ROOTFS/data"

# ------------------------------------------------------------
# init
# ------------------------------------------------------------

if [[ -f "$BUILD/init" ]]; then
    cp "$BUILD/init" "$ROOTFS/init/init"
elif [[ -f "$SRC/init/init" ]]; then
    cp "$SRC/init/init" "$ROOTFS/init/init"
elif [[ -f "$SRC/init" ]]; then
    cp "$SRC/init" "$ROOTFS/init/init"
else
    die "BlockOS init was not found."
fi

chmod 0755 "$ROOTFS/init/init"

# ------------------------------------------------------------
# Dynamic linker
# ------------------------------------------------------------

if [[ -f "$BUILD/lib/ld.so" ]]; then
    cp "$BUILD/lib/ld.so" "$ROOTFS/system/lib/ld.so"
elif [[ -f "$BUILD/ld.so" ]]; then
    cp "$BUILD/ld.so" "$ROOTFS/system/lib/ld.so"
elif [[ -f "$USERSpace/libs/build/ld.so" ]]; then
    cp "$USERSpace/libs/build/ld.so" "$ROOTFS/system/lib/ld.so"
else
    log "WARNING: ld.so not found; install it before building a dynamic userspace."
fi

# ------------------------------------------------------------
# Shared libraries
# ------------------------------------------------------------

install_so_files() {
    local found=0

    while IFS= read -r -d '' lib; do
        found=1
        cp -a "$lib" "$ROOTFS/system/lib/"
        log "Shared library: $(basename "$lib")"
    done < <(
        find "$BUILD" "$USERSpace" \
            -type f \
            -name '*.so' \
            -print0 2>/dev/null
    )

    if [[ "$found" -eq 0 ]]; then
        log "No shared libraries found."
    fi
}

install_so_files

# ------------------------------------------------------------
# System binaries
# ------------------------------------------------------------

copy_tree_if_exists "$BUILD/system/bin" "$ROOTFS/system/bin"
copy_tree_if_exists "$BUILD/bin" "$ROOTFS/bin"

copy_tree_if_exists "$SRC/userspace/bin" "$ROOTFS/bin"
copy_tree_if_exists "$USERSpace/bin" "$ROOTFS/bin"

# ------------------------------------------------------------
# BlockOS services
# ------------------------------------------------------------

copy_tree_if_exists "$SERVICES" "$ROOTFS/system/services"

if [[ -d "$SRC/etc/services.conf" ]]; then
    cp -a "$SRC/etc/services.conf" "$ROOTFS/system/services/"
elif [[ -f "$SRC/etc/services.conf" ]]; then
    cp "$SRC/etc/services.conf" "$ROOTFS/system/services/services.conf"
fi

# ------------------------------------------------------------
# Configuration
# ------------------------------------------------------------

copy_tree_if_exists "$CONFIG" "$ROOTFS/system/config"

if [[ -d "$SRC/etc" ]]; then
    copy_tree_if_exists "$SRC/etc" "$ROOTFS/etc"
fi

# ------------------------------------------------------------
# Applications
# ------------------------------------------------------------

copy_tree_if_exists "$APPS" "$ROOTFS/apps"

# ------------------------------------------------------------
# Firmware
# ------------------------------------------------------------

copy_tree_if_exists "$BUILD/firmware" "$ROOTFS/system/firmware"
copy_tree_if_exists "$SRC/firmware" "$ROOTFS/system/firmware"

# ------------------------------------------------------------
# Boot files
# ------------------------------------------------------------

copy_tree_if_exists "$BUILD/boot" "$ROOTFS/boot"

# ------------------------------------------------------------
# Device/proc mount points
# ------------------------------------------------------------

mkdir -p \
    "$ROOTFS/devices" \
    "$ROOTFS/proc" \
    "$ROOTFS/tmp"

chmod 1777 "$ROOTFS/tmp"

# ------------------------------------------------------------
# Basic filesystem metadata
# ------------------------------------------------------------

cat > "$ROOTFS/etc/rootfs.conf" <<'EOF'
# BlockOS root filesystem
ROOTFS_NAME=BlockOS
ROOTFS_VERSION=1
DYNAMIC_LINKER=/system/lib/ld.so
SYSTEM_LIB=/system/lib
SYSTEM_BIN=/system/bin
USER_BIN=/bin
DEVICE_ROOT=/devices
PROC_ROOT=/proc
EOF

# ------------------------------------------------------------
# Validate
# ------------------------------------------------------------

log "Validating rootfs..."

[[ -x "$ROOTFS/init/init" ]] \
    || die "rootfs/init/init is missing or not executable."

if [[ -f "$ROOTFS/system/lib/ld.so" ]]; then
    chmod 0755 "$ROOTFS/system/lib/ld.so"
    log "ld.so: OK"
else
    log "WARNING: /system/lib/ld.so is missing."
fi

# Check shared-library ELF files when readelf is available.
if command -v readelf >/dev/null 2>&1; then
    while IFS= read -r -d '' lib; do
        if ! readelf -h "$lib" >/dev/null 2>&1; then
            die "Invalid ELF shared library: $lib"
        fi
    done < <(
        find "$ROOTFS/system/lib" \
            -type f \
            -name '*.so' \
            -print0
    )
fi

# ------------------------------------------------------------
# Summary
# ------------------------------------------------------------

FILE_COUNT="$(find "$ROOTFS" -type f | wc -l)"
ROOTFS_SIZE="$(du -sh "$ROOTFS" | awk '{print $1}')"

echo
echo "=============================================="
echo " BlockOS rootfs created successfully"
echo "=============================================="
echo " Rootfs : $ROOTFS"
echo " Files  : $FILE_COUNT"
echo " Size   : $ROOTFS_SIZE"
echo
echo " Important paths:"
echo "   /init/init"
echo "   /system/lib/ld.so"
echo "   /system/lib/*.so"
echo "   /system/bin/"
echo "   /bin/"
echo "   /devices/"
echo "   /proc/"
echo "   /apps/"
echo "=============================================="
