## project address

project：https://github.com/webmproject/libwebp

version：v1.16.0

## info

OS：Ubuntu22.04 TLS

Build: 

```shell
git clone https://chromium.googlesource.com/webm/libwebp
cd libwebp

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

## fuzzer

```cc
// Copyright 2026 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
////////////////////////////////////////////////////////////////////////////////

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include "./fuzz_utils.h"
#include "sharpyuv/sharpyuv.h"
#include "sharpyuv/sharpyuv_csp.h"

namespace {

size_t SafeSize(size_t size) {
  static constexpr size_t kMaxBytes = 4u * 1024u * 1024u;
  return std::min(size, kMaxBytes);
}

void FillFromBlob(std::string_view blob, std::vector<uint8_t>* dst) {
  if (dst == nullptr || dst->empty()) return;
  if (blob.empty()) {
    std::fill(dst->begin(), dst->end(), 0);
    return;
  }
  for (size_t i = 0; i < dst->size(); ++i) {
    (*dst)[i] = static_cast<uint8_t>(blob[i % blob.size()]);
  }
}

void SharpYuvTest(std::string_view blob, int width, int height,
                  int rgb_bit_depth, int yuv_bit_depth, int matrix_type,
                  int transfer_type, bool use_options,
                  bool use_small_output_buffers) {
  if (width <= 0 || height <= 0) return;

  const int rgb_bytes_per_sample = (rgb_bit_depth <= 8) ? 1 : 2;
  const int yuv_bytes_per_sample = (yuv_bit_depth <= 8) ? 1 : 2;

  const int rgb_step = rgb_bytes_per_sample;
  const int rgb_stride = width * rgb_step;
  const int y_stride = width * yuv_bytes_per_sample;
  const int uv_width = (width + 1) / 2;
  const int uv_height = (height + 1) / 2;
  const int uv_stride = uv_width * yuv_bytes_per_sample;

  size_t rgb_plane_size = SafeSize(static_cast<size_t>(rgb_stride) * height);
  size_t y_plane_size = SafeSize(static_cast<size_t>(y_stride) * height);
  size_t uv_plane_size = SafeSize(static_cast<size_t>(uv_stride) * uv_height);
  if (use_small_output_buffers) {
    if (y_plane_size > 0) --y_plane_size;
    if (uv_plane_size > 0) --uv_plane_size;
  }

  std::vector<uint8_t> r(rgb_plane_size);
  std::vector<uint8_t> g(rgb_plane_size);
  std::vector<uint8_t> b(rgb_plane_size);
  std::vector<uint8_t> y(y_plane_size);
  std::vector<uint8_t> u(uv_plane_size);
  std::vector<uint8_t> v(uv_plane_size);
  FillFromBlob(blob, &r);
  FillFromBlob(blob.substr(std::min<size_t>(1, blob.size())), &g);
  FillFromBlob(blob.substr(std::min<size_t>(2, blob.size())), &b);

  const SharpYuvMatrixType safe_matrix_type =
      static_cast<SharpYuvMatrixType>(
          static_cast<uint32_t>(matrix_type) % kSharpYuvMatrixNum);
  const SharpYuvConversionMatrix* const matrix =
      SharpYuvGetConversionMatrix(safe_matrix_type);
  if (matrix == nullptr) return;

  if (use_options) {
    SharpYuvOptions options;
    if (!SharpYuvOptionsInit(matrix, &options)) return;
    options.transfer_type =
        static_cast<SharpYuvTransferFunctionType>(transfer_type);
    (void)SharpYuvConvertWithOptions(
        r.data(), g.data(), b.data(), rgb_step, rgb_stride, rgb_bit_depth,
        y.empty() ? nullptr : y.data(), y_stride, u.empty() ? nullptr : u.data(),
        uv_stride, v.empty() ? nullptr : v.data(), uv_stride, yuv_bit_depth,
        width, height, &options);
  } else {
    (void)SharpYuvConvert(
        r.data(), g.data(), b.data(), rgb_step, rgb_stride, rgb_bit_depth,
        y.empty() ? nullptr : y.data(), y_stride, u.empty() ? nullptr : u.data(),
        uv_stride, v.empty() ? nullptr : v.data(), uv_stride, yuv_bit_depth,
        width, height, matrix);
  }
}

}  // namespace

FUZZ_TEST(SharpYuv, SharpYuvTest)
    .WithDomains(
        fuzztest::String().WithMaxSize(fuzz_utils::kMaxWebPFileSize + 1),
        /*width=*/fuzztest::InRange<int>(1, 257),
        /*height=*/fuzztest::InRange<int>(1, 257),
        /*rgb_bit_depth=*/fuzztest::ElementOf<int>({8, 10, 12, 16}),
        /*yuv_bit_depth=*/fuzztest::ElementOf<int>({8, 10, 12}),
        /*matrix_type=*/fuzztest::InRange<int>(0,
                                               static_cast<int>(kSharpYuvMatrixNum)),
        /*transfer_type=*/fuzztest::Arbitrary<int>(),
        /*use_options=*/fuzztest::Arbitrary<bool>(),
        /*use_small_output_buffers=*/fuzztest::Arbitrary<bool>());
```

## ASAN Info

```
❯ ./sharpyuv_fuzzer
[==========] Running 1 test from 1 test suite.
[----------] Global test environment set-up.
[----------] 1 test from SharpYuv
[ RUN      ] SharpYuv.SharpYuvTest
FUZZTEST_PRNG_SEED=J3lMCazS5To-jbTZCDzMxjAXmYVjHMbu09ChOvOG6WM

=================================================================
=== BUG FOUND!

/mnt/h/Security/Binary/Project/libwebp/tests/fuzzer/sharpyuv_fuzzer.cc:107: Counterexample found for SharpYuv.SharpYuvTest.
The test fails with input:
argument 0: "_"
argument 1: 46
argument 2: 6
argument 3: 12
argument 4: 12
argument 5: 3
argument 6: -1972618543
argument 7: false
argument 8: true

=================================================================
=== Reproducer test

TEST(SharpYuv, SharpYuvTestRegression) {
  SharpYuvTest(
    "_",
    46,
    6,
    12,
    12,
    3,
    -1972618543,
    false,
    true
  );
}

=================================================================
UndefinedBehaviorSanitizer:DEADLYSIGNAL
==20281==ERROR: UndefinedBehaviorSanitizer: SEGV on unknown address 0x57d84b563e4c (pc 0x57d84a53b6a4 bp 0x000000000000 sp 0x7ffe41f4dda0 T20281)
==20281==The signal is caused by a READ memory access.
    #0 0x57d84a53b6a4 in SharpYuvLinearToGamma (/mnt/h/Security/Binary/Project/libwebp/build-fuzz-clang/tests/fuzzer/sharpyuv_fuzzer+0x1566a4) (BuildId: 656c2b97777d5e444554dcc12c803092a064889b)
    #1 0x57d84a538198 in SharpYuvConvertWithOptions (/mnt/h/Security/Binary/Project/libwebp/build-fuzz-clang/tests/fuzzer/sharpyuv_fuzzer+0x153198) (BuildId: 656c2b97777d5e444554dcc12c803092a064889b)
    #2 0x57d84a537381 in SharpYuvConvert (/mnt/h/Security/Binary/Project/libwebp/build-fuzz-clang/tests/fuzzer/sharpyuv_fuzzer+0x152381) (BuildId: 656c2b97777d5e444554dcc12c803092a064889b)
    #3 0x57d84a50c369 in (anonymous namespace)::SharpYuvTest(std::basic_string_view<char, std::char_traits<char>>, int, int, int, int, int, int, bool, bool) /mnt/h/Security/Binary/Project/libwebp/tests/fuzzer/sharpyuv_fuzzer.cc:97:11
    #4 0x57d84a536336 in auto fuzztest::internal::FixtureDriver<fuzztest::Domain<std::tuple<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>, int, int, int, int, int, int, bool, bool>>, fuzztest::internal::NoFixture, void (*)(std::basic_string_view<char, std::char_traits<char>>, int, int, int, int, int, int, bool, bool), void*>::Test(fuzztest::internal::MoveOnlyAny&&) const::'lambda'(auto&&...)::operator()<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>&, int&, int&, int&, int&, int&, int&, bool&, bool&>(auto&&...) const /mnt/h/Security/Binary/Project/libwebp/build-fuzz-clang/_deps/fuzztest-src/./fuzztest/internal/fixture_driver.h:294:11
    #5 0x57d84a536336 in void std::__invoke_impl<void, fuzztest::internal::FixtureDriver<fuzztest::Domain<std::tuple<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>, int, int, int, int, int, int, bool, bool>>, fuzztest::internal::NoFixture, void (*)(std::basic_string_view<char, std::char_traits<char>>, int, int, int, int, int, int, bool, bool), void*>::Test(fuzztest::internal::MoveOnlyAny&&) const::'lambda'(auto&&...), std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>&, int&, int&, int&, int&, int&, int&, bool&, bool&>(std::__invoke_other, fuzztest::internal::FixtureDriver<fuzztest::Domain<std::tuple<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>, int, int, int, int, int, int, bool, bool>>, fuzztest::internal::NoFixture, void (*)(std::basic_string_view<char, std::char_traits<char>>, int, int, int, int, int, int, bool, bool), void*>::Test(fuzztest::internal::MoveOnlyAny&&) const::'lambda'(auto&&...)&&, std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>&, int&, int&, int&, int&, int&, int&, bool&, bool&) /usr/lib/gcc/x86_64-linux-gnu/14/../../../../include/c++/14/bits/invoke.h:61:14
    #6 0x57d84a535cc4 in std::__invoke_result<fuzztest::internal::FixtureDriver<fuzztest::Domain<std::tuple<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>, int, int, int, int, int, int, bool, bool>>, fuzztest::internal::NoFixture, void (*)(std::basic_string_view<char, std::char_traits<char>>, int, int, int, int, int, int, bool, bool), void*>::Test(fuzztest::internal::MoveOnlyAny&&) const::'lambda'(auto&&...), std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>&, int&, int&, int&, int&, int&, int&, bool&, bool&>::type std::__invoke<fuzztest::internal::FixtureDriver<fuzztest::Domain<std::tuple<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>, int, int, int, int, int, int, bool, bool>>, fuzztest::internal::NoFixture, void (*)(std::basic_string_view<char, std::char_traits<char>>, int, int, int, int, int, int, bool, bool), void*>::Test(fuzztest::internal::MoveOnlyAny&&) const::'lambda'(auto&&...), std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>&, int&, int&, int&, int&, int&, int&, bool&, bool&>(fuzztest::internal::FixtureDriver<fuzztest::Domain<std::tuple<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>, int, int, int, int, int, int, bool, bool>>, fuzztest::internal::NoFixture, void (*)(std::basic_string_view<char, std::char_traits<char>>, int, int, int, int, int, int, bool, bool), void*>::Test(fuzztest::internal::MoveOnlyAny&&) const::'lambda'(auto&&...)&&, std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>&, int&, int&, int&, int&, int&, int&, bool&, bool&) /usr/lib/gcc/x86_64-linux-gnu/14/../../../../include/c++/14/bits/invoke.h:96:14
    #7 0x57d84a535cc4 in decltype(auto) std::__apply_impl<fuzztest::internal::FixtureDriver<fuzztest::Domain<std::tuple<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>, int, int, int, int, int, int, bool, bool>>, fuzztest::internal::NoFixture, void (*)(std::basic_string_view<char, std::char_traits<char>>, int, int, int, int, int, int, bool, bool), void*>::Test(fuzztest::internal::MoveOnlyAny&&) const::'lambda'(auto&&...), std::tuple<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>, int, int, int, int, int, int, bool, bool>&, 0ul, 1ul, 2ul, 3ul, 4ul, 5ul, 6ul, 7ul, 8ul>(fuzztest::internal::FixtureDriver<fuzztest::Domain<std::tuple<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>, int, int, int, int, int, int, bool, bool>>, fuzztest::internal::NoFixture, void (*)(std::basic_string_view<char, std::char_traits<char>>, int, int, int, int, int, int, bool, bool), void*>::Test(fuzztest::internal::MoveOnlyAny&&) const::'lambda'(auto&&...)&&, std::tuple<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>, int, int, int, int, int, int, bool, bool>&, std::integer_sequence<unsigned long, 0ul, 1ul, 2ul, 3ul, 4ul, 5ul, 6ul, 7ul, 8ul>) /usr/lib/gcc/x86_64-linux-gnu/14/../../../../include/c++/14/tuple:2923:14
    #8 0x57d84a535cc4 in decltype(auto) std::apply<fuzztest::internal::FixtureDriver<fuzztest::Domain<std::tuple<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>, int, int, int, int, int, int, bool, bool>>, fuzztest::internal::NoFixture, void (*)(std::basic_string_view<char, std::char_traits<char>>, int, int, int, int, int, int, bool, bool), void*>::Test(fuzztest::internal::MoveOnlyAny&&) const::'lambda'(auto&&...), std::tuple<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>, int, int, int, int, int, int, bool, bool>&>(fuzztest::internal::FixtureDriver<fuzztest::Domain<std::tuple<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>, int, int, int, int, int, int, bool, bool>>, fuzztest::internal::NoFixture, void (*)(std::basic_string_view<char, std::char_traits<char>>, int, int, int, int, int, int, bool, bool), void*>::Test(fuzztest::internal::MoveOnlyAny&&) const::'lambda'(auto&&...)&&, std::tuple<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>, int, int, int, int, int, int, bool, bool>&) /usr/lib/gcc/x86_64-linux-gnu/14/../../../../include/c++/14/tuple:2938:14
    #9 0x57d84a535cc4 in fuzztest::internal::FixtureDriver<fuzztest::Domain<std::tuple<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>, int, int, int, int, int, int, bool, bool>>, fuzztest::internal::NoFixture, void (*)(std::basic_string_view<char, std::char_traits<char>>, int, int, int, int, int, int, bool, bool), void*>::Test(fuzztest::internal::MoveOnlyAny&&) const /mnt/h/Security/Binary/Project/libwebp/build-fuzz-clang/_deps/fuzztest-src/./fuzztest/internal/fixture_driver.h:292:5
    #10 0x57d84a5a7406 in fuzztest::internal::FuzzTestFuzzerImpl::RunOneInput(fuzztest::internal::FuzzTestFuzzerImpl::Input const&) /mnt/h/Security/Binary/Project/libwebp/build-fuzz-clang/_deps/fuzztest-src/fuzztest/internal/runtime.cc:927:20
    #11 0x57d84a5aac8d in fuzztest::internal::FuzzTestFuzzerImpl::RunInUnitTestMode(fuzztest::internal::Configuration const&)::$_0::operator()() const /mnt/h/Security/Binary/Project/libwebp/build-fuzz-clang/_deps/fuzztest-src/fuzztest/internal/runtime.cc:879:7
    #12 0x57d84a5aac8d in fuzztest::internal::FuzzTestFuzzerImpl::RunInUnitTestMode(fuzztest::internal::Configuration const&) /mnt/h/Security/Binary/Project/libwebp/build-fuzz-clang/_deps/fuzztest-src/fuzztest/internal/runtime.cc:833:3
    #13 0x57d84a567fde in fuzztest::internal::GTest_TestAdaptor::TestBody() /mnt/h/Security/Binary/Project/libwebp/build-fuzz-clang/_deps/fuzztest-src/./fuzztest/internal/googletest_adaptor.h:78:15
    #14 0x57d84a68feb4 in void testing::internal::HandleSehExceptionsInMethodIfSupported<testing::Test, void>(testing::Test*, void (testing::Test::*)(), char const*) /mnt/h/Security/Binary/Project/libwebp/build-fuzz-clang/_deps/googletest-src/googletest/src/gtest.cc:2612:10
    #15 0x57d84a68feb4 in void testing::internal::HandleExceptionsInMethodIfSupported<testing::Test, void>(testing::Test*, void (testing::Test::*)(), char const*) /mnt/h/Security/Binary/Project/libwebp/build-fuzz-clang/_deps/googletest-src/googletest/src/gtest.cc:2648:14
    #16 0x57d84a68fcdd in testing::Test::Run() /mnt/h/Security/Binary/Project/libwebp/build-fuzz-clang/_deps/googletest-src/googletest/src/gtest.cc:2687:5
    #17 0x57d84a691f23 in testing::TestInfo::Run() /mnt/h/Security/Binary/Project/libwebp/build-fuzz-clang/_deps/googletest-src/googletest/src/gtest.cc:2836:11
    #18 0x57d84a693b45 in testing::TestSuite::Run() /mnt/h/Security/Binary/Project/libwebp/build-fuzz-clang/_deps/googletest-src/googletest/src/gtest.cc:3015:30
    #19 0x57d84a6b37f3 in testing::internal::UnitTestImpl::RunAllTests() /mnt/h/Security/Binary/Project/libwebp/build-fuzz-clang/_deps/googletest-src/googletest/src/gtest.cc:5920:44
    #20 0x57d84a6b2ee4 in bool testing::internal::HandleSehExceptionsInMethodIfSupported<testing::internal::UnitTestImpl, bool>(testing::internal::UnitTestImpl*, bool (testing::internal::UnitTestImpl::*)(), char const*) /mnt/h/Security/Binary/Project/libwebp/build-fuzz-clang/_deps/googletest-src/googletest/src/gtest.cc:2612:10
    #21 0x57d84a6b2ee4 in bool testing::internal::HandleExceptionsInMethodIfSupported<testing::internal::UnitTestImpl, bool>(testing::internal::UnitTestImpl*, bool (testing::internal::UnitTestImpl::*)(), char const*) /mnt/h/Security/Binary/Project/libwebp/build-fuzz-clang/_deps/googletest-src/googletest/src/gtest.cc:2648:14
    #22 0x57d84a6b2c6f in testing::UnitTest::Run() /mnt/h/Security/Binary/Project/libwebp/build-fuzz-clang/_deps/googletest-src/googletest/src/gtest.cc:5484:10
    #23 0x57d84a53c649 in RUN_ALL_TESTS() /mnt/h/Security/Binary/Project/libwebp/build-fuzz-clang/_deps/googletest-src/googletest/include/gtest/gtest.h:2317:73
    #24 0x57d84a53c649 in main /mnt/h/Security/Binary/Project/libwebp/build-fuzz-clang/_deps/fuzztest-src/fuzztest/fuzztest_gtest_main.cc:28:10
    #25 0x7f5fa3233ca7 in __libc_start_call_main csu/../sysdeps/nptl/libc_start_call_main.h:58:16
    #26 0x7f5fa3233d64 in __libc_start_main csu/../csu/libc-start.c:360:3
    #27 0x57d84a4de0e0 in _start (/mnt/h/Security/Binary/Project/libwebp/build-fuzz-clang/tests/fuzzer/sharpyuv_fuzzer+0xf90e0) (BuildId: 656c2b97777d5e444554dcc12c803092a064889b)

==20281==Register values:
rax = 0x00000000000f0d4b  rbx = 0x000000000000000e  rcx = 0x00000786a5b9779d  rdx = 0x000000000000000d
rdi = 0x000000000786a5b9  rsi = 0x000000000000000e  rbp = 0x0000000000000000  rsp = 0x00007ffe41f4dda0
 r8 = 0x0000000000000000   r9 = 0x000057d84b1a0920  r10 = 0x00000000000000b8  r11 = 0x000057d84eaa4cf4
r12 = 0x000000000000000d  r13 = 0x000000000000000e  r14 = 0x0000000000000000  r15 = 0x000000000000005c
UndefinedBehaviorSanitizer can not provide additional info.
SUMMARY: UndefinedBehaviorSanitizer: SEGV (/mnt/h/Security/Binary/Project/libwebp/build-fuzz-clang/tests/fuzzer/sharpyuv_fuzzer+0x1566a4) (BuildId: 656c2b97777d5e444554dcc12c803092a064889b) in SharpYuvLinearToGamma
==20281==ABORTING
```

