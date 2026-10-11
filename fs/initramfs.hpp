#pragma once

namespace blockos::initramfs {

// Mount the embedded newc CPIO archive into the BlockOS VFS.
// Returns false if the archive is malformed, an entry cannot be created,
// or a usable ELF64 /bin/sh was not installed.
bool mount();

}
