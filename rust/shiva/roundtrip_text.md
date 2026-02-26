# Panic in `xml::Transformer::parse()` Due to `Option::unwrap()` on `None`

## Bug Description

While fuzzing the `shiva` repository (`github.com/igumnoff/shiva`) using `cargo-fuzz`, a runtime panic was triggered at:

```
lib/src/xml.rs:113:35
```

The crash occurs inside:

```
shiva::xml::Transformer::parse()
```

where an `Option::unwrap()` is called on a `None` value:

```rust
Option::unwrap()
```

This immediately aborts execution under libFuzzer:

```
called `Option::unwrap()` on a `None` value
```

### Stack Trace Excerpt

```
thread '<unnamed>' (3017) panicked at /mnt/h/Security/Binary/Project/shiva/lib/src/xml.rs:113:35:
called `Option::unwrap()` on a `None` value
note: run with `RUST_BACKTRACE=1` environment variable to display a backtrace
==3017== ERROR: libFuzzer: deadly signal
    #0 0x5e363966bed1 in __sanitizer_print_stack_trace /rustc/llvm/src/llvm-project/compiler-rt/lib/asan/asan_stack.cpp:87:3
    #1 0x5e3639fd918a in fuzzer::PrintStackTrace() /root/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/libfuzzer-sys-0.4.12/libfuzzer/FuzzerUtil.cpp:210:5
    #2 0x5e3639fa9803 in fuzzer::Fuzzer::CrashCallback() /root/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/libfuzzer-sys-0.4.12/libfuzzer/FuzzerLoop.cpp:231:3
    #3 0x751e33b2cdef  (/lib/x86_64-linux-gnu/libc.so.6+0x3fdef) (BuildId: def5460e3cee00bfee25b429c97bcc4853e5b3a8)
    #4 0x751e33b8195b in __pthread_kill_implementation nptl/pthread_kill.c:43:17
    #5 0x751e33b2ccc1 in raise signal/../sysdeps/posix/raise.c:26:13
    #6 0x751e33b154ab in abort stdlib/abort.c:73:3
    #7 0x5e363a000d99 in std::sys::pal::unix::abort_internal::hf5e36381f64f0a78 /rustc/6ba0ce40941eee1ca02e9ba49c791ada5158747a/library/std/src/sys/pal/unix/mod.rs:366:14
    #8 0x5e363a000d88 in std::process::abort::h49f9c39b5600dd95 /rustc/6ba0ce40941eee1ca02e9ba49c791ada5158747a/library/std/src/process.rs:2499:5
    #9 0x5e3639fa75e4 in libfuzzer_sys::initialize::_$u7b$$u7b$closure$u7d$$u7d$::hdfab12933489121a /root/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/libfuzzer-sys-0.4.12/src/lib.rs:94:9
    #10 0x5e3639fefd40 in _$LT$alloc..boxed..Box$LT$F$C$A$GT$$u20$as$u20$core..ops..function..Fn$LT$Args$GT$$GT$::call::hb662795a5a5f9b91 /rustc/6ba0ce40941eee1ca02e9ba49c791ada5158747a/library/alloc/src/boxed.rs:1999:9
    #11 0x5e3639fefd40 in std::panicking::panic_with_hook::hf343f38c566b7de3 /rustc/6ba0ce40941eee1ca02e9ba49c791ada5158747a/library/std/src/panicking.rs:842:13
    #12 0x5e363a0014e5 in std::panicking::panic_handler::_$u7b$$u7b$closure$u7d$$u7d$::h5999c7a88a85d27c /rustc/6ba0ce40941eee1ca02e9ba49c791ada5158747a/library/std/src/panicking.rs:700:13
    #13 0x5e363a001478 in std::sys::backtrace::__rust_end_short_backtrace::h66ef81f3bd341240 /rustc/6ba0ce40941eee1ca02e9ba49c791ada5158747a/library/std/src/sys/backtrace.rs:174:18
    #14 0x5e3639fef74c in __rustc::rust_begin_unwind /rustc/6ba0ce40941eee1ca02e9ba49c791ada5158747a/library/std/src/panicking.rs:698:5
    #15 0x5e363a0285df in core::panicking::panic_fmt::h5434febb0b631f9c /rustc/6ba0ce40941eee1ca02e9ba49c791ada5158747a/library/core/src/panicking.rs:75:14
    #16 0x5e363a02849b in core::panicking::panic::he71bac4cfb86ea9f /rustc/6ba0ce40941eee1ca02e9ba49c791ada5158747a/library/core/src/panicking.rs:145:5
    #17 0x5e363a029968 in core::option::unwrap_failed::hd200657c7c758179 /rustc/6ba0ce40941eee1ca02e9ba49c791ada5158747a/library/core/src/option.rs:2130:5
    #18 0x5e3639712bdf in unwrap<&shiva::xml::Node> /root/.rustup/toolchains/nightly-x86_64-unknown-linux-gnu/lib/rustlib/src/rust/library/core/src/option.rs:1009:21
    #19 0x5e3639712bdf in _$LT$shiva..xml..Transformer$u20$as$u20$shiva..core..TransformerTrait$GT$::parse::haeadaf56ab9164b1 /mnt/h/Security/Binary/Project/shiva/lib/src/xml.rs:113:35
    #20 0x5e36396ea9ed in shiva::core::Document::parse::hb8c0a146e16f31e9 /mnt/h/Security/Binary/Project/shiva/lib/src/core.rs:246:34
    #21 0x5e3639692d24 in parse_dispatch::_::__libfuzzer_sys_run::h3c352d44735239f5 /mnt/h/Security/Binary/Project/shiva/lib/fuzz/fuzz_targets/parse_dispatch.rs:23:13
    #22 0x5e3639693418 in rust_fuzzer_test_input /root/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/libfuzzer-sys-0.4.12/src/lib.rs:276:60
    #23 0x5e3639fa5f65 in {closure#0} /root/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/libfuzzer-sys-0.4.12/src/lib.rs:62:9
    #24 0x5e3639fa5f65 in std::panicking::catch_unwind::do_call::h4d5fc7bfddb77e53 /root/.rustup/toolchains/nightly-x86_64-unknown-linux-gnu/lib/rustlib/src/rust/library/std/src/panicking.rs:590:40
    #25 0x5e3639fa7e18 in __rust_try libfuzzer_sys.14efa759ce49c1ba-cgu.0
    #26 0x5e3639fa5c3d in catch_unwind<i32, libfuzzer_sys::test_input_wrap::{closure_env#0}> /root/.rustup/toolchains/nightly-x86_64-unknown-linux-gnu/lib/rustlib/src/rust/library/std/src/panicking.rs:553:19
    #27 0x5e3639fa5c3d in catch_unwind<libfuzzer_sys::test_input_wrap::{closure_env#0}, i32> /root/.rustup/toolchains/nightly-x86_64-unknown-linux-gnu/lib/rustlib/src/rust/library/std/src/panic.rs:359:14
    #28 0x5e3639fa5c3d in LLVMFuzzerTestOneInput /root/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/libfuzzer-sys-0.4.12/src/lib.rs:60:22
    #29 0x5e3639fab418 in fuzzer::Fuzzer::ExecuteCallback(unsigned char const*, unsigned long) /root/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/libfuzzer-sys-0.4.12/libfuzzer/FuzzerLoop.cpp:619:13
    #30 0x5e3639fb7ed9 in fuzzer::RunOneTest(fuzzer::Fuzzer*, char const*, unsigned long) /root/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/libfuzzer-sys-0.4.12/libfuzzer/FuzzerDriver.cpp:335:6
    #31 0x5e3639fbe629 in fuzzer::FuzzerDriver(int*, char***, int (*)(unsigned char const*, unsigned long)) /root/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/libfuzzer-sys-0.4.12/libfuzzer/FuzzerDriver.cpp:871:9
    #32 0x5e3639fd8332 in main /root/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/libfuzzer-sys-0.4.12/libfuzzer/FuzzerMain.cpp:20:10
    #33 0x751e33b16ca7 in __libc_start_call_main csu/../sysdeps/nptl/libc_start_call_main.h:58:16
    #34 0x751e33b16d64 in __libc_start_main csu/../csu/libc-start.c:360:3
    #35 0x5e36395d5e60 in _start (/mnt/h/Security/Binary/Project/shiva/lib/fuzz/target/x86_64-unknown-linux-gnu/release/parse_dispatch+0x979e60) (BuildId: eabe1851688a8d4e979fa57e558ebf5e66bf3af6)
```

The panic is reliably reproducible using malformed XML input during fuzzing.

------

## Steps to Reproduce

### Install cargo-fuzz

```bash
cargo install cargo-fuzz
```

### Crashing Input

```
00000000: 0574 0a                                  .t.
```

------

### Fuzzer Target

```rust
#![no_main]

use libfuzzer_sys::fuzz_target;
use shiva::core::{Document, DocumentType};

fuzz_target!(|data: &[u8]| {
    if data.len() < 3 {
        return;
    }

    const TEXT_TYPES: [DocumentType; 7] = [
        DocumentType::Markdown,
        DocumentType::HTML,
        DocumentType::Text,
        DocumentType::Json,
        DocumentType::CSV,
        DocumentType::XML,
        DocumentType::RTF,
    ];

    let parse_ty = TEXT_TYPES[(data[0] as usize) % TEXT_TYPES.len()];
    let generate_ty = TEXT_TYPES[(data[1] as usize) % TEXT_TYPES.len()];
    let input = data[2..].to_vec().into();

    if let Ok(document) = Document::parse(&input, parse_ty) {
        if let Ok(output) = document.generate(generate_ty) {
            let _ = Document::parse(&output, generate_ty);
        }
    }
});
```

------

### Run Existing Fuzz Target

From the `lib/` directory:

```bash
RUST_BACKTRACE=1 cargo fuzz run roundtrip_text
```

The crash occurs in:

```
fuzz/fuzz_targets/roundtrip_text.rs
```

------

## Observed Panic

```
thread '<unnamed>' panicked at lib/src/xml.rs:113:35:
called `Option::unwrap()` on a `None` value
```

Followed by libFuzzer abort:

```
==11802== ERROR: libFuzzer: deadly signal
```

------

## Root Cause Analysis

At line 113 in `xml.rs`, the code performs:

```rust
some_option.unwrap()
```

The fuzz-generated input creates an invalid or incomplete XML structure, resulting in:

- A required node being missing
- A parent-child structural assumption failing
- A lookup operation returning `None`

Because `.unwrap()` is used instead of proper error handling, malformed input triggers a panic instead of returning an error.

This violates a fundamental robustness invariant:

> Parsing untrusted input must never panic.

As a document parser, all malformed input should result in a structured error (`Result::Err`) rather than terminating the process.

------

## Why This Is a Problem

- Panics in parsing logic can become denial-of-service (DoS) vectors
- Fuzzing easily discovers such failure paths
- The crate may be used in server-side or automated processing pipelines
- Using `unwrap()` in public-facing parsing logic is generally unsafe

------

## Expected Behavior

For malformed XML input, the parser should:

- Return `Err(ParseError)`
- Gracefully reject invalid structures
- Never trigger a `panic!`

------

## Suggested Fixes

### Option A: Replace `unwrap()` with Proper Error Propagation

Original code:

```rust
let node = maybe_node.unwrap();
```

Replace with:

```rust
let node = maybe_node.ok_or(ParseError::InvalidStructure)?;
```

Or:

```rust
let node = match maybe_node {
    Some(n) => n,
    None => return Err(ParseError::InvalidStructure),
};
```

------

### Option B: Add Structural Validation Before Transformation

If `Transformer::parse()` assumes certain invariants, validate them explicitly:

```rust
if maybe_node.is_none() {
    return Err(ParseError::MissingNode);
}
```

------

### Option C: Add Fuzz Regression Test

Preserve the crashing input in:

```
fuzz/corpus/roundtrip_text/
```

to prevent regressions.

------

## Minimal Defensive Patch Example

```rust
// Before
let node = something.unwrap();

// After
let node = something.ok_or(ParseError::InvalidXml)?;
```

------

## Environment

- Crate: github.com/igumnoff/shiva
- Platform: Ubuntu 22.04
- Rust: rustc 1.91.0-nightly
- Fuzzing tool: cargo-fuzz
