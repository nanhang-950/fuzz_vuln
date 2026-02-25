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
❯ ./sharpyuv_fuzzer
[==========] Running 1 test from 1 test suite.
[----------] Global test environment set-up.
[----------] 1 test from SharpYuv
[ RUN      ] SharpYuv.SharpYuvTest
FUZZTEST_PRNG_SEED=G1a4V1PK17QPisZXy6PXdr597UcXb-7Q5YMTsQESJGE
[       OK ] SharpYuv.SharpYuvTest (1023 ms)
[----------] 1 test from SharpYuv (1023 ms total)

[----------] Global test environment tear-down
[==========] 1 test from 1 test suite ran. (1023 ms total)
[  PASSED  ] 1 test.
❯ ./sharpyuv_fuzzer
[==========] Running 1 test from 1 test suite.
[----------] Global test environment set-up.
[----------] 1 test from SharpYuv
[ RUN      ] SharpYuv.SharpYuvTest
FUZZTEST_PRNG_SEED=_iqia0-5V7ukKgksHoxHNoGGk4Noec1skRTQA9tg9bM

=================================================================
=== BUG FOUND!

/mnt/h/Security/Binary/Project/libwebp/tests/fuzzer/sharpyuv_fuzzer.cc:107: Counterexample found for SharpYuv.SharpYuvTest.
The test fails with input:
argument 0: "\207"
argument 1: 35
argument 2: 34
argument 3: 10
argument 4: 8
argument 5: 3
argument 6: 131577446
argument 7: false
argument 8: true

=================================================================
=== Reproducer test

TEST(SharpYuv, SharpYuvTestRegression) {
  SharpYuvTest(
    "\207",
    35,
    34,
    10,
    8,
    3,
    131577446,
    false,
    true
  );
}

=================================================================
UndefinedBehaviorSanitizer:DEADLYSIGNAL
==31161==ERROR: UndefinedBehaviorSanitizer: SEGV on unknown address 0x5b212ecc1c70 (pc 0x5b212ddbf6a4 bp 0x000000000000 sp 0x7fff6bb7f7e0 T31161)
==31161==The signal is caused by a READ memory access.
    #0 0x5b212ddbf6a4 in SharpYuvLinearToGamma (/mnt/h/Security/Binary/Project/libwebp/build-fuzz-clang/tests/fuzzer/sharpyuv_fuzzer+0x1566a4) (BuildId: 656c2b97777d5e444554dcc12c803092a064889b)
    #1 0x5b212ddbc198 in SharpYuvConvertWithOptions (/mnt/h/Security/Binary/Project/libwebp/build-fuzz-clang/tests/fuzzer/sharpyuv_fuzzer+0x153198) (BuildId: 656c2b97777d5e444554dcc12c803092a064889b)
    #2 0x5b212ddbb381 in SharpYuvConvert (/mnt/h/Security/Binary/Project/libwebp/build-fuzz-clang/tests/fuzzer/sharpyuv_fuzzer+0x152381) (BuildId: 656c2b97777d5e444554dcc12c803092a064889b)
    #3 0x5b212dd90369 in (anonymous namespace)::SharpYuvTest(std::basic_string_view<char, std::char_traits<char>>, int, int, int, int, int, int, bool, bool) /mnt/h/Security/Binary/Project/libwebp/tests/fuzzer/sharpyuv_fuzzer.cc:97:11
    #4 0x5b212ddba336 in auto fuzztest::internal::FixtureDriver<fuzztest::Domain<std::tuple<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>, int, int, int, int, int, int, bool, bool>>, fuzztest::internal::NoFixture, void (*)(std::basic_string_view<char, std::char_traits<char>>, int, int, int, int, int, int, bool, bool), void*>::Test(fuzztest::internal::MoveOnlyAny&&) const::'lambda'(auto&&...)::operator()<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>&, int&, int&, int&, int&, int&, int&, bool&, bool&>(auto&&...) const /mnt/h/Security/Binary/Project/libwebp/build-fuzz-clang/_deps/fuzztest-src/./fuzztest/internal/fixture_driver.h:294:11
    #5 0x5b212ddba336 in void std::__invoke_impl<void, fuzztest::internal::FixtureDriver<fuzztest::Domain<std::tuple<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>, int, int, int, int, int, int, bool, bool>>, fuzztest::internal::NoFixture, void (*)(std::basic_string_view<char, std::char_traits<char>>, int, int, int, int, int, int, bool, bool), void*>::Test(fuzztest::internal::MoveOnlyAny&&) const::'lambda'(auto&&...), std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>&, int&, int&, int&, int&, int&, int&, bool&, bool&>(std::__invoke_other, fuzztest::internal::FixtureDriver<fuzztest::Domain<std::tuple<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>, int, int, int, int, int, int, bool, bool>>, fuzztest::internal::NoFixture, void (*)(std::basic_string_view<char, std::char_traits<char>>, int, int, int, int, int, int, bool, bool), void*>::Test(fuzztest::internal::MoveOnlyAny&&) const::'lambda'(auto&&...)&&, std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>&, int&, int&, int&, int&, int&, int&, bool&, bool&) /usr/lib/gcc/x86_64-linux-gnu/14/../../../../include/c++/14/bits/invoke.h:61:14
    #6 0x5b212ddb9cc4 in std::__invoke_result<fuzztest::internal::FixtureDriver<fuzztest::Domain<std::tuple<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>, int, int, int, int, int, int, bool, bool>>, fuzztest::internal::NoFixture, void (*)(std::basic_string_view<char, std::char_traits<char>>, int, int, int, int, int, int, bool, bool), void*>::Test(fuzztest::internal::MoveOnlyAny&&) const::'lambda'(auto&&...), std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>&, int&, int&, int&, int&, int&, int&, bool&, bool&>::type std::__invoke<fuzztest::internal::FixtureDriver<fuzztest::Domain<std::tuple<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>, int, int, int, int, int, int, bool, bool>>, fuzztest::internal::NoFixture, void (*)(std::basic_string_view<char, std::char_traits<char>>, int, int, int, int, int, int, bool, bool), void*>::Test(fuzztest::internal::MoveOnlyAny&&) const::'lambda'(auto&&...), std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>&, int&, int&, int&, int&, int&, int&, bool&, bool&>(fuzztest::internal::FixtureDriver<fuzztest::Domain<std::tuple<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>, int, int, int, int, int, int, bool, bool>>, fuzztest::internal::NoFixture, void (*)(std::basic_string_view<char, std::char_traits<char>>, int, int, int, int, int, int, bool, bool), void*>::Test(fuzztest::internal::MoveOnlyAny&&) const::'lambda'(auto&&...)&&, std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>&, int&, int&, int&, int&, int&, int&, bool&, bool&) /usr/lib/gcc/x86_64-linux-gnu/14/../../../../include/c++/14/bits/invoke.h:96:14
    #7 0x5b212ddb9cc4 in decltype(auto) std::__apply_impl<fuzztest::internal::FixtureDriver<fuzztest::Domain<std::tuple<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>, int, int, int, int, int, int, bool, bool>>, fuzztest::internal::NoFixture, void (*)(std::basic_string_view<char, std::char_traits<char>>, int, int, int, int, int, int, bool, bool), void*>::Test(fuzztest::internal::MoveOnlyAny&&) const::'lambda'(auto&&...), std::tuple<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>, int, int, int, int, int, int, bool, bool>&, 0ul, 1ul, 2ul, 3ul, 4ul, 5ul, 6ul, 7ul, 8ul>(fuzztest::internal::FixtureDriver<fuzztest::Domain<std::tuple<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>, int, int, int, int, int, int, bool, bool>>, fuzztest::internal::NoFixture, void (*)(std::basic_string_view<char, std::char_traits<char>>, int, int, int, int, int, int, bool, bool), void*>::Test(fuzztest::internal::MoveOnlyAny&&) const::'lambda'(auto&&...)&&, std::tuple<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>, int, int, int, int, int, int, bool, bool>&, std::integer_sequence<unsigned long, 0ul, 1ul, 2ul, 3ul, 4ul, 5ul, 6ul, 7ul, 8ul>) /usr/lib/gcc/x86_64-linux-gnu/14/../../../../include/c++/14/tuple:2923:14
    #8 0x5b212ddb9cc4 in decltype(auto) std::apply<fuzztest::internal::FixtureDriver<fuzztest::Domain<std::tuple<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>, int, int, int, int, int, int, bool, bool>>, fuzztest::internal::NoFixture, void (*)(std::basic_string_view<char, std::char_traits<char>>, int, int, int, int, int, int, bool, bool), void*>::Test(fuzztest::internal::MoveOnlyAny&&) const::'lambda'(auto&&...), std::tuple<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>, int, int, int, int, int, int, bool, bool>&>(fuzztest::internal::FixtureDriver<fuzztest::Domain<std::tuple<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>, int, int, int, int, int, int, bool, bool>>, fuzztest::internal::NoFixture, void (*)(std::basic_string_view<char, std::char_traits<char>>, int, int, int, int, int, int, bool, bool), void*>::Test(fuzztest::internal::MoveOnlyAny&&) const::'lambda'(auto&&...)&&, std::tuple<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>, int, int, int, int, int, int, bool, bool>&) /usr/lib/gcc/x86_64-linux-gnu/14/../../../../include/c++/14/tuple:2938:14
    #9 0x5b212ddb9cc4 in fuzztest::internal::FixtureDriver<fuzztest::Domain<std::tuple<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>, int, int, int, int, int, int, bool, bool>>, fuzztest::internal::NoFixture, void (*)(std::basic_string_view<char, std::char_traits<char>>, int, int, int, int, int, int, bool, bool), void*>::Test(fuzztest::internal::MoveOnlyAny&&) const /mnt/h/Security/Binary/Project/libwebp/build-fuzz-clang/_deps/fuzztest-src/./fuzztest/internal/fixture_driver.h:292:5
    #10 0x5b212de2b406 in fuzztest::internal::FuzzTestFuzzerImpl::RunOneInput(fuzztest::internal::FuzzTestFuzzerImpl::Input const&) /mnt/h/Security/Binary/Project/libwebp/build-fuzz-clang/_deps/fuzztest-src/fuzztest/internal/runtime.cc:927:20
    #11 0x5b212de2ec8d in fuzztest::internal::FuzzTestFuzzerImpl::RunInUnitTestMode(fuzztest::internal::Configuration const&)::$_0::operator()() const /mnt/h/Security/Binary/Project/libwebp/build-fuzz-clang/_deps/fuzztest-src/fuzztest/internal/runtime.cc:879:7
    #12 0x5b212de2ec8d in fuzztest::internal::FuzzTestFuzzerImpl::RunInUnitTestMode(fuzztest::internal::Configuration const&) /mnt/h/Security/Binary/Project/libwebp/build-fuzz-clang/_deps/fuzztest-src/fuzztest/internal/runtime.cc:833:3
    #13 0x5b212ddebfde in fuzztest::internal::GTest_TestAdaptor::TestBody() /mnt/h/Security/Binary/Project/libwebp/build-fuzz-clang/_deps/fuzztest-src/./fuzztest/internal/googletest_adaptor.h:78:15
    #14 0x5b212df13eb4 in void testing::internal::HandleSehExceptionsInMethodIfSupported<testing::Test, void>(testing::Test*, void (testing::Test::*)(), char const*) /mnt/h/Security/Binary/Project/libwebp/build-fuzz-clang/_deps/googletest-src/googletest/src/gtest.cc:2612:10
    #15 0x5b212df13eb4 in void testing::internal::HandleExceptionsInMethodIfSupported<testing::Test, void>(testing::Test*, void (testing::Test::*)(), char const*) /mnt/h/Security/Binary/Project/libwebp/build-fuzz-clang/_deps/googletest-src/googletest/src/gtest.cc:2648:14
    #16 0x5b212df13cdd in testing::Test::Run() /mnt/h/Security/Binary/Project/libwebp/build-fuzz-clang/_deps/googletest-src/googletest/src/gtest.cc:2687:5
    #17 0x5b212df15f23 in testing::TestInfo::Run() /mnt/h/Security/Binary/Project/libwebp/build-fuzz-clang/_deps/googletest-src/googletest/src/gtest.cc:2836:11
    #18 0x5b212df17b45 in testing::TestSuite::Run() /mnt/h/Security/Binary/Project/libwebp/build-fuzz-clang/_deps/googletest-src/googletest/src/gtest.cc:3015:30
    #19 0x5b212df377f3 in testing::internal::UnitTestImpl::RunAllTests() /mnt/h/Security/Binary/Project/libwebp/build-fuzz-clang/_deps/googletest-src/googletest/src/gtest.cc:5920:44
    #20 0x5b212df36ee4 in bool testing::internal::HandleSehExceptionsInMethodIfSupported<testing::internal::UnitTestImpl, bool>(testing::internal::UnitTestImpl*, bool (testing::internal::UnitTestImpl::*)(), char const*) /mnt/h/Security/Binary/Project/libwebp/build-fuzz-clang/_deps/googletest-src/googletest/src/gtest.cc:2612:10
    #21 0x5b212df36ee4 in bool testing::internal::HandleExceptionsInMethodIfSupported<testing::internal::UnitTestImpl, bool>(testing::internal::UnitTestImpl*, bool (testing::internal::UnitTestImpl::*)(), char const*) /mnt/h/Security/Binary/Project/libwebp/build-fuzz-clang/_deps/googletest-src/googletest/src/gtest.cc:2648:14
    #22 0x5b212df36c6f in testing::UnitTest::Run() /mnt/h/Security/Binary/Project/libwebp/build-fuzz-clang/_deps/googletest-src/googletest/src/gtest.cc:5484:10
    #23 0x5b212ddc0649 in RUN_ALL_TESTS() /mnt/h/Security/Binary/Project/libwebp/build-fuzz-clang/_deps/googletest-src/googletest/include/gtest/gtest.h:2317:73
    #24 0x5b212ddc0649 in main /mnt/h/Security/Binary/Project/libwebp/build-fuzz-clang/_deps/fuzztest-src/fuzztest/fuzztest_gtest_main.cc:28:10
    #25 0x78c988033ca7 in __libc_start_call_main csu/../sysdeps/nptl/libc_start_call_main.h:58:16
    #26 0x78c988033d64 in __libc_start_main csu/../csu/libc-start.c:360:3
    #27 0x5b212dd620e0 in _start (/mnt/h/Security/Binary/Project/libwebp/build-fuzz-clang/tests/fuzzer/sharpyuv_fuzzer+0xf90e0) (BuildId: 656c2b97777d5e444554dcc12c803092a064889b)

==31161==Register values:
rax = 0x00000000000a74d4  rbx = 0x000000000000000c  rcx = 0x0000053a6a177f9d  rdx = 0x000000000000000d
rdi = 0x00000000053a6a17  rsi = 0x000000000000000c  rbp = 0x0000000000000000  rsp = 0x00007fff6bb7f7e0
 r8 = 0x0000000000000000   r9 = 0x00005b212ea24920  r10 = 0x0000000000000090  r11 = 0x00005b2152c54aa8
r12 = 0x000000000000000d  r13 = 0x000000000000000c  r14 = 0x0000000000000000  r15 = 0x0000000000000048
UndefinedBehaviorSanitizer can not provide additional info.
SUMMARY: UndefinedBehaviorSanitizer: SEGV (/mnt/h/Security/Binary/Project/libwebp/build-fuzz-clang/tests/fuzzer/sharpyuv_fuzzer+0x1566a4) (BuildId: 656c2b97777d5e444554dcc12c803092a064889b) in SharpYuvLinearToGamma
==31161==ABORTING
```

