# Kernel integration contract

## Initialization order

1. Initialize the BlockOS memory manager and synchronization primitives.
2. Initialize the VFS and module trust store (if module providers are enabled).
3. Initialize architecture feature detection.
4. Register a vetted random provider backed by the platform CSPRNG.
5. Call `BlockOS::Crypto::Initialize()` and fail closed if it returns an error.

The current framework does not ship a production entropy conditioner or CSPRNG.
Until a reviewed provider is connected, `GenerateRandom()` returns
`EntropyUnavailable`.

## Concurrency

The fixed registry and provider slots are currently intended for serialized
initialization-time use. Before runtime registration/unregistration is enabled,
protect these operations with the BlockOS kernel's lock/interrupt discipline.
Do not add ad-hoc spinlocks without considering interrupt context and lock order.

## External module paths

The module directory is intentionally an integration boundary, not an automatic
loader. The BlockOS module manager must enumerate configured paths, validate
file ownership/permissions, verify a signature against an explicitly trusted
key, enforce ABI and architecture compatibility, enforce anti-rollback policy,
and only then map/execute the module. A path is never a trust decision.
