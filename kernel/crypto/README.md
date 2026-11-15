# BlockOS Kernel Crypto Framework

C++-only, OS-independent starting framework for `kernel/crypto/`.

## Status

This archive is a framework/scaffold, **not a security-certified cryptographic subsystem**.

Implemented:
- fixed-capacity algorithm registry (no heap allocation required)
- SHA-256 one-shot hashing
- constant-time byte comparison
- explicit secure-zero routine
- caller-supplied cryptographically secure random provider interface
- startup self-test for SHA-256
- algorithm/provider/module interfaces

Declared but intentionally not implemented:
- SHA-512, SHA-3, BLAKE2/3
- AES-GCM, ChaCha20-Poly1305
- Ed25519, X25519, RSA, ECDSA
- HKDF, PBKDF2, Argon2id
- ML-KEM, ML-DSA, SLH-DSA
- filesystem encryption, TLS, key storage and dynamic module loading

Unavailable operations return `CryptoResult::NotImplemented` or
`CryptoResult::NotSupported`. They never silently fall back to an insecure
algorithm or pretend to succeed.

## Layout

- `core/`: common types, initialization, registry and provider interfaces
- `hash/`, `cipher/`, `asymmetric/`, `kdf/`, `post_quantum/`: algorithm entry points
- `random/`: entropy and CSPRNG provider contracts
- `keys/`: key lifecycle interfaces
- `integrity/`: digest/signature verification interfaces
- `hardware/`: CPU feature/provider hooks
- `modules/`: signed external-module discovery/validation contracts
- `security/`: constant-time and memory utilities, self-tests
- `api/`: public API headers
- `tests/`: minimal host-side smoke test

## Integration notes for BlockOS

1. Add the `.cpp` files to the BlockOS kernel build, or compile the directory as a
   separate kernel component.
2. Call `BlockOS::Crypto::Initialize()` during kernel initialization.
3. Supply a real platform entropy source before enabling random-dependent features.
   Do not use `rand()`, timestamps, counters, or RDRAND alone as a guaranteed
   entropy source. Mix only entropy sources whose properties have been assessed.
4. Connect module discovery to the BlockOS VFS and module loader. The module
   interfaces in this archive do not load arbitrary files by themselves.
5. Keep all cryptographic operations behind the public API and check every result.

## Build (host smoke test)

```sh
cmake -S . -B build
cmake --build build
./build/crypto_smoke_test
```

The host smoke test is not a substitute for kernel testing, test vectors,
fuzzing, side-channel review or independent cryptographic evaluation.

## Safety requirements before production use

- Replace each unimplemented algorithm only with a vetted implementation and
  verify it against official test vectors.
- Add known-answer tests, negative tests, fuzzing and fault-injection tests.
- Implement kernel locking/serialization around registry and provider changes.
- Define key ownership, lifetime, permissions and zeroization guarantees.
- Use authenticated encryption (AEAD); never design a new cipher or protocol.
- Do not enable external kernel modules until signature verification, ABI
  compatibility checks and rollback protection are implemented.
- Avoid claiming Common Criteria/EUCC certification until an accredited
  evaluation has actually been completed.
