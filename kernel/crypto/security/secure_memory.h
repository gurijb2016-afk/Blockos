#pragma once
#include <stddef.h>
namespace BlockOS { namespace Crypto {
void SecureZero(void* memory, size_t length);
bool ConstantTimeEqual(const void* left, const void* right, size_t length);
} }
