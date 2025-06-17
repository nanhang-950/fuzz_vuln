 Out-of-Memory Crash in `ParseReadBox` due to Unchecked Slice Allocation in `mp4/senc.go`

**Describe the Bug**

During fuzzing with [`go-fuzz`](https://github.com/dvyukov/go-fuzz) on `mp4ff`, an out-of-memory (OOM) crash occurs in the `ParseReadBox` method of the `SencBox` struct, located in `mp4/senc.go`. This appears to be caused by an unvalidated memory allocation based on attacker-controlled input.

---

**To Reproduce**

Steps to reproduce the crash:

1. Clone a public corpus:
    

```bash
git clone https://github.com/strongcourage/fuzzing-corpus.git
```

2. Run the fuzzer:
    

```bash
go-fuzz -bin=./mp4-fuzz.zip -workdir=workdir
```

3. Observe crash in log:
    

```
fatal error: runtime: out of memory
...
runtime.mallocgc(0x484848480, 0x4eb040, 0x1)
...
github.com/Eyevinn/mp4ff/mp4.(*SencBox).ParseReadBox
...
```

4. Minimal reproduction in Go:
    

```go
package main

import (
    "bytes"
    "log"

    "github.com/Eyevinn/mp4ff/mp4"
)

func main() {
    input := "\x00\x00\x00Pmoof\x00\x00\x00Htraf\x00\x00\x00@" +
        "uuid\xa29ORZ\x9bO\x14\xa2DlB|d\x8d\xf4" +
        "\x000000000000000000000" +
        "00000000000000000000"

    data := []byte(input)
    r := bytes.NewReader(data)

    _, err := mp4.DecodeFile(r)
    if err != nil {
        log.Println("DecodeFile error:", err)
    } else {
        log.Println("DecodeFile completed without error.")
    }
}
```

---

**Root Cause**

In `senc.go:209`, the `ParseReadBox` method creates a slice whose length and capacity is derived from input without appropriate bounds checks:

```go
sa.Samples = make([]Sample, sa.SampleCount) // SampleCount from file input
```

When `SampleCount` is large or malformed, it leads to allocating >10GB of memory, eventually causing OOM.

---

**Expected Behavior**

`ParseReadBox` should validate `SampleCount` and other size-related fields against reasonable upper bounds (e.g., max sample limit or input stream length) before allocating large slices.

---

**Environment**

- Module: `github.com/Eyevinn/mp4ff v0.48.0`
    
- Platform: Docker container running `go-fuzz`
    
- Go version: go1.20+
    
- Fuzzer: `go-fuzz`
    

