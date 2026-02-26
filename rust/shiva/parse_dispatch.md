# Panic in `xml::Transformer::parse()` Due to `Option::unwrap()` on Malformed XML Input (via `parse_dispatch`)

## Bug Description

While fuzzing the `shiva` project using `cargo-fuzz`, a runtime panic was triggered at:

```
lib/src/xml.rs:113:35
```

The crash occurs inside:

```
shiva::xml::Transformer::parse()
```

where an `Option::unwrap()` is called on a `None` value, causing an immediate panic:

```
called `Option::unwrap()` on a `None` value
```

This results in a libFuzzer abort (`deadly signal`) and terminates the process.

------

## Steps to Reproduce

### Install cargo-fuzz

```bash
cargo install cargo-fuzz
```

### Crashing Input (Hex)

```
00000000: 0600
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

    const PARSE_TYPES: [DocumentType; 7] = [
        DocumentType::Markdown,
        DocumentType::HTML,
        DocumentType::Text,
        DocumentType::Json,
        DocumentType::CSV,
        DocumentType::RTF,
        DocumentType::XML,
    ];

    let parse_ty = PARSE_TYPES[(data[0] as usize) % PARSE_TYPES.len()];
    let input = data[1..].to_vec().into();
    let _ = Document::parse(&input, parse_ty);
});
```

### Run the Fuzz Target

```bash
RUST_BACKTRACE=1 cargo fuzz run parse_dispatch
```

### Reproduce with Saved Artifact

```bash
cargo fuzz run parse_dispatch artifacts/parse_dispatch/crash-b937066032e6f3d44184b8b9317cff27f94d5f08
```

------

## Observed Panic

```
thread '<unnamed>' (11687) panicked at /mnt/h/Security/Binary/Project/shiva/lib/src/xml.rs:113:35:
called `Option::unwrap()` on a `None` value
note: run with `RUST_BACKTRACE=1` environment variable to display a backtrace
==11687== ERROR: libFuzzer: deadly signal
```

Stack trace confirms the failure path:

- `core::option::unwrap_failed`
- `unwrap<&shiva::xml::Node>`
- `shiva::xml::Transformer::parse` at `lib/src/xml.rs:113:35`
- `shiva::core::Document::parse` at `lib/src/core.rs:246:34`
- fuzz target: `fuzz_targets/parse_dispatch.rs:23:13`

------

## Root Cause Analysis

`Transformer::parse()` assumes certain XML structural invariants and performs:

```rust
some_option.unwrap()
```

When the fuzzer provides malformed or structurally unexpected XML (e.g., missing required nodes / unexpected nesting / lookup failure), the internal lookup returns `None`. The subsequent `.unwrap()` causes a panic instead of a recoverable parsing error.

This violates a key robustness invariant:

> Parsing untrusted input must never panic.

------

## Why This Is a Problem

- Panics in parsing code are denial-of-service (DoS) vectors
- Malformed input can trivially crash applications that parse untrusted XML
- Fuzzing readily discovers these paths, indicating real-world exploitability for availability impacts
- Parser APIs should return structured errors (`Result::Err`) instead of aborting

------

## Expected Behavior

Malformed XML input should:

- Return `Err(ParseError)` (or equivalent error type)
- Gracefully reject invalid structures
- Never trigger `panic!`

------

## Suggested Fixes

### Option A: Replace `unwrap()` With Proper Error Propagation

Replace:

```rust
let node = maybe_node.unwrap();
```

With:

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

### Option B: Add Structural Validation Before Transformation

If `Transformer::parse()` depends on invariants, validate them explicitly before dereferencing:

```rust
if maybe_node.is_none() {
    return Err(ParseError::MissingNode);
}
```

### Option C: Add Fuzz Regression Test

Preserve the crashing input under:

```
fuzz/corpus/parse_dispatch/
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

- Target/Project: `shiva` (panic location: `lib/src/xml.rs`)
- Platform: Ubuntu 22.04
- Rustc: 1.91.0 (nightly per backtrace context)
- Fuzzing: `cargo-fuzz` / libFuzzer
