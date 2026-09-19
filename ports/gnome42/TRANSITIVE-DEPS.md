# Transitive dependencies

GNOME 42 also requires lower-level libraries depending on enabled backends: zlib,
libpng, libjpeg-turbo, brotli, bzip2, pcre2, libxml2, SQLite, X11/XCB, xkbcommon,
libdrm/Mesa or another renderer, and SpiderMonkey for GJS. These must exist in the
BlockOS sysroot before the corresponding package reaches its configure stage.

The package intentionally does not silently substitute host Linux libraries. If one of
these is missing, the cross build should fail and the missing `.pc`, header, or symbol
must be added to the BlockOS port set.
