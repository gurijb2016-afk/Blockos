# Kernel integration checklist

## Build

Compile the implementation files below as part of BlockOS. Keep the library
freestanding: no STL, heap allocator, exceptions, RTTI, or host OS calls are used.

- `core/`, `hash/`, `cipher/`, `kdf/`, `random/`, `keys/`, `integrity/`, `security/`

Suggested BlockOS-compatible compiler flags include the project's existing
freestanding options (`-ffreestanding -fno-exceptions -fno-rtti -fno-builtin`).
Do not blindly copy the host CMake command into the kernel build.

## Initialization order

1. `Crypto::Initialize()` registers SHA-256, SHA-512, SHA3-256 and BLAKE2s-256,
   then runs known-answer self-tests.
2. Initialize the platform entropy collection subsystem.
3. Register a static `Crypto::RandomProvider` that returns conditioned entropy.
4. Call `Crypto::CSPRNGInitialize()`; do not use random-dependent services before
   this succeeds.
5. Use the CSPRNG for `GenerateVolatileKey()` and other kernel random needs.
6. Call `Crypto::Shutdown()` during orderly kernel shutdown.

## Required BlockOS work outside this directory

- Supply conditioned entropy with source health tests, reseed policy and failure
  reporting. RDRAND alone must not automatically be treated as a sufficient
  entropy source.
- Serialize registry, DRBG and key-vault global state using BlockOS locks.
- Add VFS-based key persistence only with a separately designed access-control
  and authenticated-storage scheme. The included key vault is volatile only.
- Implement signed external-module discovery only after integrating BlockOS VFS,
  module loader, signature verification, ABI/version checks and rollback policy.
- Add platform-level boot-chain verification and signed update integration.

## Verified functions

The host CTest target covers the known-answer vectors listed in `README.md`.
This validates the tested algorithms and code paths, not kernel integration or
security certification.
