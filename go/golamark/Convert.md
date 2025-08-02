### Panic in Segment.Value() due to Invalid Slice Bounds from Malformed Fenced Code Block

**Bug Description**

While fuzzing the [Goldmark](https://github.com/yuin/goldmark) Markdown renderer, a runtime panic was triggered in the `Segment.Value` method located at `segment.go:60`. This method performs slicing on a source byte array using a segment's `Start` and `Stop` positions:

```
return source[s.Start:s.Stop]
```

However, when `s.Stop < s.Start` (i.e., invalid range), this leads to a `slice bounds out of range` panic. This condition can occur when Goldmark attempts to render malformed fenced code blocks that are nested inside list items with incorrect indentation.

The crash is reproducible using the following fuzzed input:

```go
[]byte("*\n\t ~~~\n\t0")
```

This causes the renderer to attempt slicing `source[11:10]`, which panics at runtime.

------

**Steps to Reproduce**

1. Create a fuzzer:

```go
package fuzz

import (
        "bytes"
        "testing"

        "github.com/yuin/goldmark"
)

func FuzzGoldmarkRender(f *testing.F) {
        md := goldmark.New()

    	f.Add([]byte("# Hello\nThis is *markdown*."))
  
    	f.Add([]byte("```go\nfmt.Println(\"Hello\")\n```"))

        f.Fuzz(func(t *testing.T, data []byte) {
                var buf bytes.Buffer
                err := md.Convert(data, &buf)
                if err != nil {
                        t.Logf("Error: %v", err)
                        return
                }
        })
```

2. Run with:

```bash
go test -fuzz=FuzzGoldmarkRender
```

3. Crash observed:

```
fuzz: elapsed: 0s, gathering baseline coverage: 0/2385 completed
failure while testing seed corpus entry: FuzzGoldmarkRender/e3a25fbeed14d138
fuzz: elapsed: 0s, gathering baseline coverage: 2/2385 completed
--- FAIL: FuzzGoldmarkRender (0.27s)
    --- FAIL: FuzzGoldmarkRender (0.00s)
        testing.go:1591: panic: runtime error: slice bounds out of range [11:10]
            goroutine 29 [running]:
            runtime/debug.Stack()
                /usr/local/go/src/runtime/debug/stack.go:26 +0x9b
            testing.tRunner.func1()
                /usr/local/go/src/testing/testing.go:1591 +0x1c8
            panic({0x6b4020?, 0xc0001420d8?})
                /usr/local/go/src/runtime/panic.go:791 +0x132
            github.com/yuin/goldmark/text.(*Segment).Value(0xc0000ed3c0, {0xc00009f1d0, 0xa?, 0x10})
                /root/.local/go/pkg/mod/github.com/yuin/goldmark@v1.7.12/text/segment.go:60 +0x452
            github.com/yuin/goldmark/renderer/html.(*Renderer).writeLines(0xc0000a7290, {0x71fa60, 0xc0000b3580}, {0xc00009f1d0, 0xa, 0x10}, {0x722060, 0xc0000bc2c0})
                /root/.local/go/pkg/mod/github.com/yuin/goldmark@v1.7.12/renderer/html/html.go:293 +0x12a
            github.com/yuin/goldmark/renderer/html.(*Renderer).renderFencedCodeBlock(0xc0000a7290, {0x71fa60, 0xc0000b3580}, {0xc00009f1d0, 0xa, 0x10}, {0x722060?, 0xc0000bc2c0}, 0xa4?)
                /root/.local/go/pkg/mod/github.com/yuin/goldmark@v1.7.12/renderer/html/html.go:368 +0x20f
            github.com/yuin/goldmark/renderer.(*renderer).Render.func2({0x722060, 0xc0000bc2c0}, 0x1)
                /root/.local/go/pkg/mod/github.com/yuin/goldmark@v1.7.12/renderer/renderer.go:166 +0x111
            github.com/yuin/goldmark/ast.walkHelper({0x722060, 0xc0000bc2c0}, 0xc0000ed720)
                /root/.local/go/pkg/mod/github.com/yuin/goldmark@v1.7.12/ast/ast.go:505 +0x57
            github.com/yuin/goldmark/ast.walkHelper({0x721640, 0xc0000a3180}, 0xc0000ed720)
                /root/.local/go/pkg/mod/github.com/yuin/goldmark@v1.7.12/ast/ast.go:511 +0x29e
            github.com/yuin/goldmark/ast.walkHelper({0x7219a0, 0xc0000a30e0}, 0xc0000ed720)
                /root/.local/go/pkg/mod/github.com/yuin/goldmark@v1.7.12/ast/ast.go:511 +0x29e
            github.com/yuin/goldmark/ast.walkHelper({0x721400, 0xc0000a3040}, 0xc0000ed720)
                /root/.local/go/pkg/mod/github.com/yuin/goldmark@v1.7.12/ast/ast.go:511 +0x29e
            github.com/yuin/goldmark/ast.Walk(...)
                /root/.local/go/pkg/mod/github.com/yuin/goldmark@v1.7.12/ast/ast.go:500
            github.com/yuin/goldmark/renderer.(*renderer).Render(0xc0000c84b0, {0x71d540, 0xc0000a7bf0}, {0xc00009f1d0, 0xa, 0x10}, {0x721400, 0xc0000a3040})
                /root/.local/go/pkg/mod/github.com/yuin/goldmark@v1.7.12/renderer/renderer.go:161 +0x374
            github.com/yuin/goldmark.(*markdown).Convert(0xc0000b2f40, {0xc00009f1d0, 0xa, 0x10}, {0x71d540, 0xc0000a7bf0}, {0x0, 0x0, 0x0})
                /root/.local/go/pkg/mod/github.com/yuin/goldmark@v1.7.12/markdown.go:118 +0xe7
            fuzzing_goldmark_test.FuzzGoldmarkRender.func1(0x0?, {0xc00009f1d0, 0xa, 0x10})
                /root/project/goldmark/goldmark_test.go:19 +0x7d
            reflect.Value.call({0x687820?, 0xc0000b7170?, 0x13?}, {0x6cb425, 0x4}, {0xc0000a7ad0, 0x2, 0x2?})
                /usr/local/go/src/reflect/value.go:584 +0xca6
            reflect.Value.Call({0x687820?, 0xc0000b7170?, 0x544d8d?}, {0xc0000a7ad0?, 0x6ca960?, 0xf?})
                /usr/local/go/src/reflect/value.go:368 +0xb9
            testing.(*F).Fuzz.func1.1(0xc0000d49c0?)
                /usr/local/go/src/testing/fuzz.go:335 +0x305
            testing.tRunner(0xc0000d49c0, 0xc00017e2d0)
                /usr/local/go/src/testing/testing.go:1690 +0xf4
            created by testing.(*F).Fuzz.func1 in goroutine 19
                /usr/local/go/src/testing/fuzz.go:322 +0x577


FAIL
exit status 1
FAIL    fuzzing_goldmark        0.290s
```

------

**Minimal Reproduction**

```go
package main

import (
    "bytes"
    "fmt"

    "github.com/yuin/goldmark"
)

func main() {
    data := []byte("*\n\t ~~~\n\t0")
    md := goldmark.New()

    var buf bytes.Buffer
    err := md.Convert(data, &buf)
    if err != nil {
        fmt.Println("Error:", err)
        return
    }

    fmt.Println("Output:", buf.String())
}
```

------

**Root Cause**

When parsing malformed Markdown constructs (e.g., code blocks within improperly indented list items), the AST builder can create a `Segment` with an invalid `Start` and `Stop` range (e.g., `Start > Stop`). The renderer does not validate this before performing slicing, which leads to a panic:

```go
func (s Segment) Value(source []byte) []byte {
    return source[s.Start:s.Stop] // panics if Start > Stop
}
```

------

**Expected Behavior**

The `Segment.Value()` method should validate `Start` and `Stop` before slicing. If `s.Stop < s.Start` or the bounds exceed the length of the source, it should return `nil` or panic safely with a clear message.

**Suggested Fixes**:

- Add bounds checks to `Segment.Value()`:

```go
if s.Start < 0 || s.Stop > len(source) || s.Start > s.Stop {
    // Optionally: log or return a sentinel value for diagnostics
    return nil
}
return source[s.Start:s.Stop]
```

- Alternatively, sanitize `Segment` ranges during AST node construction.
- Add fuzzing-based regression tests to prevent recurrence.

------

**Environment**

- Module: `github.com/yuin/goldmark v1.7.12`
- Platform: Ubuntu 22.04 (Docker)
- Go version: `go1.23.7`
