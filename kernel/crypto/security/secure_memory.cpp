#include "secure_memory.h"
#include <stdint.h>
namespace BlockOS { namespace Crypto {
void SecureZero(void* memory, size_t length) {
    if (!memory) return;
    volatile uint8_t* p=static_cast<volatile uint8_t*>(memory);
    while (length--) *p++=0;
}
bool ConstantTimeEqual(const void* left, const void* right, size_t length) {
    if ((!left || !right) && length!=0) return false;
    const volatile uint8_t* a=static_cast<const volatile uint8_t*>(left);
    const volatile uint8_t* b=static_cast<const volatile uint8_t*>(right);
    uint8_t difference=0;
    for (size_t i=0;i<length;++i) difference=static_cast<uint8_t>(difference | (a[i]^b[i]));
    return difference==0;
}
} }
