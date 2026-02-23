# Heap-Buffer-Overflow in modules/preprocs/nasm/nasmlib.c (nasm_readnum)

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
```

## Poc

https://github.com/nanhang-950/fuzz_vuln/blob/main/yasm/crash-9842926af7ca0a8cca12604f945414f07b01e13d

## ASAN Info

```
❯ ./fuzz_nasm_readnum ./crash-9842926af7ca0a8cca12604f945414f07b01e13d
INFO: Running with entropic power schedule (0xFF, 100).
INFO: Seed: 2356677816
INFO: Loaded 1 modules   (4007 inline 8-bit counters): 4007 [0x5fd350, 0x5fe2f7),
INFO: Loaded 1 PC tables (4007 PCs): 4007 [0x5b8ba0,0x5c8610),
./fuzz_nasm_readnum: Running 1 inputs 1 time(s) each.
Running: ./crash-9842926af7ca0a8cca12604f945414f07b01e13d
==8133==AddressSanitizer CHECK failed: compiler-rt/lib/sanitizer_common/sanitizer_linux_libcdep.cpp:556 "((*tls_addr + *tls_size)) <= ((*stk_addr + *stk_size))" (0x7fd9649fa080, 0x7fd9649fa000)
=================================================================
==8133==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x6020000000af at pc 0x000000596a33 bp 0x7ffcb506b490 sp 0x7ffcb506b488
READ of size 1 at 0x6020000000af thread T0
    #0 0x4d6b9e in __asan::AsanCheckFailed(char const*, int, char const*, unsigned long long, unsigned long long) (/mnt/h/Security/Binary/Reports/fuzz_vuln/yasm/fuzz_nasm_readnum+0x4d6b9e)
    #0 0x596a32 in nasm_readnum (/mnt/h/Security/Binary/Reports/fuzz_vuln/yasm/fuzz_nasm_readnum+0x596a32)
    #1 0x4eb38f in __sanitizer::CheckFailed(char const*, int, char const*, unsigned long long, unsigned long long) (/mnt/h/Security/Binary/Reports/fuzz_vuln/yasm/fuzz_nasm_readnum+0x4eb38f)
    #1 0x598728 in LLVMFuzzerTestOneInput (/mnt/h/Security/Binary/Reports/fuzz_vuln/yasm/fuzz_nasm_readnum+0x598728)
    #2 0x4ec220 in __sanitizer::GetThreadStackAndTls(bool, unsigned long*, unsigned long*, unsigned long*, unsigned long*) (/mnt/h/Security/Binary/Reports/fuzz_vuln/yasm/fuzz_nasm_readnum+0x4ec220)
    #2 0x43a7b3 in fuzzer::Fuzzer::ExecuteCallback(unsigned char const*, unsigned long) (/mnt/h/Security/Binary/Reports/fuzz_vuln/yasm/fuzz_nasm_readnum+0x43a7b3)
    #3 0x4d9abd in __asan::AsanThread::SetThreadStackAndTls(__asan::AsanThread::InitOptions const*) (/mnt/h/Security/Binary/Reports/fuzz_vuln/yasm/fuzz_nasm_readnum+0x4d9abd)
    #3 0x42462e in fuzzer::RunOneTest(fuzzer::Fuzzer*, char const*, unsigned long) (/mnt/h/Security/Binary/Reports/fuzz_vuln/yasm/fuzz_nasm_readnum+0x42462e)
    #4 0x4d9690 in __asan::AsanThread::Init(__asan::AsanThread::InitOptions const*) (/mnt/h/Security/Binary/Reports/fuzz_vuln/yasm/fuzz_nasm_readnum+0x4d9690)
    #4 0x42a3d6 in fuzzer::FuzzerDriver(int*, char***, int (*)(unsigned char const*, unsigned long)) (/mnt/h/Security/Binary/Reports/fuzz_vuln/yasm/fuzz_nasm_readnum+0x42a3d6)
    #5 0x4d9bb1 in __asan::AsanThread::ThreadStart(unsigned long long) (/mnt/h/Security/Binary/Reports/fuzz_vuln/yasm/fuzz_nasm_readnum+0x4d9bb1)
    #5 0x453b72 in main (/mnt/h/Security/Binary/Reports/fuzz_vuln/yasm/fuzz_nasm_readnum+0x453b72)
    #6 0x7fd96977fb7a in start_thread nptl/./nptl/pthread_create.c:448:8
    #6 0x7fd969716ca7 in __libc_start_call_main csu/../sysdeps/nptl/libc_start_call_main.h:58:16
    #7 0x7fd9697fd7b7 in __GI___clone3 misc/../sysdeps/unix/sysv/linux/x86_64/clone3.S:78

    #7 0x7fd969716d64 in __libc_start_main csu/../csu/libc-start.c:360:3
```

