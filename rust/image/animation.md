**Bug Description**

In `markdown-rs`, the Setext-style heading parsing path contains a function `on_exit_heading_setext_underline_sequence` that assumes a `heading` element exists on the parser’s state stack and uses `unreachable!()` as an assertion. A fuzzer can craft input that contains only a Setext underline line (e.g., `===` / `---`) without a corresponding heading text. This causes the parser to reach the branch without a `heading` on the stack, triggering the `unreachable!()` macro, causing a panic and crash, which results in a denial of service (DoS).

Example trigger input:

```
00000000: 4e47 4946 3837 6146 4e50 890d 0a1a 0a0a  NGIF87aFNP......
```

**Steps to Reproduce**

1. Create a fuzz target:

```rust
#![no_main]
#[macro_use]
extern crate libfuzzer_sys;
extern crate image;

use std::io::Cursor;

use image::codecs::gif::GifDecoder;
use image::codecs::png::PngDecoder;
use image::codecs::webp::WebPDecoder;
use image::{AnimationDecoder, ImageDecoder, Limits};

const MAX_FRAME_BYTES: u64 = 32 * 1024 * 1024;
const MAX_WEBP_PIXELS: u64 = 16 * 1024 * 1024;
const MAX_FRAMES: usize = 8;

fn fuzz_gif(data: &[u8]) {
    let mut decoder = match GifDecoder::new(Cursor::new(data)) {
        Ok(decoder) => decoder,
        Err(_) => return,
    };

    let mut limits = Limits::default();
    limits.max_alloc = Some(MAX_FRAME_BYTES);
    if decoder.set_limits(limits).is_err() {
        return;
    }

    let _ = decoder.loop_count();
    for frame in decoder.into_frames().take(MAX_FRAMES) {
        let _ = frame;
    }
}

fn fuzz_webp(data: &[u8]) {
    let decoder = match WebPDecoder::new(Cursor::new(data)) {
        Ok(decoder) => decoder,
        Err(_) => return,
    };

    if !decoder.has_animation() {
        return;
    }

    let (width, height) = decoder.dimensions();
    if u64::from(width) * u64::from(height) > MAX_WEBP_PIXELS {
        return;
    }

    let _ = decoder.loop_count();
    for frame in decoder.into_frames().take(MAX_FRAMES) {
        let _ = frame;
    }
}

fn fuzz_apng(data: &[u8]) {
    let mut limits = Limits::default();
    limits.max_alloc = Some(MAX_FRAME_BYTES);

    let decoder = match PngDecoder::with_limits(Cursor::new(data), limits) {
        Ok(decoder) => decoder,
        Err(_) => return,
    };

    match decoder.is_apng() {
        Ok(true) => {}
        Ok(false) | Err(_) => return,
    }

    let decoder = match decoder.apng() {
        Ok(decoder) => decoder,
        Err(_) => return,
    };

    let _ = decoder.loop_count();
    for frame in decoder.into_frames().take(MAX_FRAMES) {
        let _ = frame;
    }
}

fuzz_target!(|data: &[u8]| {
    if data.len() < 2 {
        return;
    }

    match data[0] % 3 {
        0 => fuzz_gif(&data[1..]),
        1 => fuzz_webp(&data[1..]),
        _ => fuzz_apng(&data[1..]),
    }
});
```

- Run the fuzzer:

```
CXX=/usr/bin/g++ CC=/usr/bin/gcc cargo fuzz run fuzzer_script_animation
```

- Observed crash output:

```
thread '<unnamed>' (1828) panicked at /mnt/h/Security/Binary/Project/image/src/codecs/gif.rs:430:43:
Repeat::Finite should be non-zero
note: run with `RUST_BACKTRACE=1` environment variable to display a backtrace
==1828== ERROR: libFuzzer: deadly signal
    #0 0x5e2524edacc1 in __sanitizer_print_stack_trace /rustc/llvm/src/llvm-project/compiler-rt/lib/asan/asan_stack.cpp:87:3
    #1 0x5e2525102d8a in fuzzer::PrintStackTrace() /root/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/libfuzzer-sys-0.4.12/libfuzzer/FuzzerUtil.cpp:210:38
    #2 0x5e25250bd8e5 in fuzzer::Fuzzer::CrashCallback() /root/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/libfuzzer-sys-0.4.12/libfuzzer/FuzzerLoop.cpp:231:18
    #3 0x5e25250bd8e5 in fuzzer::Fuzzer::CrashCallback() /root/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/libfuzzer-sys-0.4.12/libfuzzer/FuzzerLoop.cpp:226:6
    #4 0x794d46049def  (/lib/x86_64-linux-gnu/libc.so.6+0x3fdef) (BuildId: def5460e3cee00bfee25b429c97bcc4853e5b3a8)
    #5 0x794d4609e95b in __pthread_kill_implementation nptl/pthread_kill.c:43:17
    #6 0x794d46049cc1 in raise signal/../sysdeps/posix/raise.c:26:13
    #7 0x794d460324ab in abort stdlib/abort.c:73:3
    #8 0x5e252512ba59 in std::sys::pal::unix::abort_internal::hf5e36381f64f0a78 /rustc/6ba0ce40941eee1ca02e9ba49c791ada5158747a/library/std/src/sys/pal/unix/mod.rs:366:14
    #9 0x5e252512ba48 in std::process::abort::h49f9c39b5600dd95 /rustc/6ba0ce40941eee1ca02e9ba49c791ada5158747a/library/std/src/process.rs:2499:5
    #10 0x5e25250b54e4 in libfuzzer_sys::initialize::_$u7b$$u7b$closure$u7d$$u7d$::h011da2e033d5ec05 /root/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/libfuzzer-sys-0.4.12/src/lib.rs:94:9
    #11 0x5e252511c9b0 in _$LT$alloc..boxed..Box$LT$F$C$A$GT$$u20$as$u20$core..ops..function..Fn$LT$Args$GT$$GT$::call::hb662795a5a5f9b91 /rustc/6ba0ce40941eee1ca02e9ba49c791ada5158747a/library/alloc/src/boxed.rs:1999:9
    #12 0x5e252511c9b0 in std::panicking::panic_with_hook::hf343f38c566b7de3 /rustc/6ba0ce40941eee1ca02e9ba49c791ada5158747a/library/std/src/panicking.rs:842:13
    #13 0x5e252512c189 in std::panicking::panic_handler::_$u7b$$u7b$closure$u7d$$u7d$::h5999c7a88a85d27c /rustc/6ba0ce40941eee1ca02e9ba49c791ada5158747a/library/std/src/panicking.rs:707:13
    #14 0x5e252512c0e8 in std::sys::backtrace::__rust_end_short_backtrace::h66ef81f3bd341240 /rustc/6ba0ce40941eee1ca02e9ba49c791ada5158747a/library/std/src/sys/backtrace.rs:174:18
    #15 0x5e252511c3bc in __rustc::rust_begin_unwind /rustc/6ba0ce40941eee1ca02e9ba49c791ada5158747a/library/std/src/panicking.rs:698:5
    #16 0x5e252514b55f in core::panicking::panic_fmt::h5434febb0b631f9c /rustc/6ba0ce40941eee1ca02e9ba49c791ada5158747a/library/core/src/panicking.rs:75:14
    #17 0x5e252514be4a in core::panicking::panic_display::hd9477ee3e3670115 /rustc/6ba0ce40941eee1ca02e9ba49c791ada5158747a/library/core/src/panicking.rs:259:5
    #18 0x5e252514be4a in core::option::expect_failed::hf93710017d34c715 /rustc/6ba0ce40941eee1ca02e9ba49c791ada5158747a/library/core/src/option.rs:2139:5
    #19 0x5e2524f6f874 in expect<core::num::nonzero::NonZero<u32>> /root/.rustup/toolchains/nightly-x86_64-unknown-linux-gnu/lib/rustlib/src/rust/library/core/src/option.rs:964:21
    #20 0x5e2524f6f874 in loop_count<std::io::cursor::Cursor<&[u8]>> /mnt/h/Security/Binary/Project/image/src/codecs/gif.rs:430:43
    #21 0x5e2524f6f874 in fuzz_gif /mnt/h/Security/Binary/Project/image/fuzz/fuzzers/fuzzer_script_animation.rs:29:21
    #22 0x5e2524f6f874 in fuzzer_script_animation::_::__libfuzzer_sys_run::h40909a138bfec27c /mnt/h/Security/Binary/Project/image/fuzz/fuzzers/fuzzer_script_animation.rs:87:14
    #23 0x5e2524f695e8 in rust_fuzzer_test_input /root/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/libfuzzer-sys-0.4.12/src/lib.rs:276:60
    #24 0x5e25250b5975 in {closure#0} /root/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/libfuzzer-sys-0.4.12/src/lib.rs:62:9
    #25 0x5e25250b5975 in std::panicking::catch_unwind::do_call::h1d1434b3113fdc85 /root/.rustup/toolchains/nightly-x86_64-unknown-linux-gnu/lib/rustlib/src/rust/library/std/src/panicking.rs:590:40
    #26 0x5e25250b6ae8 in __rust_try libfuzzer_sys.a11ee64f1d731ead-cgu.0
    #27 0x5e25250b564d in catch_unwind<i32, libfuzzer_sys::test_input_wrap::{closure_env#0}> /root/.rustup/toolchains/nightly-x86_64-unknown-linux-gnu/lib/rustlib/src/rust/library/std/src/panicking.rs:553:19
    #28 0x5e25250b564d in catch_unwind<libfuzzer_sys::test_input_wrap::{closure_env#0}, i32> /root/.rustup/toolchains/nightly-x86_64-unknown-linux-gnu/lib/rustlib/src/rust/library/std/src/panic.rs:359:14
    #29 0x5e25250b564d in LLVMFuzzerTestOneInput /root/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/libfuzzer-sys-0.4.12/src/lib.rs:60:22
    #30 0x5e25250bde54 in fuzzer::Fuzzer::ExecuteCallback(unsigned char const*, unsigned long) /root/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/libfuzzer-sys-0.4.12/libfuzzer/FuzzerLoop.cpp:619:15
    #31 0x5e25250ce798 in fuzzer::RunOneTest(fuzzer::Fuzzer*, char const*, unsigned long) /root/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/libfuzzer-sys-0.4.12/libfuzzer/FuzzerDriver.cpp:335:21
    #32 0x5e25250dadc1 in fuzzer::FuzzerDriver(int*, char***, int (*)(unsigned char const*, unsigned long)) /root/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/libfuzzer-sys-0.4.12/libfuzzer/FuzzerDriver.cpp:871:19
    #33 0x5e2525101cc2 in main /root/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/libfuzzer-sys-0.4.12/libfuzzer/FuzzerMain.cpp:20:30
    #34 0x794d46033ca7 in __libc_start_call_main csu/../sysdeps/nptl/libc_start_call_main.h:58:16
    #35 0x794d46033d64 in __libc_start_main csu/../csu/libc-start.c:360:3
    #36 0x5e2524e453e0 in _start (/mnt/h/Security/Binary/Project/image/fuzz/target/x86_64-unknown-linux-gnu/release/fuzzer_script_animation+0x1ac3e0) (BuildId: add4d5411182d4bc872d8eac8f5ef1c917622d5c)
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

- Module: `github.com/image-rs/image`
- Platform: Ubuntu 22.04
- Rustc version: 1.91.0
- Fuzzing: `cargo-fuzz`

