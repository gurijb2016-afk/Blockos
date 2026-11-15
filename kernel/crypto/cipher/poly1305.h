#pragma once
#include "../core/status.h"
#include <stddef.h>
#include <stdint.h>
namespace BlockOS { namespace Crypto {
struct Poly1305Context {
    uint32_t r[5]; uint32_t h[5]; uint32_t pad[4];
    uint8_t buffer[16]; size_t leftover; bool finalized;
};
void Poly1305Init(Poly1305Context* context, const uint8_t key[32]);
CryptoResult Poly1305Update(Poly1305Context* context, const uint8_t* message, size_t length);
CryptoResult Poly1305Final(Poly1305Context* context, uint8_t tag[16]);
CryptoResult Poly1305(const uint8_t key[32], const uint8_t* message, size_t length, uint8_t tag[16]);
} }
