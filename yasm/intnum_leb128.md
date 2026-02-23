# Heap-Buffer-Overflow in LEB128 Integer Parsing Function yasm_intnum_create_leb128

## project address

https://github.com/yasm/yasm

## info

OS：Ubuntu22.04 TLS

Build: 

```shell
./autogen.sh
make distclean 2>/dev/null || true
CC=gcc CXX=g++ \
CFLAGS="-fsanitize=address -fno-omit-frame-pointer -g" \
CXXFLAGS="-fsanitize=address -fno-omit-frame-pointer -g" \
LDFLAGS="-fsanitize=address" \
./configure --prefix=$PWD/build --disable-shared
make -j$(nproc)
```

## fuzzer

```c
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

    /* We intentionally ignore yasm_error_* and warnings here; libFuzzer
     * will catch memory safety issues via sanitizers.
     */
    return 0;
}
```

## Poc

https://github.com/z1r00/fuzz_vuln/blob/main/yasm/stack-overflow/parse_expr1/id:000206%2Csig:06%2Csrc:007018%2B003531%2Cop:splice%2Crep:32

## ASAN Info

```
./fuzz_intnum_leb128 ./crash-1599e9fa41ec68c80230491902786bee889f5bcb   
INFO: Running with entropic power schedule (0xFF, 100).
INFO: Seed: 1221961173
INFO: Loaded 1 modules   (3942 inline 8-bit counters): 3942 [0x5fa280, 0x5fb1e6),
INFO: Loaded 1 PC tables (3942 PCs): 3942 [0x5b6ac8,0x5c6128),
./fuzz_intnum_leb128: Running 1 inputs 1 time(s) each.
Running: ./crash-c4488af0c158e8c2832cb927cfb3ce534104cd1e
==24000==AddressSanitizer CHECK failed: compiler-rt/lib/sanitizer_common/sanitizer_linux_libcdep.cpp:556 "((*tls_addr + *tls_size)) <= ((*stk_addr + *stk_size))" (0x7fab021fa080, 0x7fab021fa000)
=================================================================
==24000==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x602000000091 at pc 0x000000558237 bp 0x7fff54e81fa0 sp 0x7fff54e81f98
READ of size 1 at 0x602000000091 thread T0
    #0 0x4d6b9e in __asan::AsanCheckFailed(char const*, int, char const*, unsigned long long, unsigned long long) (/mnt/h/Security/Binary/Reports/fuzz_vuln/yasm/fuzz/fuzz_intnum_leb128+0x4d6b9e)
    #0 0x558236 in yasm_intnum_create_leb128 (/mnt/h/Security/Binary/Reports/fuzz_vuln/yasm/fuzz/fuzz_intnum_leb128+0x558236)
    #1 0x4eb38f in __sanitizer::CheckFailed(char const*, int, char const*, unsigned long long, unsigned long long) (/mnt/h/Security/Binary/Reports/fuzz_vuln/yasm/fuzz/fuzz_intnum_leb128+0x4eb38f)
    #1 0x59654d in LLVMFuzzerTestOneInput (/mnt/h/Security/Binary/Reports/fuzz_vuln/yasm/fuzz/fuzz_intnum_leb128+0x59654d)
    #2 0x4ec220 in __sanitizer::GetThreadStackAndTls(bool, unsigned long*, unsigned long*, unsigned long*, unsigned long*) (/mnt/h/Security/Binary/Reports/fuzz_vuln/yasm/fuzz/fuzz_intnum_leb128+0x4ec220)
    #2 0x43a7b3 in fuzzer::Fuzzer::ExecuteCallback(unsigned char const*, unsigned long) (/mnt/h/Security/Binary/Reports/fuzz_vuln/yasm/fuzz/fuzz_intnum_leb128+0x43a7b3)
    #3 0x4d9abd in __asan::AsanThread::SetThreadStackAndTls(__asan::AsanThread::InitOptions const*) (/mnt/h/Security/Binary/Reports/fuzz_vuln/yasm/fuzz/fuzz_intnum_leb128+0x4d9abd)
    #4 0x4d9690 in __asan::AsanThread::Init(__asan::AsanThread::InitOptions const*) (/mnt/h/Security/Binary/Reports/fuzz_vuln/yasm/fuzz/fuzz_intnum_leb128+0x4d9690)
    #3 0x42462e in fuzzer::RunOneTest(fuzzer::Fuzzer*, char const*, unsigned long) (/mnt/h/Security/Binary/Reports/fuzz_vuln/yasm/fuzz/fuzz_intnum_leb128+0x42462e)
    #5 0x4d9bb1 in __asan::AsanThread::ThreadStart(unsigned long long) (/mnt/h/Security/Binary/Reports/fuzz_vuln/yasm/fuzz/fuzz_intnum_leb128+0x4d9bb1)
    #4 0x42a3d6 in fuzzer::FuzzerDriver(int*, char***, int (*)(unsigned char const*, unsigned long)) (/mnt/h/Security/Binary/Reports/fuzz_vuln/yasm/fuzz/fuzz_intnum_leb128+0x42a3d6)
    #6 0x7fab06facb7a in start_thread nptl/./nptl/pthread_create.c:448:8
    #5 0x453b72 in main (/mnt/h/Security/Binary/Reports/fuzz_vuln/yasm/fuzz/fuzz_intnum_leb128+0x453b72)
    #7 0x7fab0702a7b7 in __GI___clone3 misc/../sysdeps/unix/sysv/linux/x86_64/clone3.S:78
```
