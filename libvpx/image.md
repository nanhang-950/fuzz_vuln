## project address

project：https://chromium.googlesource.com/webm/libvpx

version：v1.16.0

## info

OS：Ubuntu22.04 TLS

Build: 

```shell
git clone https://chromium.googlesource.com/webm/libvpx
cd libvpx

mkdir build && cd build

../configure \
    --disable-unit-tests \
    --size-limit=12288x12288 \
    --extra-cflags="-fsanitize=fuzzer-no-link -DVPX_MAX_ALLOCABLE_MEMORY=1073741824" \
    --disable-webm-io \
    --enable-debug \
    --enable-vp8-encoder \
    --enable-vp9-encoder

make -j$(nproc)
```

## fuzzer

**目标**: 图像格式转换和处理

**测试内容**:

- 各种像素格式 (I420, I422, I444, NV12, YV12)
- 高位深度格式 (10-bit, 12-bit)
- 色彩空间转换 (BT.601, BT.709, BT.2020)
- 图像分配和包装
- 显示矩形设置
- 图像翻转操作

**输入格式**: 

- 前 32 字节: 配置头
- 剩余数据: YUV 像素数据

## Poc

https://github.com/nanhang-950/fuzz_vuln/blob/main/libvpx/fuzzers/crash-883ef121deb9e622bd4b1bfc5a7b634047492381

## ASAN Info

```shell
❯ ./vpx_image_fuzzer ./crash-883ef121deb9e622bd4b1bfc5a7b634047492381
INFO: Running with entropic power schedule (0xFF, 100).
INFO: Seed: 728820164
INFO: Loaded 1 modules   (215 inline 8-bit counters): 215 [0x651e408a5768, 0x651e408a583f),
INFO: Loaded 1 PC tables (215 PCs): 215 [0x651e408a5840,0x651e408a65b0),
INFO: -max_len is not provided; libFuzzer will not generate inputs larger than 4096 bytes
INFO: A corpus is not provided, starting from an empty corpus
#2      INITED cov: 2 ft: 2 corp: 1/1b exec/s: 0 rss: 31Mb
AddressSanitizer:DEADLYSIGNAL
=================================================================
==2587==ERROR: AddressSanitizer: SEGV on unknown address 0x7968ce3aa810 (pc 0x651e4085c74a bp 0x7ffc4b9a91c0 sp 0x7ffc4b9a9040 T0)
==2587==The signal is caused by a READ memory access.
    #0 0x651e4085c74a in LLVMFuzzerTestOneInput /mnt/h/Security/Binary/Reports/fuzz_vuln/libvpx/vpx_image_fuzzer.c:204:46
    #1 0x651e4075f58a in fuzzer::Fuzzer::ExecuteCallback(unsigned char const*, unsigned long) (/mnt/h/Security/Binary/Reports/fuzz_vuln/libvpx/fuzzers_bin/vpx_image_fuzzer+0x5058a) (BuildId: 59ffdb81c6935e766b4bb852f8d5a624ffd37a11)
    #2 0x651e4075eb99 in fuzzer::Fuzzer::RunOne(unsigned char const*, unsigned long, bool, fuzzer::InputInfo*, bool, bool*) (/mnt/h/Security/Binary/Reports/fuzz_vuln/libvpx/fuzzers_bin/vpx_image_fuzzer+0x4fb99) (BuildId: 59ffdb81c6935e766b4bb852f8d5a624ffd37a11)
    #3 0x651e40760665 in fuzzer::Fuzzer::MutateAndTestOne() (/mnt/h/Security/Binary/Reports/fuzz_vuln/libvpx/fuzzers_bin/vpx_image_fuzzer+0x51665) (BuildId: 59ffdb81c6935e766b4bb852f8d5a624ffd37a11)
    #4 0x651e40761155 in fuzzer::Fuzzer::Loop(std::vector<fuzzer::SizedFile, std::allocator<fuzzer::SizedFile>>&) (/mnt/h/Security/Binary/Reports/fuzz_vuln/libvpx/fuzzers_bin/vpx_image_fuzzer+0x52155) (BuildId: 59ffdb81c6935e766b4bb852f8d5a624ffd37a11)
    #5 0x651e4074d715 in fuzzer::FuzzerDriver(int*, char***, int (*)(unsigned char const*, unsigned long)) (/mnt/h/Security/Binary/Reports/fuzz_vuln/libvpx/fuzzers_bin/vpx_image_fuzzer+0x3e715) (BuildId: 59ffdb81c6935e766b4bb852f8d5a624ffd37a11)
    #6 0x651e40779a76 in main (/mnt/h/Security/Binary/Reports/fuzz_vuln/libvpx/fuzzers_bin/vpx_image_fuzzer+0x6aa76) (BuildId: 59ffdb81c6935e766b4bb852f8d5a624ffd37a11)
    #7 0x7967d7b16ca7 in __libc_start_call_main csu/../sysdeps/nptl/libc_start_call_main.h:58:16
    #8 0x7967d7b16d64 in __libc_start_main csu/../csu/libc-start.c:360:3
    #9 0x651e40741a70 in _start (/mnt/h/Security/Binary/Reports/fuzz_vuln/libvpx/fuzzers_bin/vpx_image_fuzzer+0x32a70) (BuildId: 59ffdb81c6935e766b4bb852f8d5a624ffd37a11)

==2587==Register values:
rax = 0x0000000000000000  rbx = 0x00007ffc4b9a9040  rcx = 0x0000000000000021  rdx = 0x0000000000000fd8
rdi = 0x0000000000000040  rsi = 0x0000000200000000  rbp = 0x00007ffc4b9a91c0  rsp = 0x00007ffc4b9a9040
 r8 = 0x0000651e408a6c00   r9 = 0x0000000000000000  r10 = 0x00007967ca3ab810  r11 = 0x0000000000002000
r12 = 0x000050d000000070  r13 = 0x0000000000000001  r14 = 0x0000000000000fff  r15 = 0x00007968ce3aa810
AddressSanitizer can not provide additional info.
SUMMARY: AddressSanitizer: SEGV /mnt/h/Security/Binary/Reports/fuzz_vuln/libvpx/vpx_image_fuzzer.c:204:46 in LLVMFuzzerTestOneInput
==2587==ABORTING
MS: 5 ChangeByte-ShuffleBytes-CrossOver-InsertRepeatedBytes-InsertRepeatedBytes-; base unit: adc83b19e793491b1c6ea0fd8b46cd9f32e592fc
0xf2,0xf2,0xf2,0xf2,0xf2,0xf2,0xf2,0xf2,0xf2,0xf2,0xf2,0xf2,0xf2,0xf2,0x6f,0x6f,0x6f,0x6f,0x6f,0x6f,0x6f,0x6f,0x6f,0x6f,0x6f,0x6f,0x6f,0x6f,0x6f,0xf2,0xf2,0xff,0xa,
\362\362\362\362\362\362\362\362\362\362\362\362\362\362ooooooooooooooo\362\362\377\012
artifact_prefix='./'; Test unit written to ./crash-883ef121deb9e622bd4b1bfc5a7b634047492381
Base64: 8vLy8vLy8vLy8vLy8vJvb29vb29vb29vb29vb2/y8v8K
```

