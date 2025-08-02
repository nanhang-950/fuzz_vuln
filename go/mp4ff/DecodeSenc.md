### Bug Report: Slice Bounds Out of Range Panic in `mp4.DecodeSenc`

**Describe the Bug**

When fuzzing the `github.com/Eyevinn/mp4ff` library, an out-of-bounds panic occurs inside the `DecodeSenc` function due to accessing a slice beyond its capacity. Specifically, the code tries to access the first 4 bytes of a slice that is empty or shorter than 4 bytes, causing a runtime panic.

**The error message:**

```
panic: runtime error: slice bounds out of range [:4] with capacity 0

goroutine 1 [running]:
github.com/Eyevinn/mp4ff/mp4.DecodeSenc({{0xc0000a8050?, 0xc0000a60f0?}, 0xc0000a8050?, 0x4?}, 0x0, {0x524db8?, 0xc0000a6180?})
        /root/.local/go/pkg/mod/github.com/!eyevinn/mp4ff@v0.49.0/mp4/senc.go:120 +0x40a
github.com/Eyevinn/mp4ff/mp4.DecodeBox(0x0, {0x524db8, 0xc0000a6180})
        /root/.local/go/pkg/mod/github.com/!eyevinn/mp4ff@v0.49.0/mp4/box.go:321 +0xb2
github.com/Eyevinn/mp4ff/mp4.DecodeFile({0x524db8, 0xc0000a6180}, {0x0, 0x0, 0xc0000061c0?})
        /root/.local/go/pkg/mod/github.com/!eyevinn/mp4ff@v0.49.0/mp4/file.go:183 +0x272
main.main()
        /root/mp4ff/main.go:16 +0x9f
exit status 2
```

------

**To Reproduce**

1. Clone the fuzzing corpus:

```bash
git clone https://github.com/strongcourage/fuzzing-corpus.git
```

1. Run the fuzzer:

```bash
go-fuzz -bin=./mp4-fuzz.zip -workdir=workdir
```

1. Observe the crash in the logs as above.
2. Minimal reproducible Go code:

```go
package main

import (
    "bytes"
    "log"

    "github.com/Eyevinn/mp4ff/mp4"
)

func main() {
    input := "\x00\x00\x00\x01senc\x00\x00\x00\x00\x00\x00\x00\x10"

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

In the `DecodeSenc` function (located in `mp4/senc.go`), there is an attempt to read 4 bytes from a data slice without checking whether the slice has at least 4 bytes, leading to an out-of-bounds panic.

Suggested fix:

```go
if len(data) < 4 {
    return fmt.Errorf("DecodeSenc: data too short to read required bytes")
}
```

------

**Expected Behavior**

The function should validate input data length before slicing to prevent out-of-bounds access, and return a meaningful error instead of crashing.

------

**Environment**

- Module: `github.com/Eyevinn/mp4ff v0.48.0`
- Platform: Docker container running `go-fuzz`
- Go version: go1.20+
- Fuzzer: `go-fuzz`
