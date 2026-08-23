menu "File Systems"

config FS
    bool "File system support"
    default y

if FS

config VFS
    bool "Virtual File System (VFS)"
    default y

menu "BlockOS File Systems"

config FS_EXT4
    bool "EXT4"
    depends on VFS
    default y

config FS_FAT32
    bool "FAT32"
    depends on VFS
    default y

config FS_FAT16
    bool "FAT16"
    depends on VFS
    default n

config FS_EXFAT
    bool "exFAT"
    depends on VFS
    default n

config FS_NTFS
    bool "NTFS"
    depends on VFS
    default n

config FS_LITTLEFS
    bool "LittleFS"
    depends on VFS
    default y

config FS_SQUASHFS
    bool "SquashFS"
    depends on VFS
    default y

config FS_TMPFS
    bool "tmpfs"
    depends on VFS
    default y

config FS_PROC
    bool "procfs"
    depends on VFS
    default y

config FS_SYSFS
    bool "sysfs"
    depends on VFS
    default y

config FS_DEVFS
    bool "devfs"
    depends on VFS
    default y

config FS_OVERLAYFS
    bool "OverlayFS"
    depends on VFS
    default n

endmenu

menu "Compression"

config FS_ZSTD
    bool "Zstandard compression"
    default y

config FS_GZIP
    bool "GZIP compression"
    default y

config FS_LZ4
    bool "LZ4 compression"
    default n

config FS_XZ
    bool "XZ compression"
    default n

endmenu

menu "File System Features"

config FS_PERMISSIONS
    bool "File permissions"
    default y

config FS_SYMLINKS
    bool "Symbolic links"
    default y

config FS_HARDLINKS
    bool "Hard links"
    default y

config FS_MOUNT
    bool "Mount support"
    default y

config FS_UNMOUNT
    bool "Unmount support"
    default y

config FS_JOURNAL
    bool "Journaling support"
    default y

endmenu

endif

endmenu
