# BlockOS Kernel Crypto

A freestanding-friendly C++ cryptographic component for BlockOS. The code uses
fixed-size state and caller-owned buffers; it does not use STL containers,
heap allocation, exceptions, RTTI, or Linux kernel APIs.

## Implemented algorithms

| Component | Implementation | Status |
|---|---|---|
| SHA-256 | `hash/sha256.cpp` | Implemented; streaming and one-shot APIs |
| SHA-512 | `hash/sha512.cpp` | Implemented; one-shot API |
| SHA3-256 | `hash/sha3.cpp` | Implemented; one-shot API |
| BLAKE2s-256 | `hash/blake2.cpp` | Implemented; one-shot API |
| ChaCha20 | `cipher/chacha20.cpp` | RFC 8439 IETF 96-bit nonce variant |
| Poly1305 | `cipher/poly1305.cpp` | Streaming and one-shot APIs |
| ChaCha20-Poly1305 | `cipher/chacha20_poly1305.cpp` | AEAD encrypt/decrypt; verifies tag before decrypting |
| HMAC-SHA-256 | `kdf/hmac_sha256.cpp` | Streaming and one-shot APIs |
| HKDF-SHA-256 | `kdf/hkdf.cpp` | RFC 5869 extract-and-expand |
| PBKDF2-HMAC-SHA-256 | `kdf/pbkdf2.cpp` | RFC 8018 derivation |
| HMAC-DRBG-SHA-256 | `random/csprng.cpp` | Seeded by a BlockOS platform entropy provider |
| Volatile key vault | `keys/key_manager.cpp` | Fixed-capacity, memory-only key handles and usage checks |
| SHA-256 integrity check | `integrity/` | Checksum generation and constant-time verification |

Built-in hash algorithms are registered by `BlockOS::Crypto::Initialize()` and
are available through `Hash("SHA-256" | "SHA-512" | "SHA3-256" | "BLAKE2s-256", ...)`.

## Not implemented in this package

AES/AES-GCM, XChaCha20, RSA, ECDSA, Ed25519, X25519, Argon2id, ML-KEM,
ML-DSA, SLH-DSA, TLS, persistent key storage, and dynamic crypto-module loading
are **not implemented**. They are intentionally absent rather than represented
by empty functions or success-shaped placeholders. The BlockOS VFS and kernel
module loader integration is also not included.

## Build and test on a host

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

The library target is compiled with freestanding/no-exception/no-RTTI flags.
The host test executable uses the host C runtime only for test output.
Known-answer tests cover SHA-256, SHA-512, SHA3-256, BLAKE2s-256, ChaCha20,
Poly1305, ChaCha20-Poly1305, HKDF-SHA-256, and PBKDF2-HMAC-SHA-256. The
ChaCha20-Poly1305 expected output was cross-checked against Python's
`cryptography` implementation when preparing this package.

## BlockOS integration

1. Add all `.cpp` files under `core/`, `hash/`, `cipher/`, `kdf/`, `random/`,
   `keys/`, `integrity/`, and `security/` to the BlockOS kernel build.
2. Include headers from `kernel/crypto/api/` or the specific component headers.
3. Call `BlockOS::Crypto::Initialize()` during kernel initialization. It
   registers the built-in hashes and runs startup known-answer self-tests.
4. After the platform entropy subsystem is ready, register a static
   `RandomProvider` whose callback supplies 48 bytes of conditioned entropy,
   then call `CSPRNGInitialize()`. Set `approved_for_security_use=true` only
   after the source has been reviewed for entropy quality and health monitoring.
5. Call `BlockOS::Crypto::Shutdown()` before crypto state is torn down; this
   wipes the DRBG and volatile key vault and clears providers/registrations.
6. Serialize registry, DRBG, and volatile-key-vault operations with the
   BlockOS kernel's lock/interrupt primitives. This component deliberately does
   not invent a platform locking API.

Example platform entropy provider:

```cpp
static BlockOS::Crypto::CryptoResult PlatformEntropy(uint8_t* out, size_t n) {
    // Replace with BlockOS's reviewed entropy collector.
    return BlockOS::Hardware::FillConditionedEntropy(out, n);
}

static const BlockOS::Crypto::RandomProvider provider = {
    "blockos-platform-entropy", 1, &PlatformEntropy, true
};

// After Crypto::Initialize() and after the entropy subsystem is ready:
BlockOS::Crypto::RegisterRandomProvider(&provider);
BlockOS::Crypto::CSPRNGInitialize();
```

The example's `BlockOS::Hardware::FillConditionedEntropy` is an integration
placeholder that must be replaced with the real BlockOS implementation; it is
not supplied by this package.

## Important security limits

This is a substantial working implementation, **not a certified or audited
cryptographic module**. Passing known-answer tests proves conformance for those
test cases, not resistance to every attack. Before production use, run larger
official test-vector suites, fuzzing, static analysis, 32/64-bit and compiler
matrix tests, side-channel review, fault-injection tests, and an independent
cryptographic audit. Protect global state with BlockOS locks. Do not claim
Common Criteria/EUCC certification based on this source alone.

API constraints:
- ChaCha20 and AEAD allow exact in-place input/output; arbitrary partial buffer
  overlap is not supported.
- AEAD nonce reuse with the same key is forbidden.
- The CSPRNG is unavailable until a reviewed platform entropy provider is
  registered and `CSPRNGInitialize()` succeeds. Requests are limited to 64 KiB.
- The DRBG requires reseeding after its configured request interval; a reseed
  must contain at least 32 bytes of platform-provided entropy input.
- The volatile key vault stores key bytes only in kernel memory and does not
  persist them or protect against arbitrary kernel-memory compromise.
