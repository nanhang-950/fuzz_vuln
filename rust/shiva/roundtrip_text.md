# Panic in `xml::Transformer::parse()` due to `Option::unwrap()` on malformed XML input

## Bug Description

While fuzzing the `shiva` crate (`github.com/igumnoff/shiva`) using `cargo-fuzz`, a runtime panic was triggered in:

```
lib/src/xml.rs:113:35
```

The crash is caused by calling:

```rust
Option::unwrap()
```

on a `None` value inside:

```
shiva::xml::Transformer::parse()
```

This leads to an immediate abort under libFuzzer:

```
called `Option::unwrap()` on a `None` value
```

Stack trace excerpt:

```
#18 unwrap<&shiva::xml::Node>
#19 shiva::xml::Transformer::parse
    /lib/src/xml.rs:113:35
#20 shiva::core::Document::parse
    /lib/src/core.rs:246:34
#21 roundtrip_text fuzz target
    /lib/fuzz/fuzz_targets/roundtrip_text.rs:25:27
```

The panic is reproducible via fuzzing malformed XML input.

------

## Steps to Reproduce

### Install cargo-fuzz

```bash
cargo install cargo-fuzz
```

### Run the existing fuzz target

From `lib/`:

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

Inside `xml.rs` at line 113, the code performs:

```rust
some_option.unwrap()
```

The fuzzed input produces an invalid or incomplete XML structure where:

- A required node is missing
- A parent/child relationship assumption fails
- A lookup returns `None`

Because `.unwrap()` is used instead of proper error handling, malformed input causes a panic instead of returning an error.

This violates a fundamental robustness invariant:

> Parsing untrusted input must never panic.

Given this is a document parser, all malformed input should result in a structured error (`Result::Err`) rather than process termination.

------

## Why This Is a Problem

- Panics in parsing logic are denial-of-service vectors
- Fuzzing easily discovers these paths
- The crate may be used in server-side or automated processing pipelines
- `unwrap()` in parser logic is generally unsafe for public-facing APIs

------

## Expected Behavior

Malformed XML input should:

- Return `Err(ParseError)`
- Or gracefully reject invalid structure
- Never trigger `panic!`

------

## Suggested Fixes

### Option A: Replace `unwrap()` with proper error propagation

Instead of:

```rust
let node = maybe_node.unwrap();
```

Use:

```rust
let node = maybe_node.ok_or(ParseError::InvalidStructure)?;
```

or:

```rust
let node = match maybe_node {
    Some(n) => n,
    None => return Err(ParseError::InvalidStructure),
};
```

------

### Option B: Add structural validation before transformation

If `Transformer::parse()` assumes invariants, validate them explicitly:

```rust
if maybe_node.is_none() {
    return Err(ParseError::MissingNode);
}
```

------

### Option C: Add fuzz regression test

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
- Fuzzing: cargo-fuzz

```
00000000: 0574 0a                                  .t.
```

```
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

