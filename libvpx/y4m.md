**目标**: Y4M (YUV4MPEG2) 格式解析

**测试内容**:

- Y4M 文件头解析
- 帧头解析
- 色彩空间参数
- 交错模式
- 帧率和宽高比
- 畸形头部处理

## project address

**libvpx** 是 Google 的 **WebM 项目的核心库**，提供 **VP8 和 VP9 视频编解码**功能。

**官方网站**：https://chromium.googlesource.com/webm/libvpx

## info

OS：Ubuntu22.04 TLS

Build: 

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

## fuzzer

```c
/*
 * Fuzzer for libvpx Y4M (YUV4MPEG2) format parsing
 * =================================================
 * 
 * Y4M is a simple uncompressed video format used for testing and
 * as input to video encoders. It consists of a file header and
 * frame headers followed by raw YUV data.
 * 
 * Attack Surface:
 * - Y4M file header parsing
 * - Frame header parsing
 * - Colorspace parameter parsing
 * - Interlacing mode parsing
 * - Aspect ratio parsing
 * - Frame rate parsing
 * - Resolution parsing
 * - YUV data reading
 * 
 * Build:
 * ------
 * $CC -fsanitize=fuzzer,address -DENCODER=vp9 \
 *     -I./libvpx -I./libvpx/build \
 *     vpx_y4m_fuzzer.c libvpx/y4minput.c \
 *     -o vpx_y4m_fuzzer \
 *     ./libvpx/build/libvpx.a -lpthread -lm
 * 
 * Run:
 * ----
 * ./vpx_y4m_fuzzer -max_len=262144 corpus/
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define Y4M_MAGIC "YUV4MPEG2 "
#define Y4M_FRAME_MAGIC "FRAME"
#define MAX_Y4M_HEADER 1024
#define MAX_WIDTH 4096
#define MAX_HEIGHT 4096

typedef struct {
    int width;
    int height;
    int fps_num;
    int fps_den;
    int par_num;
    int par_den;
    char interlace;
    char colorspace[16];
} y4m_info_t;

// Parse Y4M file header
static int parse_y4m_header(const uint8_t *data, size_t size, 
                            y4m_info_t *info, size_t *header_len) {
    if (size < strlen(Y4M_MAGIC)) {
        return -1;
    }

    // Check magic
    if (memcmp(data, Y4M_MAGIC, strlen(Y4M_MAGIC)) != 0) {
        return -1;
    }

    // Initialize defaults
    info->width = 0;
    info->height = 0;
    info->fps_num = 30;
    info->fps_den = 1;
    info->par_num = 1;
    info->par_den = 1;
    info->interlace = 'p'; // progressive
    strcpy(info->colorspace, "420");

    // Find end of header (newline)
    size_t i;
    for (i = strlen(Y4M_MAGIC); i < size && i < MAX_Y4M_HEADER; i++) {
        if (data[i] == '\n') {
            break;
        }
    }

    if (i >= size) {
        return -1;
    }

    *header_len = i + 1;

    // Parse parameters
    const char *header = (const char *)data + strlen(Y4M_MAGIC);
    size_t header_size = i - strlen(Y4M_MAGIC);

    for (size_t pos = 0; pos < header_size; pos++) {
        if (header[pos] == ' ') continue;

        char tag = header[pos];
        pos++;

        // Find end of value
        size_t value_start = pos;
        while (pos < header_size && header[pos] != ' ' && header[pos] != '\n') {
            pos++;
        }

        char value[256];
        size_t value_len = pos - value_start;
        if (value_len >= sizeof(value)) value_len = sizeof(value) - 1;
        memcpy(value, header + value_start, value_len);
        value[value_len] = '\0';

        // Parse tag
        switch (tag) {
            case 'W': // Width
                info->width = atoi(value);
                if (info->width <= 0 || info->width > MAX_WIDTH) {
                    info->width = 320;
                }
                break;

            case 'H': // Height
                info->height = atoi(value);
                if (info->height <= 0 || info->height > MAX_HEIGHT) {
                    info->height = 240;
                }
                break;

            case 'F': // Frame rate
                if (sscanf(value, "%d:%d", &info->fps_num, &info->fps_den) != 2) {
                    info->fps_num = 30;
                    info->fps_den = 1;
                }
                if (info->fps_num <= 0) info->fps_num = 30;
                if (info->fps_den <= 0) info->fps_den = 1;
                break;

            case 'A': // Aspect ratio
                if (sscanf(value, "%d:%d", &info->par_num, &info->par_den) != 2) {
                    info->par_num = 1;
                    info->par_den = 1;
                }
                if (info->par_num <= 0) info->par_num = 1;
                if (info->par_den <= 0) info->par_den = 1;
                break;

            case 'I': // Interlacing
                if (value_len > 0) {
                    info->interlace = value[0];
                }
                break;

            case 'C': // Colorspace
                if (value_len > 0 && value_len < sizeof(info->colorspace)) {
                    strcpy(info->colorspace, value);
                }
                break;
        }
    }

    return 0;
}

// Parse Y4M frame header
static int parse_frame_header(const uint8_t *data, size_t size, 
                              size_t *header_len) {
    if (size < strlen(Y4M_FRAME_MAGIC)) {
        return -1;
    }

    // Check magic
    if (memcmp(data, Y4M_FRAME_MAGIC, strlen(Y4M_FRAME_MAGIC)) != 0) {
        return -1;
    }

    // Find end of frame header (newline)
    size_t i;
    for (i = strlen(Y4M_FRAME_MAGIC); i < size && i < 256; i++) {
        if (data[i] == '\n') {
            break;
        }
    }

    if (i >= size) {
        return -1;
    }

    *header_len = i + 1;
    return 0;
}

// Calculate frame size based on colorspace
static size_t calculate_frame_size(const y4m_info_t *info) {
    size_t y_size = info->width * info->height;
    size_t uv_size;

    if (strcmp(info->colorspace, "420") == 0 ||
        strcmp(info->colorspace, "420jpeg") == 0 ||
        strcmp(info->colorspace, "420mpeg2") == 0) {
        // 4:2:0 - UV planes are 1/4 size each
        uv_size = y_size / 4;
        return y_size + uv_size * 2;
    } else if (strcmp(info->colorspace, "422") == 0) {
        // 4:2:2 - UV planes are 1/2 size each
        uv_size = y_size / 2;
        return y_size + uv_size * 2;
    } else if (strcmp(info->colorspace, "444") == 0) {
        // 4:4:4 - UV planes are same size as Y
        return y_size * 3;
    } else if (strcmp(info->colorspace, "mono") == 0) {
        // Monochrome - Y plane only
        return y_size;
    }

    // Default to 4:2:0
    uv_size = y_size / 4;
    return y_size + uv_size * 2;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 32) {
        return 0;
    }

    // Parse file header
    y4m_info_t info;
    size_t file_header_len;
    
    if (parse_y4m_header(data, size, &info, &file_header_len) != 0) {
        // Try parsing without proper header (malformed file)
        info.width = 320;
        info.height = 240;
        info.fps_num = 30;
        info.fps_den = 1;
        strcpy(info.colorspace, "420");
        file_header_len = 0;
    }

    data += file_header_len;
    size -= file_header_len;

    // Calculate expected frame size
    size_t frame_data_size = calculate_frame_size(&info);
    if (frame_data_size == 0 || frame_data_size > 100 * 1024 * 1024) {
        return 0; // Unreasonable frame size
    }

    // Parse frames
    int frame_count = 0;
    const int max_frames = 10;

    while (size > 0 && frame_count < max_frames) {
        // Parse frame header
        size_t frame_header_len;
        if (parse_frame_header(data, size, &frame_header_len) != 0) {
            // Try to continue without frame header (malformed)
            frame_header_len = 0;
        }

        data += frame_header_len;
        size -= frame_header_len;

        // Read frame data
        size_t read_size = (size < frame_data_size) ? size : frame_data_size;
        if (read_size == 0) break;

        // Touch the frame data to trigger any issues
        volatile uint8_t dummy = data[0];
        if (read_size > 1) {
            dummy = data[read_size - 1];
        }
        if (read_size > frame_data_size / 2) {
            dummy = data[frame_data_size / 2];
        }
        (void)dummy;

        data += read_size;
        size -= read_size;
        frame_count++;
    }

    // Test edge cases
    
    // 1. Malformed header with extreme values
    if (size > 100) {
        y4m_info_t test_info;
        size_t test_len;
        parse_y4m_header(data - file_header_len, 100, &test_info, &test_len);
    }

    // 2. Missing frame headers
    if (frame_count > 0) {
        const uint8_t *test_data = data - frame_data_size;
        size_t test_size = frame_data_size;
        volatile uint8_t dummy = test_data[0];
        (void)dummy;
    }

    // 3. Truncated frames
    if (size > 10) {
        size_t truncated_size = size / 2;
        volatile uint8_t dummy = data[truncated_size - 1];
        (void)dummy;
    }

    return 0;
}
```

## Poc

https://github.com/z1r00/fuzz_vuln/blob/main/yasm/stack-overflow/parse_expr1/id:000206%2Csig:06%2Csrc:007018%2B003531%2Cop:splice%2Crep:32

## ASAN Info

```
❯ ./vpx_y4m_fuzzer
INFO: Running with entropic power schedule (0xFF, 100).
INFO: Seed: 1753147002
INFO: Loaded 1 modules   (248 inline 8-bit counters): 248 [0x64baa92f4910, 0x64baa92f4a08),
INFO: Loaded 1 PC tables (248 PCs): 248 [0x64baa92f4a08,0x64baa92f5988),
INFO: -max_len is not provided; libFuzzer will not generate inputs larger than 4096 bytes
INFO: A corpus is not provided, starting from an empty corpus
#2      INITED cov: 2 ft: 2 corp: 1/1b exec/s: 0 rss: 31Mb
AddressSanitizer:DEADLYSIGNAL
=================================================================
==2738==ERROR: AddressSanitizer: SEGV on unknown address 0x503ffffe3ff1 (pc 0x64baa92ab7b8 bp 0x7ffc6451afe0 sp 0x7ffc6451af00 T0)
==2738==The signal is caused by a READ memory access.
    #0 0x64baa92ab7b8 in LLVMFuzzerTestOneInput /mnt/h/Security/Binary/Reports/fuzz_vuln/libvpx/vpx_y4m_fuzzer.c:296:34
    #1 0x64baa91af4ba in fuzzer::Fuzzer::ExecuteCallback(unsigned char const*, unsigned long) (/mnt/h/Security/Binary/Reports/fuzz_vuln/libvpx/fuzzers_bin/vpx_y4m_fuzzer+0x514ba) (BuildId: 68ef0571f03bd4312db0fcc6101244b007ef5c68)
    #2 0x64baa91aeac9 in fuzzer::Fuzzer::RunOne(unsigned char const*, unsigned long, bool, fuzzer::InputInfo*, bool, bool*) (/mnt/h/Security/Binary/Reports/fuzz_vuln/libvpx/fuzzers_bin/vpx_y4m_fuzzer+0x50ac9) (BuildId: 68ef0571f03bd4312db0fcc6101244b007ef5c68)
    #3 0x64baa91b0595 in fuzzer::Fuzzer::MutateAndTestOne() (/mnt/h/Security/Binary/Reports/fuzz_vuln/libvpx/fuzzers_bin/vpx_y4m_fuzzer+0x52595) (BuildId: 68ef0571f03bd4312db0fcc6101244b007ef5c68)
    #4 0x64baa91b1085 in fuzzer::Fuzzer::Loop(std::vector<fuzzer::SizedFile, std::allocator<fuzzer::SizedFile>>&) (/mnt/h/Security/Binary/Reports/fuzz_vuln/libvpx/fuzzers_bin/vpx_y4m_fuzzer+0x53085) (BuildId: 68ef0571f03bd4312db0fcc6101244b007ef5c68)
    #5 0x64baa919d645 in fuzzer::FuzzerDriver(int*, char***, int (*)(unsigned char const*, unsigned long)) (/mnt/h/Security/Binary/Reports/fuzz_vuln/libvpx/fuzzers_bin/vpx_y4m_fuzzer+0x3f645) (BuildId: 68ef0571f03bd4312db0fcc6101244b007ef5c68)
    #6 0x64baa91c99a6 in main (/mnt/h/Security/Binary/Reports/fuzz_vuln/libvpx/fuzzers_bin/vpx_y4m_fuzzer+0x6b9a6) (BuildId: 68ef0571f03bd4312db0fcc6101244b007ef5c68)
    #7 0x73ee91a33ca7 in __libc_start_call_main csu/../sysdeps/nptl/libc_start_call_main.h:58:16
    #8 0x73ee91a33d64 in __libc_start_main csu/../csu/libc-start.c:360:3
    #9 0x64baa91919a0 in _start (/mnt/h/Security/Binary/Reports/fuzz_vuln/libvpx/fuzzers_bin/vpx_y4m_fuzzer+0x339a0) (BuildId: 68ef0571f03bd4312db0fcc6101244b007ef5c68)

==2738==Register values:
rax = 0x0000000000000000  rbx = 0x00007ffc6451af00  rcx = 0x0000000000000001  rdx = 0x00000000000018d8
rdi = 0x0000000000002000  rsi = 0x0000000000000002  rbp = 0x00007ffc6451afe0  rsp = 0x00007ffc6451af00
 r8 = 0x000064baa92f6000   r9 = 0x0000000000000110  r10 = 0x0000000000000001  r11 = 0x00000c97d5250c18
r12 = 0x0000000000000001  r13 = 0x00005040000001f1  r14 = 0x0000000000000000  r15 = 0x0000503ffffe3ff1
AddressSanitizer can not provide additional info.
SUMMARY: AddressSanitizer: SEGV /mnt/h/Security/Binary/Reports/fuzz_vuln/libvpx/vpx_y4m_fuzzer.c:296:34 in LLVMFuzzerTestOneInput
==2738==ABORTING
MS: 5 ShuffleBytes-ChangeBit-InsertRepeatedBytes-InsertRepeatedBytes-CopyPart-; base unit: adc83b19e793491b1c6ea0fd8b46cd9f32e592fc
0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x1a,
\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\032
artifact_prefix='./'; Test unit written to ./crash-a8816fc673feac79c1d9a04fc160c46c5865396c
Base64: AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAa
```



## Reference

https://github.com/z1r00/fuzz_vuln/blob/main/yasm/stack-overflow/parse_expr1/readme.md
