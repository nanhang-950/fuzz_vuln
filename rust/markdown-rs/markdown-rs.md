### unreachable!() Panic in Setext Heading Parsing Due to Assumed Stack State (Denial of Service)

**Bug Description**

In `markdown-rs`, the Setext-style heading parsing path contains a function `on_exit_heading_setext_underline_sequence` that assumes a `heading` element exists on the parser’s state stack and uses `unreachable!()` as an assertion. A fuzzer can craft input that contains only a Setext underline line (e.g., `===` / `---`) without a corresponding heading text. This causes the parser to reach the branch without a `heading` on the stack, triggering the `unreachable!()` macro, causing a panic and crash, which results in a denial of service (DoS).

Example trigger input:

```markdown
t
=
=
\=
=
```

------

**Steps to Reproduce**

1. Create a fuzz target:

```rust
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

```bash
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
