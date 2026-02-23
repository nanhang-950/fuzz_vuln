/*
 * libFuzzer harness for YASM's LEB128 integer parser.
 *
 * Target function:
 *   yasm_intnum *yasm_intnum_create_leb128(const unsigned char *ptr,
 *                                          int sign,
 *                                          unsigned long *size);
 *
 * This harness:
 *   - Initializes BitVector, error/warning, and intnum subsystems once
 *     in LLVMFuzzerInitialize.
 *   - For each input, calls yasm_intnum_create_leb128 on the raw bytes,
 *     toggling the signed/unsigned flag based on input size.
 *   - Destroys the returned intnum to avoid leaks.
 *
 * Build example (from project root, with clang and libFuzzer):
 *   clang -fsanitize=fuzzer,address -I. \
 *     libyasm/*.c \
 *     fuzz_intnum_leb128.c \
 *     -o fuzz_intnum_leb128
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>

#include "libyasm/coretype.h"
#include "libyasm/bitvect.h"
#include "libyasm/errwarn.h"
#include "libyasm/intnum.h"

/* Optional: libFuzzer one-time initialization hook. */
int LLVMFuzzerInitialize(int *argc, char ***argv)
{
    (void)argc;
    (void)argv;

    /* Initialize subsystems used by intnum/bitvector code. */
    BitVector_Boot();
    yasm_errwarn_initialize();
    yasm_intnum_initialize();

    return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size)
{
    if (Data == NULL || Size == 0)
        return 0;

    /* libyasm API expects unsigned char*. */
    const unsigned char *ptr = (const unsigned char *)Data;

    /* Flip between signed and unsigned LEB128 based on Size. */
    int sign = (Size & 1) ? 1 : 0;

    unsigned long consumed = 0;
    yasm_intnum *intn = yasm_intnum_create_leb128(ptr, sign, &consumed);

    /* Clean up to avoid leaks during long fuzzing runs. */
    if (intn != NULL)
        yasm_intnum_destroy(intn);

    return 0;
}

