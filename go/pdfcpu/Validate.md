### Out-of-Bounds Panic in `model.skipStringLit` due to Missing Bounds Check

**Bug Description**

While fuzzing `pdfcpu`, a runtime panic was observed in the `skipStringLit` function located at `parse.go:1273`. This function attempts to skip over PDF string literals, but fails to properly check the result of `bytes.IndexByte` when searching for the closing parenthesis `)`.

If the closing parenthesis is missing from the input (i.e., malformed string), `IndexByte` returns `-1`, and this value is then used directly in a slice expression, causing an out-of-bounds panic:

```go
end := bytes.IndexByte(b[pos:], ')') 
str := b[pos : pos+end] // end == -1 leads to panic
```

------

**Steps to Reproduce**

1. Create a fuzzer:

```go
package fuzz

import (
    "bytes"
    "os"
    "testing"

    "github.com/pdfcpu/pdfcpu/pkg/api"
    "github.com/pdfcpu/pdfcpu/pkg/pdfcpu/model"
)

func FuzzPDF(f *testing.F) {
    if data, err := os.ReadFile("/root/corpus/pdf/pdf.pdf"); err == nil {
        f.Add(data)
    }

    f.Fuzz(func(t *testing.T, data []byte) {
        r := bytes.NewReader(data)
        conf := model.NewDefaultConfiguration()
        _ = api.Validate(r, conf)
    })
}
```

2. Run with:

```bash
go test -fuzz=FuzzPDF
```

3. Crash observed:

```
panic: runtime error: slice bounds out of range [-1:]

github.com/pdfcpu/pdfcpu/pkg/pdfcpu/model.skipStringLit
	.../model/parse.go:1273 +0x2c6
...
```

------

**Minimal Reproduction**

```go
package main

import (
    "bytes"
    "fmt"

    "github.com/pdfcpu/pdfcpu/pkg/api"
    "github.com/pdfcpu/pdfcpu/pkg/pdfcpu/model"
)

func main() {
    data := []byte("%PDF-1.0stream\n0000")
    r := bytes.NewReader(data)
    conf := model.NewDefaultConfiguration()
    if err := api.Validate(r, conf); err != nil {
        fmt.Printf("Validation error: %v\n", err)
    } else {
        fmt.Println("Validation succeeded.")
    }
}
```

------

**Root Cause**

The `skipStringLit` function does not check if the closing parenthesis `)` exists in the string literal. When the input is malformed (e.g., the string is not properly closed), `bytes.IndexByte` returns `-1`, but this is not checked before using it in slicing operations, leading to a panic:

```go
end := bytes.IndexByte(b[pos:], ')')
if end == -1 {
    // should return error here
}
str := b[pos : pos+end] // causes panic when end == -1
```

------

**Expected Behavior**

The parser should validate whether `end` is `-1` before using it for slicing. If the delimiter is missing, the function should return a descriptive parsing error (e.g., `ErrInvalidPDF`), rather than panicking.

**Suggested Fixes**:

- Update `skipStringLit` and similar functions to return `(int, error)` and handle invalid cases gracefully.
- Add fuzzing regression tests for malformed string literals.
- Improve overall parser resilience for malformed or malicious PDF input.

------

**Environment**

- Module: `github.com/pdfcpu/pdfcpu v0.11.0`
- Platform: Ubuntu 22.04 (Docker)
- Go version: `go1.23.7`
