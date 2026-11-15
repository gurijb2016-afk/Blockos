#pragma once
#include "../core/status.h"
#include <stddef.h>
#include <stdint.h>
namespace BlockOS { namespace Crypto {
constexpr size_t kChaCha20Poly1305TagSize = 16;
CryptoResult ChaCha20Poly1305Encrypt(const uint8_t key[32], const uint8_t nonce[12],
    const uint8_t* aad, size_t aad_length, const uint8_t* plaintext, size_t plaintext_length,
    uint8_t* ciphertext, uint8_t tag[16]);
CryptoResult ChaCha20Poly1305Decrypt(const uint8_t key[32], const uint8_t nonce[12],
    const uint8_t* aad, size_t aad_length, const uint8_t* ciphertext, size_t ciphertext_length,
    const uint8_t tag[16], uint8_t* plaintext);
} }
