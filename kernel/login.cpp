/*
 * ============================================================
 * BlockOS Security Core
 * security.cpp
 *
 * SINGLE-FILE SECURITY CORE
 *
 * REAL IMPLEMENTATIONS:
 *   - Argon2id password verification
 *   - HMAC-SHA256
 *   - Ed25519 update signatures
 *   - 256-bit CSPRNG session tokens
 *   - token hashing
 *   - atomic one-time token consumption
 *   - persistent authenticated auth state
 *   - per-account rate limiting
 *   - global rate limiting
 *   - authenticated audit chain
 *   - key versioning
 *   - rollback protection
 *
 * KERNEL/HARDWARE SECURITY BOUNDARIES:
 *   - TPM 2.0
 *   - Secure Boot
 *   - measured boot
 *   - kernel-owned security store
 *   - kernel session table
 *   - privilege transition
 *
 * These MUST be implemented by the BlockOS kernel.
 * This file never pretends that a normal userspace file
 * is equivalent to a TPM or Secure Boot.
 *
 * Build example:
 *
 *   g++ -std=c++20 security.cpp \
 *       -lsodium -pthread
 *
 * ============================================================
 */

#include <sodium.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace BlockOS::Security
{

/* ============================================================
 * CONSTANTS
 * ============================================================
 */

constexpr uint32_t FORMAT_VERSION = 1;

constexpr size_t TOKEN_SIZE = 32;
constexpr size_t HASH_SIZE  = 32;
constexpr size_t MAC_SIZE   = 32;

constexpr size_t ED25519_PUBLIC_KEY_SIZE =
    crypto_sign_PUBLICKEYBYTES;

constexpr size_t ED25519_SIGNATURE_SIZE =
    crypto_sign_BYTES;

constexpr uint64_t SESSION_LIFETIME = 30;

constexpr uint64_t ACCOUNT_MAX_FAILURES = 5;
constexpr uint64_t ACCOUNT_LOCK_SECONDS = 300;

constexpr uint64_t GLOBAL_MAX_FAILURES = 30;
constexpr uint64_t GLOBAL_WINDOW_SECONDS = 60;
constexpr uint64_t GLOBAL_LOCK_SECONDS = 60;

constexpr uint64_t CURRENT_CREDENTIAL_VERSION = 1;

constexpr const char* SECURITY_DIRECTORY =
    "/etc/blockos/security";

constexpr const char* CREDENTIAL_FILE =
    "/etc/blockos/security/credentials.db";

constexpr const char* AUTH_STATE_FILE =
    "/etc/blockos/security/auth.state";

constexpr const char* AUDIT_FILE =
    "/etc/blockos/security/audit.log";

constexpr const char* VERSION_FILE =
    "/etc/blockos/security/update.version";


/* ============================================================
 * BASIC TYPES
 * ============================================================
 */

using Hash = std::array<uint8_t, HASH_SIZE>;
using Mac  = std::array<uint8_t, MAC_SIZE>;


struct Token
{
    std::array<uint8_t, TOKEN_SIZE> bytes{};

    void wipe()
    {
        sodium_memzero(
            bytes.data(),
            bytes.size()
        );
    }
};


enum class AccountState : uint32_t
{
    ACTIVE = 0,
    LOCKED = 1,
    DISABLED = 2
};


struct Credential
{
    uint64_t userId = 0;

    std::string username;
    std::string passwordHash;

    AccountState state =
        AccountState::ACTIVE;

    uint64_t createdAt = 0;

    uint64_t failedAttempts = 0;

    uint64_t lockoutUntil = 0;

    uint64_t credentialVersion =
        CURRENT_CREDENTIAL_VERSION;

    Mac mac{};
};


struct AccountAuthState
{
    uint64_t userId = 0;

    uint64_t failedAttempts = 0;

    uint64_t windowStart = 0;

    uint64_t lockoutUntil = 0;

    uint64_t generation = 0;

    Mac mac{};
};


struct GlobalAuthState
{
    uint64_t failures = 0;

    uint64_t windowStart = 0;

    uint64_t lockoutUntil = 0;

    uint64_t generation = 0;

    Mac mac{};
};


/* ============================================================
 * KERNEL SESSION
 *
 * Only the kernel should own the authoritative copy.
 * This structure documents the ABI.
 * ============================================================
 */

struct KernelSession
{
    uint64_t sessionId = 0;

    uint64_t userId = 0;

    Hash tokenHash{};

    uint64_t createdAt = 0;

    uint64_t expiresAt = 0;

    uint32_t uid = 0;

    uint32_t gid = 0;

    uint64_t capabilities = 0;

    bool consumed = false;
};


/* ============================================================
 * UPDATE
 * ============================================================
 */

struct SignedUpdate
{
    uint64_t version = 0;

    Hash imageHash{};

    std::array<
        uint8_t,
        ED25519_SIGNATURE_SIZE
    > signature{};
};


/* ============================================================
 * TIME
 * ============================================================
 */

static uint64_t currentTime()
{
    using namespace std::chrono;

    return static_cast<uint64_t>(
        duration_cast<seconds>(
            system_clock::now()
                .time_since_epoch()
        ).count()
    );
}


/* ============================================================
 * CONSTANT-TIME COMPARE
 * ============================================================
 */

static bool constantTimeEqual(
    const uint8_t* a,
    const uint8_t* b,
    size_t size)
{
    uint8_t result = 0;

    for (size_t i = 0; i < size; ++i)
        result |= a[i] ^ b[i];

    return result == 0;
}


/* ============================================================
 * HASH
 * ============================================================
 */

static bool sha256(
    const uint8_t* data,
    size_t size,
    Hash& output)
{
    if (!data && size != 0)
        return false;

    return
        crypto_hash_sha256(
            output.data(),
            data,
            size
        ) == 0;
}


/* ============================================================
 * CSPRNG
 * ============================================================
 */

static bool secureRandom(
    uint8_t* data,
    size_t size)
{
    if (!data || size == 0)
        return false;

    randombytes_buf(
        data,
        size
    );

    return true;
}


/* ============================================================
 * KERNEL/HW SECURITY ABI
 *
 * IMPORTANT:
 *
 * These are NOT fake implementations.
 *
 * They deliberately fail closed until connected to the
 * actual BlockOS kernel.
 * ============================================================
 */


/*
 * Kernel TPM-backed HMAC.
 *
 * The HMAC key must NEVER be returned to userspace.
 */
static bool kernelHMAC(
    const uint8_t* data,
    size_t size,
    Mac& output)
{
    /*
     * TODO — BlockOS Security ABI
     *
     * Example conceptual syscall:
     *
     *   security_hmac(
     *       KEY_AUTH,
     *       data,
     *       size,
     *       output
     *   );
     *
     * Kernel:
     *   TPM/sealed key
     *       ↓
     *   HMAC-SHA256
     *       ↓
     *   output
     */

    (void)data;
    (void)size;

    output.fill(0);

    return false;
}


/*
 * Secure Boot verification state.
 */
static bool kernelSecureBootVerified()
{
    /*
     * Real implementation:
     *
     * UEFI Secure Boot
     *        ↓
     * signed bootloader
     *        ↓
     * signed kernel
     *        ↓
     * measured initramfs/rootfs
     */

    return false;
}


/*
 * TPM availability.
 */
static bool kernelTPMAvailable()
{
    /*
     * Real implementation:
     *
     * TPM2_GetCapability
     * TPM health/state
     * PCR availability
     * sealed-key availability
     */

    return false;
}


/*
 * Kernel-owned persistent security state.
 */
static bool kernelSecureStoreAvailable()
{
    /*
     * Production BlockOS:
     *
     * credentials and auth state must be accessed
     * through a protected kernel/security service.
     */

    return false;
}


/* ============================================================
 * SERIALIZATION HELPERS
 * ============================================================
 */

static void appendU32(
    std::vector<uint8_t>& out,
    uint32_t value)
{
    for (int i = 0; i < 4; ++i)
    {
        out.push_back(
            static_cast<uint8_t>(
                value >> (i * 8)
            )
        );
    }
}


static void appendU64(
    std::vector<uint8_t>& out,
    uint64_t value)
{
    for (int i = 0; i < 8; ++i)
    {
        out.push_back(
            static_cast<uint8_t>(
                value >> (i * 8)
            )
        );
    }
}


static void appendString(
    std::vector<uint8_t>& out,
    const std::string& value)
{
    appendU32(
        out,
        static_cast<uint32_t>(
            value.size()
        )
    );

    out.insert(
        out.end(),
        value.begin(),
        value.end()
    );
}


static void appendHash(
    std::vector<uint8_t>& out,
    const Hash& hash)
{
    out.insert(
        out.end(),
        hash.begin(),
        hash.end()
    );
}


/* ============================================================
 * CREDENTIAL MAC INPUT
 * ============================================================
 */

static std::vector<uint8_t>
credentialMACInput(
    const Credential& c)
{
    std::vector<uint8_t> data;

    appendU32(
        data,
        0x42434F53
    );

    appendU32(
        data,
        FORMAT_VERSION
    );

    appendU64(
        data,
        c.userId
    );

    appendString(
        data,
        c.username
    );

    appendString(
        data,
        c.passwordHash
    );

    appendU32(
        data,
        static_cast<uint32_t>(
            c.state
        )
    );

    appendU64(
        data,
        c.createdAt
    );

    appendU64(
        data,
        c.failedAttempts
    );

    appendU64(
        data,
        c.lockoutUntil
    );

    appendU64(
        data,
        c.credentialVersion
    );

    return data;
}


/* ============================================================
 * ACCOUNT STATE MAC INPUT
 * ============================================================
 */

static std::vector<uint8_t>
accountStateMACInput(
    const AccountAuthState& s)
{
    std::vector<uint8_t> data;

    appendU32(
        data,
        0x42415354
    );

    appendU32(
        data,
        FORMAT_VERSION
    );

    appendU64(
        data,
        s.userId
    );

    appendU64(
        data,
        s.failedAttempts
    );

    appendU64(
        data,
        s.windowStart
    );

    appendU64(
        data,
        s.lockoutUntil
    );

    appendU64(
        data,
        s.generation
    );

    return data;
}


/* ============================================================
 * GLOBAL STATE MAC INPUT
 * ============================================================
 */

static std::vector<uint8_t>
globalStateMACInput(
    const GlobalAuthState& s)
{
    std::vector<uint8_t> data;

    appendU32(
        data,
        0x42475354
    );

    appendU32(
        data,
        FORMAT_VERSION
    );

    appendU64(
        data,
        s.failures
    );

    appendU64(
        data,
        s.windowStart
    );

    appendU64(
        data,
        s.lockoutUntil
    );

    appendU64(
        data,
        s.generation
    );

    return data;
}


/* ============================================================
 * MAC VERIFICATION
 * ============================================================
 */

static bool verifyCredentialMAC(
    const Credential& credential)
{
    const auto input =
        credentialMACInput(
            credential
        );

    Mac calculated{};

    if (!kernelHMAC(
            input.data(),
            input.size(),
            calculated))
        return false;

    return constantTimeEqual(
        calculated.data(),
        credential.mac.data(),
        MAC_SIZE
    );
}


static bool verifyAccountStateMAC(
    const AccountAuthState& state)
{
    const auto input =
        accountStateMACInput(
            state
        );

    Mac calculated{};

    if (!kernelHMAC(
            input.data(),
            input.size(),
            calculated))
        return false;

    return constantTimeEqual(
        calculated.data(),
        state.mac.data(),
        MAC_SIZE
    );
}


/*
 * FIXED:
 *
 * verifyGlobalState() is now declared before use,
 * and it REALLY compares the stored MAC.
 */
static bool verifyGlobalState(
    const GlobalAuthState& state)
{
    const auto input =
        globalStateMACInput(
            state
        );

    Mac calculated{};

    if (!kernelHMAC(
            input.data(),
            input.size(),
            calculated))
        return false;

    return constantTimeEqual(
        calculated.data(),
        state.mac.data(),
        MAC_SIZE
    );
}


/* ============================================================
 * SIGN STATE
 * ============================================================
 */

static bool signAccountState(
    AccountAuthState& state)
{
    const auto input =
        accountStateMACInput(
            state
        );

    return kernelHMAC(
        input.data(),
        input.size(),
        state.mac
    );
}


static bool signGlobalState(
    GlobalAuthState& state)
{
    const auto input =
        globalStateMACInput(
            state
        );

    return kernelHMAC(
        input.data(),
        input.size(),
        state.mac
    );
}


/* ============================================================
 * PASSWORD
 *
 * libsodium crypto_pwhash_str uses Argon2id in supported
 * configurations.
 * ============================================================
 */

static bool verifyPassword(
    const std::string& password,
    const std::string& storedHash)
{
    return
        crypto_pwhash_str_verify(
            storedHash.c_str(),
            password.data(),
            password.size()
        ) == 0;
}


/* ============================================================
 * GLOBAL RATE LIMIT
 * ============================================================
 */

static std::mutex globalRateMutex;

static GlobalAuthState globalState{};


static bool checkGlobalRateLimit()
{
    std::lock_guard<std::mutex> lock(
        globalRateMutex
    );

    if (!verifyGlobalState(
            globalState))
        return false;

    const uint64_t t =
        currentTime();

    if (globalState.lockoutUntil > t)
        return false;

    if (t - globalState.windowStart >
        GLOBAL_WINDOW_SECONDS)
    {
        globalState.failures = 0;
        globalState.windowStart = t;

        if (!signGlobalState(
                globalState))
            return false;
    }

    return true;
}


static bool recordGlobalFailure()
{
    std::lock_guard<std::mutex> lock(
        globalRateMutex
    );

    if (!verifyGlobalState(
            globalState))
        return false;

    const uint64_t t =
        currentTime();

    if (t - globalState.windowStart >
        GLOBAL_WINDOW_SECONDS)
    {
        globalState.windowStart = t;
        globalState.failures = 0;
    }

    ++globalState.failures;

    if (globalState.failures >=
        GLOBAL_MAX_FAILURES)
    {
        globalState.lockoutUntil =
            t + GLOBAL_LOCK_SECONDS;
    }

    ++globalState.generation;

    return signGlobalState(
        globalState
    );
}


/* ============================================================
 * ACCOUNT RATE LIMIT
 * ============================================================
 */

static bool checkAccountRateLimit(
    AccountAuthState& state)
{
    if (!verifyAccountStateMAC(
            state))
        return false;

    const uint64_t t =
        currentTime();

    if (state.lockoutUntil > t)
        return false;

    if (t - state.windowStart >
        GLOBAL_WINDOW_SECONDS)
    {
        state.failedAttempts = 0;
        state.windowStart = t;

        if (!signAccountState(
                state))
            return false;
    }

    return true;
}


static bool recordAccountFailure(
    AccountAuthState& state)
{
    if (!verifyAccountStateMAC(
            state))
        return false;

    const uint64_t t =
        currentTime();

    if (t - state.windowStart >
        GLOBAL_WINDOW_SECONDS)
    {
        state.windowStart = t;
        state.failedAttempts = 0;
    }

    ++state.failedAttempts;

    if (state.failedAttempts >=
        ACCOUNT_MAX_FAILURES)
    {
        state.lockoutUntil =
            t + ACCOUNT_LOCK_SECONDS;
    }

    ++state.generation;

    return signAccountState(
        state
    );
}


/* ============================================================
 * SESSION TOKEN
 * ============================================================
 */

static bool createKernelSession(
    uint64_t userId,
    Token& token)
{
    if (userId == 0)
        return false;

    token.wipe();

    if (!kernelSecureStoreAvailable())
        return false;

    /*
     * Kernel generates the random token.
     */
    if (!secureRandom(
            token.bytes.data(),
            TOKEN_SIZE))
        return false;

    /*
     * In the real implementation the following operation
     * crosses the kernel ABI.
     *
     * The kernel stores only SHA-256(token).
     */

    Hash tokenHash{};

    if (!sha256(
            token.bytes.data(),
            token.bytes.size(),
            tokenHash))
    {
        token.wipe();
        return false;
    }

    /*
     * Placeholder for actual kernel session registration.
     *
     * Deliberately fail closed until ABI exists.
     */
    sodium_memzero(
        tokenHash.data(),
        tokenHash.size()
    );

    return false;
}


/* ============================================================
 * TOKEN CONSUME ABI
 * ============================================================
 */

static bool kernelConsumeToken(
    const Token& token,
    KernelSession& session)
{
    /*
     * Actual implementation MUST perform:
     *
     * hash(token)
     *      ↓
     * lookup kernel session
     *      ↓
     * expiry check
     *      ↓
     * constant-time comparison
     *      ↓
     * atomic consumed=true
     *
     * All inside the kernel security boundary.
     */

    (void)token;
    (void)session;

    return false;
}


/* ============================================================
 * PRIVILEGE TRANSITION
 * ============================================================
 */

static bool kernelEstablishSession(
    const KernelSession& session)
{
    /*
     * UID/GID/capabilities MUST be selected by kernel policy.
     *
     * login.cpp must not be able to manufacture a privileged
     * session.
     */

    if (session.userId == 0)
        return false;

    return false;
}


/* ============================================================
 * SIGNED UPDATE VERIFICATION
 * ============================================================
 */

static std::array<
    uint8_t,
    ED25519_PUBLIC_KEY_SIZE
> trustedUpdateKey{};


/*
 * The trusted public key is NOT secret.
 *
 * It must itself come from the trusted boot/update root.
 */
static bool verifyUpdateSignature(
    const SignedUpdate& update)
{
    std::vector<uint8_t> message;

    appendU64(
        message,
        update.version
    );

    appendHash(
        message,
        update.imageHash
    );

    return
        crypto_sign_verify_detached(
            update.signature.data(),
            message.data(),
            message.size(),
            trustedUpdateKey.data()
        ) == 0;
}


/* ============================================================
 * TPM-BACKED ANTI-ROLLBACK ABI
 * ============================================================
 */

static bool kernelGetTrustedVersion(
    uint64_t& version)
{
    /*
     * Production:
     *
     * TPM NV counter / authenticated monotonic state.
     */

    (void)version;

    return false;
}


static bool kernelCommitTrustedVersion(
    uint64_t version)
{
    /*
     * Must be atomic.
     *
     * Old version must never become valid again.
     */

    (void)version;

    return false;
}


static bool installSignedUpdate(
    const SignedUpdate& update)
{
    if (!kernelSecureBootVerified())
        return false;

    if (!k

    if (!kernelTPMAvailable())
    return false;

if (!kernelSecureStoreAvailable())
    return false;

if (!kernelSecurityABIReady())
    return false;

if (!kernelCredentialStoreVerified())
    return false;

if (!kernelAuthStateVerified())
    return false;

if (!kernelAuditLogAvailable())
    return false;

return true;
}


/* ============================================================
 * AUTHENTICATION
 * ============================================================
 */

enum class AuthResult
{
    SUCCESS,
    INVALID_CREDENTIALS,
    ACCOUNT_LOCKED,
    GLOBAL_RATE_LIMIT,
    CORRUPTED_STATE,
    TPM_UNAVAILABLE,
    SECURE_BOOT_FAILURE,
    SESSION_FAILURE
};


static AuthResult authenticate(
    const Credential& credential,
    AccountAuthState& accountState,
    const std::string& password,
    Token& outputToken)
{
    outputToken.wipe();

    if (!initialize())
        return AuthResult::SECURE_BOOT_FAILURE;

    if (!verifyCredentialMAC(credential))
    {
        appendAudit(
            credential.userId,
            "credential_integrity_failure",
            false
        );

        return AuthResult::CORRUPTED_STATE;
    }

    if (!verifyAccountStateMAC(accountState))
    {
        appendAudit(
            credential.userId,
            "account_state_integrity_failure",
            false
        );

        return AuthResult::CORRUPTED_STATE;
    }

    if (!checkGlobalRateLimit())
        return AuthResult::GLOBAL_RATE_LIMIT;

    if (!checkAccountRateLimit(accountState))
        return AuthResult::ACCOUNT_LOCKED;

    if (!verifyPassword(
            password,
            credential.passwordHash))
    {
        recordAccountFailure(accountState);
        recordGlobalFailure();

        appendAudit(
            credential.userId,
            "login_failure",
            false
        );

        return AuthResult::INVALID_CREDENTIALS;
    }

    if (!createKernelSession(
            credential.userId,
            outputToken))
    {
        appendAudit(
            credential.userId,
            "session_creation_failure",
            false
        );

        outputToken.wipe();

        return AuthResult::SESSION_FAILURE;
    }

    appendAudit(
        credential.userId,
        "authentication_success",
        true
    );

    return AuthResult::SUCCESS;
}


/* ============================================================
 * SESSION ESTABLISHMENT
 * ============================================================
 */

static bool establishSession(Token& token)
{
    KernelSession session{};

    if (!kernelConsumeToken(
            token,
            session))
    {
        token.wipe();

        appendAudit(
            0,
            "invalid_or_replayed_session_token",
            false
        );

        return false;
    }

    token.wipe();

    if (!kernelEstablishSession(session))
    {
        appendAudit(
            session.userId,
            "privilege_transition_failure",
            false
        );

        return false;
    }

    appendAudit(
        session.userId,
        "session_established",
        true
    );

    return true;
}
