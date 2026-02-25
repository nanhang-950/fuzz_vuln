```
CXX=/usr/bin/g++ CC=/usr/bin/gcc cargo fuzz run fuzzer_script_decoder_api
CXX=/usr/bin/g++ CC=/usr/bin/gcc cargo fuzz run fuzzer_script_animation
CXX=/usr/bin/g++ CC=/usr/bin/gcc cargo fuzz run fuzzer_script_farbfeld
CXX=/usr/bin/g++ CC=/usr/bin/gcc cargo fuzz run fuzzer_script_qoi
```

**Bug Description**

In `markdown-rs`, the Setext-style heading parsing path contains a function `on_exit_heading_setext_underline_sequence` that assumes a `heading` element exists on the parser’s state stack and uses `unreachable!()` as an assertion. A fuzzer can craft input that contains only a Setext underline line (e.g., `===` / `---`) without a corresponding heading text. This causes the parser to reach the branch without a `heading` on the stack, triggering the `unreachable!()` macro, causing a panic and crash, which results in a denial of service (DoS).

Example trigger input:

```
t
=
=
\=
=
```



------

**Steps to Reproduce**

1. Create a fuzz target:

```
#![no_main]
use libfuzzer_sys::fuzz_target;

fuzz_target!(|data: &[u8]| {
    if let Ok(s) = std::str::from_utf8(data) {
        let _ = markdown::to_html(s);
        let _ = markdown::to_html_with_options(s, &markdown::Options::gfm());
        let _ = markdown::to_mdast(s, &markdown::ParseOptions::default());
        let _ = markdown::to_mdast(s, &markdown::ParseOptions::gfm());
        let _ = markdown::to_mdast(s, &markdown::ParseOptions::mdx());
    }
});
```



1. Run the fuzzer:

```
cargo fuzz run fuzzer
```



1. Observed crash output:

```
thread '<unnamed>' (89412) panicked at /mnt/d/.../src/to_html.rs:197:37:
at least one buffer should exist
note: run with `RUST_BACKTRACE=1` environment variable to display a backtrace
==89412== ERROR: libFuzzer: deadly signal
...
```



The full backtrace (obtainable with `RUST_BACKTRACE=1`) shows that `expect()` failed in the function `line_ending_if_needed`.

------

**Root Cause**

`on_exit_heading_setext_underline_sequence` expects the parser state stack to contain the corresponding heading element when handling a Setext underline (`===` / `---`). The current implementation uses `unreachable!()` to assert that this branch should never be executed if the heading is missing. However, when encountering malformed input (e.g., only underline lines), the parser may reach this branch without constructing a heading, causing the `unreachable!()` macro to execute and trigger a panic.

This issue is a case of **insufficient defensive programming**. The parser should handle uncertain input robustly rather than asserting that impossible conditions will never occur.

------

**Impact**

- The panic immediately terminates the program and can be triggered by local or remote users, causing a **denial of service (DoS)**.
- Malformed Markdown files crafted via fuzzing can reliably trigger the crash.

------

**Expected Behavior**

The parser should safely exit or ignore anomalous underline lines when the heading is missing, rather than panicking. For example:

- Check whether the state stack is empty and safely return or report an error if the heading is missing.
- Ensure robust parsing for fuzzer or malicious input.

------

**Suggested Fixes**

- Update `on_exit_heading_setext_underline_sequence` to check the state stack before accessing its top element instead of using `unreachable!()`.
- Add fuzzing regression tests covering cases with only underline lines or missing headings.
- Improve overall parser resilience to malformed or malicious Markdown input.

------

**Environment**

- Module: `github.com/wooorm/markdown-rs`
- Platform: Ubuntu 22.04 (Docker)
- Rustc version: 1.92.0
- Fuzzing: `cargo-fuzz` + libFuzzer

```
thread '<unnamed>' (15966) panicked at /mnt/h/Security/Binary/Project/image/src/codecs/gif.rs:430:43:
Repeat::Finite should be non-zero
note: run with `RUST_BACKTRACE=1` environment variable to display a backtrace
==15966== ERROR: libFuzzer: deadly signal
    #0 0x592492ae8cc1 in __sanitizer_print_stack_trace /rustc/llvm/src/llvm-project/compiler-rt/lib/asan/asan_stack.cpp:87:3
    #1 0x592492d10d8a in fuzzer::PrintStackTrace() /root/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/libfuzzer-sys-0.4.12/libfuzzer/FuzzerUtil.cpp:210:38
    #2 0x592492ccb8e5 in fuzzer::Fuzzer::CrashCallback() /root/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/libfuzzer-sys-0.4.12/libfuzzer/FuzzerLoop.cpp:231:18
    #3 0x592492ccb8e5 in fuzzer::Fuzzer::CrashCallback() /root/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/libfuzzer-sys-0.4.12/libfuzzer/FuzzerLoop.cpp:226:6
    #4 0x7d498d449def  (/lib/x86_64-linux-gnu/libc.so.6+0x3fdef) (BuildId: def5460e3cee00bfee25b429c97bcc4853e5b3a8)
    #5 0x7d498d49e95b in __pthread_kill_implementation nptl/pthread_kill.c:43:17
    #6 0x7d498d449cc1 in raise signal/../sysdeps/posix/raise.c:26:13
    #7 0x7d498d4324ab in abort stdlib/abort.c:73:3
    #8 0x592492d39a59 in std::sys::pal::unix::abort_internal::hf5e36381f64f0a78 /rustc/6ba0ce40941eee1ca02e9ba49c791ada5158747a/library/std/src/sys/pal/unix/mod.rs:366:14
    #9 0x592492d39a48 in std::process::abort::h49f9c39b5600dd95 /rustc/6ba0ce40941eee1ca02e9ba49c791ada5158747a/library/std/src/process.rs:2499:5
    #10 0x592492cc34e4 in libfuzzer_sys::initialize::_$u7b$$u7b$closure$u7d$$u7d$::h011da2e033d5ec05 /root/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/libfuzzer-sys-0.4.12/src/lib.rs:94:9
    #11 0x592492d2a9b0 in _$LT$alloc..boxed..Box$LT$F$C$A$GT$$u20$as$u20$core..ops..function..Fn$LT$Args$GT$$GT$::call::hb662795a5a5f9b91 /rustc/6ba0ce40941eee1ca02e9ba49c791ada5158747a/library/alloc/src/boxed.rs:1999:9
    #12 0x592492d2a9b0 in std::panicking::panic_with_hook::hf343f38c566b7de3 /rustc/6ba0ce40941eee1ca02e9ba49c791ada5158747a/library/std/src/panicking.rs:842:13
    #13 0x592492d3a189 in std::panicking::panic_handler::_$u7b$$u7b$closure$u7d$$u7d$::h5999c7a88a85d27c /rustc/6ba0ce40941eee1ca02e9ba49c791ada5158747a/library/std/src/panicking.rs:707:13
    #14 0x592492d3a0e8 in std::sys::backtrace::__rust_end_short_backtrace::h66ef81f3bd341240 /rustc/6ba0ce40941eee1ca02e9ba49c791ada5158747a/library/std/src/sys/backtrace.rs:174:18
    #15 0x592492d2a3bc in __rustc::rust_begin_unwind /rustc/6ba0ce40941eee1ca02e9ba49c791ada5158747a/library/std/src/panicking.rs:698:5
    #16 0x592492d5955f in core::panicking::panic_fmt::h5434febb0b631f9c /rustc/6ba0ce40941eee1ca02e9ba49c791ada5158747a/library/core/src/panicking.rs:75:14
    #17 0x592492d59e4a in core::panicking::panic_display::hd9477ee3e3670115 /rustc/6ba0ce40941eee1ca02e9ba49c791ada5158747a/library/core/src/panicking.rs:259:5
    #18 0x592492d59e4a in core::option::expect_failed::hf93710017d34c715 /rustc/6ba0ce40941eee1ca02e9ba49c791ada5158747a/library/core/src/option.rs:2139:5
    #19 0x592492b7d874 in expect<core::num::nonzero::NonZero<u32>> /root/.rustup/toolchains/nightly-x86_64-unknown-linux-gnu/lib/rustlib/src/rust/library/core/src/option.rs:964:21
    #20 0x592492b7d874 in loop_count<std::io::cursor::Cursor<&[u8]>> /mnt/h/Security/Binary/Project/image/src/codecs/gif.rs:430:43
    #21 0x592492b7d874 in fuzz_gif /mnt/h/Security/Binary/Project/image/fuzz/fuzzers/fuzzer_script_animation.rs:29:21
    #22 0x592492b7d874 in fuzzer_script_animation::_::__libfuzzer_sys_run::h40909a138bfec27c /mnt/h/Security/Binary/Project/image/fuzz/fuzzers/fuzzer_script_animation.rs:87:14
    #23 0x592492b775e8 in rust_fuzzer_test_input /root/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/libfuzzer-sys-0.4.12/src/lib.rs:276:60
    #24 0x592492cc3975 in {closure#0} /root/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/libfuzzer-sys-0.4.12/src/lib.rs:62:9
    #25 0x592492cc3975 in std::panicking::catch_unwind::do_call::h1d1434b3113fdc85 /root/.rustup/toolchains/nightly-x86_64-unknown-linux-gnu/lib/rustlib/src/rust/library/std/src/panicking.rs:590:40
    #26 0x592492cc4ae8 in __rust_try libfuzzer_sys.a11ee64f1d731ead-cgu.0
    #27 0x592492cc364d in catch_unwind<i32, libfuzzer_sys::test_input_wrap::{closure_env#0}> /root/.rustup/toolchains/nightly-x86_64-unknown-linux-gnu/lib/rustlib/src/rust/library/std/src/panicking.rs:553:19
    #28 0x592492cc364d in catch_unwind<libfuzzer_sys::test_input_wrap::{closure_env#0}, i32> /root/.rustup/toolchains/nightly-x86_64-unknown-linux-gnu/lib/rustlib/src/rust/library/std/src/panic.rs:359:14
    #29 0x592492cc364d in LLVMFuzzerTestOneInput /root/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/libfuzzer-sys-0.4.12/src/lib.rs:60:22
    #30 0x592492ccbe54 in fuzzer::Fuzzer::ExecuteCallback(unsigned char const*, unsigned long) /root/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/libfuzzer-sys-0.4.12/libfuzzer/FuzzerLoop.cpp:619:15
    #31 0x592492cd2aaf in fuzzer::Fuzzer::RunOne(unsigned char const*, unsigned long, bool, fuzzer::InputInfo*, bool, bool*) /root/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/libfuzzer-sys-0.4.12/libfuzzer/FuzzerLoop.cpp:516:22
    #32 0x592492cd3c9a in fuzzer::Fuzzer::MutateAndTestOne() /root/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/libfuzzer-sys-0.4.12/libfuzzer/FuzzerLoop.cpp:765:25
    #33 0x592492cd4a37 in fuzzer::Fuzzer::Loop(std::vector<fuzzer::SizedFile, std::allocator<fuzzer::SizedFile>>&) /root/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/libfuzzer-sys-0.4.12/libfuzzer/FuzzerLoop.cpp:910:21
    #34 0x592492ce93de in fuzzer::FuzzerDriver(int*, char***, int (*)(unsigned char const*, unsigned long)) /root/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/libfuzzer-sys-0.4.12/libfuzzer/FuzzerDriver.cpp:923:10
    #35 0x592492d0fcc2 in main /root/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/libfuzzer-sys-0.4.12/libfuzzer/FuzzerMain.cpp:20:30
    #36 0x7d498d433ca7 in __libc_start_call_main csu/../sysdeps/nptl/libc_start_call_main.h:58:16
    #37 0x7d498d433d64 in __libc_start_main csu/../csu/libc-start.c:360:3
    #38 0x592492a533e0 in _start (/mnt/h/Security/Binary/Project/image/fuzz/target/x86_64-unknown-linux-gnu/release/fuzzer_script_animation+0x1ac3e0) (BuildId: add4d5411182d4bc872d8eac8f5ef1c917622d5c)

NOTE: libFuzzer has rudimentary signal handlers.
      Combine libFuzzer with AddressSanitizer or similar for better crash reports.
SUMMARY: libFuzzer: deadly signal
MS: 3 CMP-PersAutoDict-CrossOver- DE: "FNP\211"-"\015\012\032\012"-; base unit: adc83b19e793491b1c6ea0fd8b46cd9f32e592fc
0x4e,0x47,0x49,0x46,0x38,0x37,0x61,0x46,0x4e,0x50,0x89,0xd,0xa,0x1a,0xa,0xa,
NGIF87aFNP\211\015\012\032\012\012
artifact_prefix='/mnt/h/Security/Binary/Project/image/fuzz/artifacts/fuzzer_script_animation/'; Test unit written to /mnt/h/Security/Binary/Project/image/fuzz/artifacts/fuzzer_script_animation/crash-b3803e80ca3e89cecaf4b784a8b6ad9ab77c050b
Base64: TkdJRjg3YUZOUIkNChoKCg==

────────────────────────────────────────────────────────────────────────────────

Failing input:

        fuzz/artifacts/fuzzer_script_animation/crash-b3803e80ca3e89cecaf4b784a8b6ad9ab77c050b

Output of `std::fmt::Debug`:

        [78, 71, 73, 70, 56, 55, 97, 70, 78, 80, 137, 13, 10, 26, 10, 10]

Reproduce with:

        cargo fuzz run fuzzer_script_animation fuzz/artifacts/fuzzer_script_animation/crash-b3803e80ca3e89cecaf4b784a8b6ad9ab77c050b

Minimize test case with:

        cargo fuzz tmin fuzzer_script_animation fuzz/artifacts/fuzzer_script_animation/crash-b3803e80ca3e89cecaf4b784a8b6ad9ab77c050b
```

