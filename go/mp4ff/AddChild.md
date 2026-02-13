### Bug Report: Nil Pointer Dereference in `mp4.File.AddChild` Due to Missing `MediaSegment` Check

**Describe the Bug**

During fuzz testing of the `github.com/Eyevinn/mp4ff` library, a runtime panic caused by nil pointer dereference occurs inside the `mp4.File.AddChild` method. This happens because the `MediaSegment` field of the `File` struct is `nil`, but the code attempts to call `LastFragment()` on it without checking for nil, resulting in a segmentation fault.

The crash log:

```
panic: runtime error: invalid memory address or nil pointer dereference
[signal SIGSEGV: segmentation violation code=0x1 addr=0x10 pc=0x4ac2f1]

goroutine 1 [running]:
github.com/Eyevinn/mp4ff/mp4.(*File).AddChild(0xc00011a000, {0x526100, 0xc00011e2a0}, 0x0)
        /root/.local/go/pkg/mod/github.com/!eyevinn/mp4ff@v0.49.0/mp4/file.go:262 +0x8b1
github.com/Eyevinn/mp4ff/mp4.DecodeFile({0x524dd8, 0xc000102180}, {0x0, 0x0, 0xc0000061c0?})
        /root/.local/go/pkg/mod/github.com/!eyevinn/mp4ff@v0.49.0/mp4/file.go:234 +0x1e5
main.main()
        /root/mp4ff/main.go:18 +0x9f
exit status 2
```

------

**To Reproduce**

1. Clone the fuzzing corpus repository:

```bash
git clone https://github.com/strongcourage/fuzzing-corpus.git
```

1. Run the fuzzer:

```bash
go-fuzz -bin=./mp4-fuzz.zip -workdir=workdir
```

1. Observe the panic in the logs.
2. Minimal reproduction example:

```go
package main

import (
    "bytes"
    "log"

    "github.com/Eyevinn/mp4ff/mp4"
)

func main() {
    input := "\x00\x00\x008moov\x00\x00\x00(moof\x00\x00\x00\b" +
        "moof\x00\x00\x00\bmoof\x00\x00\x00\bmoof" +
        "\x00\x00\x00\bmoof\x00\x00\x00\bmoof\xeb"

    data := []byte(input)
    r := bytes.NewReader(data)

    _, err := mp4.DecodeFile(r)
    if err != nil {
        log.Println("DecodeFile error:", err)
    }
}
```

------

**Root Cause**

In `mp4/file.go`, the `File.AddChild` method calls `file.MediaSegment.LastFragment()` without checking whether `MediaSegment` is `nil`. When it is `nil`, calling `LastFragment()` results in a nil pointer dereference and panic.

Suggested fix snippet:

```go
ms := file.MediaSegment
if ms == nil {
    return fmt.Errorf("MediaSegment is nil")
}
lastFrag := ms.LastFragment()
```

------

**Expected Behavior**

The code should properly check for nil `MediaSegment` before calling its methods to avoid panics caused by malformed input. Instead of crashing, it should return a meaningful error.

------

**Environment**

- Module: `github.com/Eyevinn/mp4ff v0.50.0`
- Platform: Docker container running `go-fuzz`
- Go version: go1.20+
- Fuzzer: `go-fuzz`
