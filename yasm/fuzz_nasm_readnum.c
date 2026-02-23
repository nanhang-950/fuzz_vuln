/*
 * libFuzzer harness for NASM-style integer parsing in YASM.
 *
 * Target function (from modules/preprocs/nasm/nasmlib.c):
 *   yasm_intnum *nasm_readnum(char *str, int *error);
 *
 * This fuzzer:
 *   - Initializes BitVector, error/warning, and intnum subsystems once.
 *   - For each input, copies the bytes to a writable, NUL-terminated buffer
 *     and calls nasm_readnum.
 *   - Destroys the returned intnum to avoid leaks.
 *
 * Example build (from project root, with clang + libFuzzer):
 *   clang -fsanitize=fuzzer,address -I. \
 *     libyasm/*.c modules/preprocs/nasm/nasmlib.c \
 *     fuzz_nasm_readnum.c -o fuzz_nasm_readnum
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include "libyasm/coretype.h"
#include "libyasm/bitvect.h"
#include "libyasm/errwarn.h"
#include "libyasm/intnum.h"
#include "modules/preprocs/nasm/nasmlib.h"

int LLVMFuzzerInitialize(int *argc, char ***argv)
{
    (void)argc;
    (void)argv;

    BitVector_Boot();
    yasm_errwarn_initialize();
    yasm_intnum_initialize();
    return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size)
{
    if (!Data || Size == 0)
        return 0;

    /* nasm_readnum expects a writable, NUL-terminated char * string. */
    char *buf = (char *)malloc(Size + 1);
    if (!buf)
        return 0;
    memcpy(buf, Data, Size);
    buf[Size] = '\0';

    int err = 0;
    yasm_intnum *intn = nasm_readnum(buf, &err);
    if (intn)
        yasm_intnum_destroy(intn);

    free(buf);
    return 0;
}

