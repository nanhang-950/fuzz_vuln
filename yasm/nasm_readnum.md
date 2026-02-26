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
INFO: Seed: 4047307543
INFO: Loaded 1 modules   (69 inline 8-bit counters): 69 [0x57afc6cf6f48, 0x57afc6cf6f8d),
INFO: Loaded 1 PC tables (69 PCs): 69 [0x57afc6cf6f90,0x57afc6cf73e0),
./fuzz_nasm_readnum: Running 1 inputs 1 time(s) each.
Running: ./crash-9842926af7ca0a8cca12604f945414f07b01e13d
=================================================================
==10921==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x50200000008f at pc 0x57afc6c9f7a7 bp 0x7ffeeaf9b3d0 sp 0x7ffeeaf9b3c8
READ of size 1 at 0x50200000008f thread T0
    #0 0x57afc6c9f7a6 in nasm_readnum /mnt/h/Security/Binary/Project/yasm/modules/preprocs/nasm/nasmlib.c:56:14
    #1 0x57afc6c9ef8a in LLVMFuzzerTestOneInput /mnt/h/Security/Binary/Project/yasm/fuzzer/fuzz_nasm_readnum.c:56:25
    #2 0x57afc6ba350a in fuzzer::Fuzzer::ExecuteCallback(unsigned char const*, unsigned long) (/mnt/h/Security/Binary/Reports/fuzz_vuln/yasm/fuzz_nasm_readnum+0x4f50a) (BuildId: d2095e9107efcb8bc5dd8bc1958e972df71d6494)
    #3 0x57afc6b8b403 in fuzzer::RunOneTest(fuzzer::Fuzzer*, char const*, unsigned long) (/mnt/h/Security/Binary/Reports/fuzz_vuln/yasm/fuzz_nasm_readnum+0x37403) (BuildId: d2095e9107efcb8bc5dd8bc1958e972df71d6494)
    #4 0x57afc6b91541 in fuzzer::FuzzerDriver(int*, char***, int (*)(unsigned char const*, unsigned long)) (/mnt/h/Security/Binary/Reports/fuzz_vuln/yasm/fuzz_nasm_readnum+0x3d541) (BuildId: d2095e9107efcb8bc5dd8bc1958e972df71d6494)
    #5 0x57afc6bbd9f6 in main (/mnt/h/Security/Binary/Reports/fuzz_vuln/yasm/fuzz_nasm_readnum+0x699f6) (BuildId: d2095e9107efcb8bc5dd8bc1958e972df71d6494)
    #6 0x7c39dc033ca7 in __libc_start_call_main csu/../sysdeps/nptl/libc_start_call_main.h:58:16
    #7 0x7c39dc033d64 in __libc_start_main csu/../csu/libc-start.c:360:3
    #8 0x57afc6b859f0 in _start (/mnt/h/Security/Binary/Reports/fuzz_vuln/yasm/fuzz_nasm_readnum+0x319f0) (BuildId: d2095e9107efcb8bc5dd8bc1958e972df71d6494)

0x50200000008f is located 1 bytes before 2-byte region [0x502000000090,0x502000000092)
allocated by thread T0 here:
    #0 0x57afc6c5d763 in malloc (/mnt/h/Security/Binary/Reports/fuzz_vuln/yasm/fuzz_nasm_readnum+0x109763) (BuildId: d2095e9107efcb8bc5dd8bc1958e972df71d6494)
    #1 0x57afc6c9ef35 in LLVMFuzzerTestOneInput /mnt/h/Security/Binary/Project/yasm/fuzzer/fuzz_nasm_readnum.c:49:25
    #2 0x57afc6ba350a in fuzzer::Fuzzer::ExecuteCallback(unsigned char const*, unsigned long) (/mnt/h/Security/Binary/Reports/fuzz_vuln/yasm/fuzz_nasm_readnum+0x4f50a) (BuildId: d2095e9107efcb8bc5dd8bc1958e972df71d6494)
    #3 0x57afc6b8b403 in fuzzer::RunOneTest(fuzzer::Fuzzer*, char const*, unsigned long) (/mnt/h/Security/Binary/Reports/fuzz_vuln/yasm/fuzz_nasm_readnum+0x37403) (BuildId: d2095e9107efcb8bc5dd8bc1958e972df71d6494)
    #4 0x57afc6b91541 in fuzzer::FuzzerDriver(int*, char***, int (*)(unsigned char const*, unsigned long)) (/mnt/h/Security/Binary/Reports/fuzz_vuln/yasm/fuzz_nasm_readnum+0x3d541) (BuildId: d2095e9107efcb8bc5dd8bc1958e972df71d6494)
    #5 0x57afc6bbd9f6 in main (/mnt/h/Security/Binary/Reports/fuzz_vuln/yasm/fuzz_nasm_readnum+0x699f6) (BuildId: d2095e9107efcb8bc5dd8bc1958e972df71d6494)
    #6 0x7c39dc033ca7 in __libc_start_call_main csu/../sysdeps/nptl/libc_start_call_main.h:58:16

SUMMARY: AddressSanitizer: heap-buffer-overflow /mnt/h/Security/Binary/Project/yasm/modules/preprocs/nasm/nasmlib.c:56:14 in nasm_readnum
Shadow bytes around the buggy address:
  0x501ffffffe00: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x501ffffffe80: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x501fffffff00: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x501fffffff80: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x502000000000: fa fa 00 00 fa fa 00 00 fa fa 01 fa fa fa 01 fa
=>0x502000000080: fa[fa]02 fa fa fa fa fa fa fa fa fa fa fa fa fa
  0x502000000100: fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa
  0x502000000180: fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa
  0x502000000200: fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa
  0x502000000280: fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa
  0x502000000300: fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa
Shadow byte legend (one shadow byte represents 8 application bytes):
  Addressable:           00
  Partially addressable: 01 02 03 04 05 06 07
  Heap left redzone:       fa
  Freed heap region:       fd
  Stack left redzone:      f1
  Stack mid redzone:       f2
  Stack right redzone:     f3
  Stack after return:      f5
  Stack use after scope:   f8
  Global redzone:          f9
  Global init order:       f6
  Poisoned by user:        f7
  Container overflow:      fc
  Array cookie:            ac
  Intra object redzone:    bb
  ASan internal:           fe
  Left alloca redzone:     ca
  Right alloca redzone:    cb
==10921==ABORTING
```

