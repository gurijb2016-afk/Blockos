#include <ffi.h>
#include <stdio.h>

static int add_ints(int a, int b) { return a + b; }

int main(void) {
    ffi_cif cif;
    ffi_type *args[2] = { &ffi_type_sint, &ffi_type_sint };
    void *values[2];
    int a = 7, b = 35, result = 0;
    values[0] = &a; values[1] = &b;
    if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 2, &ffi_type_sint, args) != FFI_OK)
        return 2;
    ffi_call(&cif, FFI_FN(add_ints), &result, values);
    printf("ffi=%d\n", result);
    return result == 42 ? 0 : 3;
}
