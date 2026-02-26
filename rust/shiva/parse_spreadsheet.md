# Panic in `xls::Transformer::parse()` Due to `Result::expect()` on Malformed Spreadsheet Input

## Bug Description

While fuzzing the `shiva` repository (`github.com/igumnoff/shiva`) using `cargo-fuzz`, a runtime panic was triggered at:

```
lib/src/xls.rs:18:43
```

The crash occurs inside:

```
shiva::xls::Transformer::parse()
```

where `Result::expect()` is invoked on an `Err` value returned from the XLS parser:

```
Cannot open xls file from bytes: Cfb(Io(Error { kind: UnexpectedEof, message: "failed to fill whole buffer" }))
```

This immediately aborts execution under libFuzzer:

```
==10899== ERROR: libFuzzer: deadly signal
```

### Stack Trace Excerpt

```
thread '<unnamed>' (10899) panicked at /mnt/h/Security/Binary/Project/shiva/lib/src/xls.rs:18:43:
Cannot open xls file from bytes: Cfb(Io(Error { kind: UnexpectedEof, message: "failed to fill whole buffer" }))
note: run with `RUST_BACKTRACE=1` environment variable to display a backtrace
==10899== ERROR: libFuzzer: deadly signal
...
#16 core::result::unwrap_failed
#17 expect<calamine::xls::Xls<...>, calamine::xls::XlsError>
#18 shiva::xls::Transformer::parse
    /mnt/h/Security/Binary/Project/shiva/lib/src/xls.rs:18:43
#19 shiva::core::Document::parse
    /mnt/h/Security/Binary/Project/shiva/lib/src/core.rs:250:34
#20 parse_spreadsheet fuzz target
    /mnt/h/Security/Binary/Project/shiva/lib/fuzz/fuzz_targets/parse_spreadsheet.rs:17:27
...
```

The panic is reliably reproducible with malformed spreadsheet input during fuzzing.

------

## Steps to Reproduce

### Install cargo-fuzz

```bash
cargo install cargo-fuzz
```

### Crashing Input

```
00000000: 00ff                                     ..
```

### Fuzzer Target

```rust
#![no_main]

use libfuzzer_sys::fuzz_target;
use shiva::core::{Document, DocumentType};

fuzz_target!(|data: &[u8]| {
    if data.len() < 2 {
        return;
    }

    const SHEET_TYPES: [DocumentType; 3] =
        [DocumentType::XLS, DocumentType::XLSX, DocumentType::ODS];

    let parse_ty = SHEET_TYPES[(data[0] as usize) % SHEET_TYPES.len()];
    let input = data[1..].to_vec().into();

    if let Ok(document) = Document::parse(&input, parse_ty) {
        if parse_ty != DocumentType::XLS {
            let _ = document.generate(parse_ty);
        }
    }
});
```

### Run the Fuzz Target

From the `lib/` directory:

```bash
RUST_BACKTRACE=1 cargo fuzz run parse_spreadsheet
```

(Or the fuzz target name used in your stack trace: `fuzz_targets/parse_spreadsheet.rs`.)

------

## Observed Panic

```
thread '<unnamed>' (10899) panicked at /mnt/h/Security/Binary/Project/shiva/lib/src/xls.rs:18:43:
Cannot open xls file from bytes: Cfb(Io(Error { kind: UnexpectedEof, message: "failed to fill whole buffer" }))
note: run with `RUST_BACKTRACE=1` environment variable to display a backtrace
==10899== ERROR: libFuzzer: deadly signal
    #0 0x598bfe833181 in __sanitizer_print_stack_trace /rustc/llvm/src/llvm-project/compiler-rt/lib/asan/asan_stack.cpp:87:3
    #1 0x598c000f881a in fuzzer::PrintStackTrace() /root/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/libfuzzer-sys-0.4.12/libfuzzer/FuzzerUtil.cpp:210:5
    #2 0x598c000c8e93 in fuzzer::Fuzzer::CrashCallback() /root/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/libfuzzer-sys-0.4.12/libfuzzer/FuzzerLoop.cpp:231:3
    #3 0x7a1a00a49def  (/lib/x86_64-linux-gnu/libc.so.6+0x3fdef) (BuildId: def5460e3cee00bfee25b429c97bcc4853e5b3a8)
    #4 0x7a1a00a9e95b in __pthread_kill_implementation nptl/pthread_kill.c:43:17
    #5 0x7a1a00a49cc1 in raise signal/../sysdeps/posix/raise.c:26:13
    #6 0x7a1a00a324ab in abort stdlib/abort.c:73:3
    #7 0x598c00125da9 in std::sys::pal::unix::abort_internal::hf5e36381f64f0a78 /rustc/6ba0ce40941eee1ca02e9ba49c791ada5158747a/library/std/src/sys/pal/unix/mod.rs:366:14
    #8 0x598c00125d98 in std::process::abort::h49f9c39b5600dd95 /rustc/6ba0ce40941eee1ca02e9ba49c791ada5158747a/library/std/src/process.rs:2499:5
    #9 0x598c000c6c74 in libfuzzer_sys::initialize::_$u7b$$u7b$closure$u7d$$u7d$::hdfab12933489121a /root/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/libfuzzer-sys-0.4.12/src/lib.rs:94:9
    #10 0x598c001132e2 in _$LT$alloc..boxed..Box$LT$F$C$A$GT$$u20$as$u20$core..ops..function..Fn$LT$Args$GT$$GT$::call::hb662795a5a5f9b91 /rustc/6ba0ce40941eee1ca02e9ba49c791ada5158747a/library/alloc/src/boxed.rs:1999:9
    #11 0x598c001132e2 in std::panicking::panic_with_hook::hf343f38c566b7de3 /rustc/6ba0ce40941eee1ca02e9ba49c791ada5158747a/library/std/src/panicking.rs:842:13
    #12 0x598c00128339 in std::panicking::panic_handler::_$u7b$$u7b$closure$u7d$$u7d$::h5999c7a88a85d27c /rustc/6ba0ce40941eee1ca02e9ba49c791ada5158747a/library/std/src/panicking.rs:707:13
    #13 0x598c00128298 in std::sys::backtrace::__rust_end_short_backtrace::h66ef81f3bd341240 /rustc/6ba0ce40941eee1ca02e9ba49c791ada5158747a/library/std/src/sys/backtrace.rs:174:18
    #14 0x598c00112b5c in __rustc::rust_begin_unwind /rustc/6ba0ce40941eee1ca02e9ba49c791ada5158747a/library/std/src/panicking.rs:698:5
    #15 0x598c0015245f in core::panicking::panic_fmt::h5434febb0b631f9c /rustc/6ba0ce40941eee1ca02e9ba49c791ada5158747a/library/core/src/panicking.rs:75:14
    #16 0x598c00154765 in core::result::unwrap_failed::ha73b6819b4bea37b /rustc/6ba0ce40941eee1ca02e9ba49c791ada5158747a/library/core/src/result.rs:1852:5
    #17 0x598bfeabacaa in expect<calamine::xls::Xls<std::io::cursor::Cursor<bytes::bytes::Bytes>>, calamine::xls::XlsError> /root/.rustup/toolchains/nightly-x86_64-unknown-linux-gnu/lib/rustlib/src/rust/library/core/src/result.rs:1175:23
    #18 0x598bfeabacaa in _$LT$shiva..xls..Transformer$u20$as$u20$shiva..core..TransformerTrait$GT$::parse::h9156bbd187096039 /mnt/h/Security/Binary/Project/shiva/lib/src/xls.rs:18:43
    #19 0x598bfe940093 in shiva::core::Document::parse::hb8c0a146e16f31e9 /mnt/h/Security/Binary/Project/shiva/lib/src/core.rs:250:34
    #20 0x598bfe85a013 in parse_spreadsheet::_::__libfuzzer_sys_run::hddaaf46d1e0c5778 /mnt/h/Security/Binary/Project/shiva/lib/fuzz/fuzz_targets/parse_spreadsheet.rs:17:27
    #21 0x598bfe85ac88 in rust_fuzzer_test_input /root/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/libfuzzer-sys-0.4.12/src/lib.rs:276:60
    #22 0x598c000c55f5 in {closure#0} /root/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/libfuzzer-sys-0.4.12/src/lib.rs:62:9
    #23 0x598c000c55f5 in std::panicking::catch_unwind::do_call::h4d5fc7bfddb77e53 /root/.rustup/toolchains/nightly-x86_64-unknown-linux-gnu/lib/rustlib/src/rust/library/std/src/panicking.rs:590:40
    #24 0x598c000c74a8 in __rust_try libfuzzer_sys.14efa759ce49c1ba-cgu.0
    #25 0x598c000c52cd in catch_unwind<i32, libfuzzer_sys::test_input_wrap::{closure_env#0}> /root/.rustup/toolchains/nightly-x86_64-unknown-linux-gnu/lib/rustlib/src/rust/library/std/src/panicking.rs:553:19
    #26 0x598c000c52cd in catch_unwind<libfuzzer_sys::test_input_wrap::{closure_env#0}, i32> /root/.rustup/toolchains/nightly-x86_64-unknown-linux-gnu/lib/rustlib/src/rust/library/std/src/panic.rs:359:14
    #27 0x598c000c52cd in LLVMFuzzerTestOneInput /root/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/libfuzzer-sys-0.4.12/src/lib.rs:60:22
    #28 0x598c000caaa8 in fuzzer::Fuzzer::ExecuteCallback(unsigned char const*, unsigned long) /root/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/libfuzzer-sys-0.4.12/libfuzzer/FuzzerLoop.cpp:619:13
    #29 0x598c000d7569 in fuzzer::RunOneTest(fuzzer::Fuzzer*, char const*, unsigned long) /root/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/libfuzzer-sys-0.4.12/libfuzzer/FuzzerDriver.cpp:335:6
    #30 0x598c000ddcb9 in fuzzer::FuzzerDriver(int*, char***, int (*)(unsigned char const*, unsigned long)) /root/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/libfuzzer-sys-0.4.12/libfuzzer/FuzzerDriver.cpp:871:9
    #31 0x598c000f79c2 in main /root/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/libfuzzer-sys-0.4.12/libfuzzer/FuzzerMain.cpp:20:10
    #32 0x7a1a00a33ca7 in __libc_start_call_main csu/../sysdeps/nptl/libc_start_call_main.h:58:16
    #33 0x7a1a00a33d64 in __libc_start_main csu/../csu/libc-start.c:360:3
    #34 0x598bfe79d110 in _start (/mnt/h/Security/Binary/Project/shiva/lib/fuzz/target/x86_64-unknown-linux-gnu/release/parse_spreadsheet+0x150d110) (BuildId: a8dd7c5f16e6e39fd406c21defd7e81b2cfc33fb)
```

------

## Root Cause Analysis

At line 18 in `lib/src/xls.rs`, the XLS transformer attempts to open an XLS document from raw bytes using an API that returns `Result<T, E>`. When the fuzzed input is too short or malformed, the underlying library returns an error such as:

- `UnexpectedEof` / “failed to fill whole buffer”
- container parsing failures (e.g., CFB/OLE structure truncated)

Instead of propagating this error upward, the code calls `.expect(...)`, which **panics on any `Err`**, terminating the process.

This violates a fundamental robustness invariant:

> Parsing untrusted input must never panic.

A document parser should return a structured error (`Result::Err`) for invalid input, not abort the process.

------

## Why This Is a Problem

- Panics in parsing logic can be denial-of-service (DoS) vectors
- Fuzzing easily discovers these failure paths
- The crate may be used in server-side or automated processing pipelines
- Using `.expect()` / `.unwrap()` in public-facing parsing code is generally unsafe

------

## Expected Behavior

Malformed spreadsheet input (XLS/XLSX/ODS) should:

- Return `Err(ParseError)` (or the crate’s existing error type)
- Gracefully reject invalid/truncated files
- Never trigger a `panic!`

------

## Suggested Fixes

### Option A: Replace `expect()` with Proper Error Propagation

Instead of (conceptually):

```rust
let workbook = open_from_bytes(input).expect("Cannot open xls file from bytes: ...");
```

Use error propagation:

```rust
let workbook = open_from_bytes(input)
    .map_err(|e| ParseError::InvalidStructure(format!("Cannot open xls file: {e}")))?;
```

Or:

```rust
let workbook = match open_from_bytes(input) {
    Ok(wb) => wb,
    Err(e) => return Err(ParseError::InvalidStructure(format!("Cannot open xls file: {e}"))),
};
```

### Option B: Treat Truncated/Invalid Files as Normal Parse Failures

If the transformer expects a minimum length or header, validate before calling the underlying parser:

```rust
if input.len() < MIN_XLS_SIZE {
    return Err(ParseError::InvalidStructure("XLS input too short".into()));
}
```

(Still keep `Result` propagation—size checks alone won’t cover all malformed cases.)

### Option C: Add Fuzz Regression Test

Preserve the crashing input in an appropriate corpus directory, e.g.:

```
fuzz/corpus/parse_spreadsheet/
```

so future changes don’t reintroduce the panic.

------

## Minimal Defensive Patch Example

```rust
// Before
let wb = calamine_open(...).expect("Cannot open xls file from bytes");

// After
let wb = calamine_open(...)
    .map_err(|e| ParseError::InvalidSpreadsheet(format!("{e}")))?;
```

------

## Environment

- Crate: github.com/igumnoff/shiva
- Platform: Ubuntu 22.04
- Rust: rustc 1.91.0-nightly
- Fuzzing tool: cargo-fuzz
