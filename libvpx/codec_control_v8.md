| 漏洞类型        | 奖金范围      |
| --------------- | ------------- |
| 普通崩溃        | 0（无利用性） |
| 可控堆溢出      | 几百~几千美元 |
| 可利用 RCE      | $5000+        |
| Chrome 链式利用 | $10k-$30k+    |

**目标**: 编解码器控制接口

**测试内容**:

- VP8/VP9 编码器控制参数
- VP8/VP9 解码器控制参数
- 无效控制 ID 处理
- 参数类型不匹配
- 极端参数值
- 控制序列组合

**变体**:

- `vpx_codec_control_fuzzer_vp8` - VP8 专用
- `vpx_codec_control_fuzzer_vp9` - VP9 专用

```shell
git clone https://chromium.googlesource.com/webm/libvpx
cd libvpx

# 创建构建目录
mkdir build && cd build

# 配置 (启用 fuzzing 支持)
../configure \
    --disable-unit-tests \
    --size-limit=12288x12288 \
    --extra-cflags="-fsanitize=fuzzer-no-link -DVPX_MAX_ALLOCABLE_MEMORY=1073741824" \
    --disable-webm-io \
    --enable-debug \
    --enable-vp8-encoder \
    --enable-vp9-encoder

# 编译
make -j$(nproc)

clang -fsanitize=fuzzer,address,undefined \
      -I./libvpx -I./libvpx/build \
      vpx_image_fuzzer.c \
      -o vpx_image_fuzzer \
      ./libvpx/build/libvpx.a -lpthread -lm
```



```
❯ ./vpx_codec_control_fuzzer_vp8
INFO: Running with entropic power schedule (0xFF, 100).
INFO: Seed: 3724115580
INFO: Loaded 1 modules   (105 inline 8-bit counters): 105 [0x562f6ee534c0, 0x562f6ee53529),
INFO: Loaded 1 PC tables (105 PCs): 105 [0x562f6ee53530,0x562f6ee53bc0),
INFO: -max_len is not provided; libFuzzer will not generate inputs larger than 4096 bytes
INFO: A corpus is not provided, starting from an empty corpus
#2      INITED cov: 2 ft: 2 corp: 1/1b exec/s: 0 rss: 31Mb
#7711   NEW    cov: 12 ft: 12 corp: 2/79b lim: 80 exec/s: 0 rss: 37Mb L: 78/78 MS: 4 InsertRepeatedBytes-CopyPart-ChangeByte-InsertRepeatedBytes-
#7722   NEW    cov: 17 ft: 17 corp: 3/159b lim: 80 exec/s: 0 rss: 69Mb L: 80/80 MS: 1 CopyPart-
#7730   NEW    cov: 18 ft: 18 corp: 4/239b lim: 80 exec/s: 0 rss: 89Mb L: 80/80 MS: 3 InsertByte-ChangeBinInt-InsertByte-
#7734   NEW    cov: 20 ft: 20 corp: 5/319b lim: 80 exec/s: 0 rss: 100Mb L: 80/80 MS: 4 CMP-CrossOver-CopyPart-CMP- DE: "\377\377"-"\001\000"-
#7781   NEW    cov: 21 ft: 21 corp: 6/399b lim: 80 exec/s: 0 rss: 184Mb L: 80/80 MS: 2 CMP-ChangeBinInt- DE: "\000\000\000\000\000\000\000L"-
#7796   REDUCE cov: 21 ft: 21 corp: 6/396b lim: 80 exec/s: 0 rss: 216Mb L: 77/80 MS: 5 EraseBytes-ChangeBit-ChangeBinInt-InsertByte-CopyPart-
#7899   REDUCE cov: 21 ft: 21 corp: 6/392b lim: 80 exec/s: 0 rss: 223Mb L: 76/80 MS: 3 CopyPart-ChangeBit-EraseBytes-
#7972   REDUCE cov: 21 ft: 21 corp: 6/391b lim: 80 exec/s: 0 rss: 224Mb L: 77/80 MS: 3 InsertByte-ChangeBinInt-EraseBytes-
#8007   REDUCE cov: 22 ft: 22 corp: 7/468b lim: 80 exec/s: 0 rss: 224Mb L: 77/80 MS: 5 CrossOver-ChangeBit-ChangeBit-ChangeASCIIInt-CMP- DE: "\377\377\377\377\377\377\377K"-
#8176   REDUCE cov: 22 ft: 22 corp: 7/466b lim: 80 exec/s: 0 rss: 224Mb L: 78/80 MS: 4 ShuffleBytes-ChangeBit-CrossOver-CopyPart-
AddressSanitizer:DEADLYSIGNAL
=================================================================
==2652==ERROR: AddressSanitizer: SEGV on unknown address 0x00004c000000 (pc 0x562f6ed2eef4 bp 0x7fff8d1b8cf0 sp 0x7fff8d1b8b58 T0)
==2652==The signal is caused by a READ memory access.
    #0 0x562f6ed2eef4 in vp8_set_postproc /mnt/h/Security/Binary/Reports/fuzz_vuln/libvpx/libvpx/build/../vp8/vp8_dx_iface.c:630:23
    #1 0x562f6ed0ed2f in vpx_codec_control_ /mnt/h/Security/Binary/Reports/fuzz_vuln/libvpx/libvpx/build/../vpx/src/vpx_codec.c:106:15
    #2 0x562f6ed0d048 in LLVMFuzzerTestOneInput /mnt/h/Security/Binary/Reports/fuzz_vuln/libvpx/vpx_codec_control_fuzzer.c
    #3 0x562f6ec10f3a in fuzzer::Fuzzer::ExecuteCallback(unsigned char const*, unsigned long) (/mnt/h/Security/Binary/Reports/fuzz_vuln/libvpx/fuzzers_bin/vpx_codec_control_fuzzer_vp8+0x51f3a) (BuildId: 84db5f9c0fcf8e35e6dd79c60ea9a0941d9f1de7)
    #4 0x562f6ec10549 in fuzzer::Fuzzer::RunOne(unsigned char const*, unsigned long, bool, fuzzer::InputInfo*, bool, bool*) (/mnt/h/Security/Binary/Reports/fuzz_vuln/libvpx/fuzzers_bin/vpx_codec_control_fuzzer_vp8+0x51549) (BuildId: 84db5f9c0fcf8e35e6dd79c60ea9a0941d9f1de7)
    #5 0x562f6ec12015 in fuzzer::Fuzzer::MutateAndTestOne() (/mnt/h/Security/Binary/Reports/fuzz_vuln/libvpx/fuzzers_bin/vpx_codec_control_fuzzer_vp8+0x53015) (BuildId: 84db5f9c0fcf8e35e6dd79c60ea9a0941d9f1de7)
    #6 0x562f6ec12b05 in fuzzer::Fuzzer::Loop(std::vector<fuzzer::SizedFile, std::allocator<fuzzer::SizedFile>>&) (/mnt/h/Security/Binary/Reports/fuzz_vuln/libvpx/fuzzers_bin/vpx_codec_control_fuzzer_vp8+0x53b05) (BuildId: 84db5f9c0fcf8e35e6dd79c60ea9a0941d9f1de7)
    #7 0x562f6ebff0c5 in fuzzer::FuzzerDriver(int*, char***, int (*)(unsigned char const*, unsigned long)) (/mnt/h/Security/Binary/Reports/fuzz_vuln/libvpx/fuzzers_bin/vpx_codec_control_fuzzer_vp8+0x400c5) (BuildId: 84db5f9c0fcf8e35e6dd79c60ea9a0941d9f1de7)
    #8 0x562f6ec2b426 in main (/mnt/h/Security/Binary/Reports/fuzz_vuln/libvpx/fuzzers_bin/vpx_codec_control_fuzzer_vp8+0x6c426) (BuildId: 84db5f9c0fcf8e35e6dd79c60ea9a0941d9f1de7)
    #9 0x7660dc033ca7 in __libc_start_call_main csu/../sysdeps/nptl/libc_start_call_main.h:58:16
    #10 0x7660dc033d64 in __libc_start_main csu/../csu/libc-start.c:360:3
    #11 0x562f6ebf3420 in _start (/mnt/h/Security/Binary/Reports/fuzz_vuln/libvpx/fuzzers_bin/vpx_codec_control_fuzzer_vp8+0x34420) (BuildId: 84db5f9c0fcf8e35e6dd79c60ea9a0941d9f1de7)

==2652==Register values:
rax = 0x000000004c000000  rbx = 0x00007660da43aac0  rcx = 0x0000000000000000  rdx = 0x00007fff8d1b8b90
rdi = 0x0000518000000090  rsi = 0x00007fff8d1b8b68  rbp = 0x00007fff8d1b8cf0  rsp = 0x00007fff8d1b8b58
 r8 = 0x0000562f6ee54200   r9 = 0x0000000000000000  r10 = 0x000000000000000c  r11 = 0x0000000000000000
r12 = 0x0000507000002400  r13 = 0x0000507000002402  r14 = 0x0000507000002404  r15 = 0x00000ecc9b47f500
AddressSanitizer can not provide additional info.
SUMMARY: AddressSanitizer: SEGV /mnt/h/Security/Binary/Reports/fuzz_vuln/libvpx/libvpx/build/../vp8/vp8_dx_iface.c:630:23 in vp8_set_postproc
==2652==ABORTING
MS: 1 ChangeBinInt-; base unit: 3b63ec564a73c8badeb5b31b438b1498f82c0811
0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x4c,0x48,0x48,0x48,0x48,0x48,0x48,0x48,0x48,0x48,0x48,0x48,0x48,0x48,0x48,0x48,0x48,0x48,0x48,0x48,0x48,0x48,0x48,0x8,0x48,0x48,0x48,0x48,0x48,0x48,0x48,0x48,0x48,0x48,0x48,0x48,0x48,0x48,0x48,0x48,0x48,0x48,0x48,0x48,0x48,0x48,0xbb,0xbb,0x48,0x48,0x48,0x48,0x48,0x48,0x48,0x48,0x48,0x48,0x48,0x48,0x48,0xbb,0xbb,0xbb,0xbb,0xbb,0xbb,0xbb,0x48,
\000\000\000\000\000\000\000LHHHHHHHHHHHHHHHHHHHHHH\010HHHHHHHHHHHHHHHHHHHHHH\273\273HHHHHHHHHHHHH\273\273\273\273\273\273\273H
artifact_prefix='./'; Test unit written to ./crash-d3a1ed54d420320f1049ec89a1cd4e850b4f7142
Base64: AAAAAAAAAExISEhISEhISEhISEhISEhISEhISEhICEhISEhISEhISEhISEhISEhISEhISEi7u0hISEhISEhISEhISEi7u7u7u7u7SA==
```

