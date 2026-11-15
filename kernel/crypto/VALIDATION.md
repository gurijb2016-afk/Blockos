# Validation record

Validation performed in the build environment:

- GCC 14.2 / CMake Release build: passed, zero compiler warnings.
- Host known-answer test suite via CTest: passed.
- AddressSanitizer + UndefinedBehaviorSanitizer Debug build and tests: passed.
- x86_64-style kernel flags (`-O2 -fPIC -fshort-wchar -mno-red-zone -fvisibility=hidden`) build and tests: passed.
- SHA-256, SHA-512, SHA3-256 and BLAKE2s-256 cross-checks against Python `hashlib` at lengths around padding/block boundaries: passed.
- Poly1305 and ChaCha20-Poly1305 cross-checks against Python `cryptography` at multiple message and AAD lengths: passed.
- Static library undefined-symbol scan: no unresolved host C-library or OS symbols found.

Not performed:

- No boot or runtime test inside BlockOS itself.
- No integration with BlockOS's actual entropy collector, VFS, module loader, filesystem, or network stack.
- No formal cryptographic audit or Common Criteria / EUCC evaluation.
