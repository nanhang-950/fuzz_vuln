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

**Objective**: Image Format Conversion and Processing

**Test Content**:

- Various pixel formats (I420, I422, I444, NV12, YV12)
- High bit-depth formats (10-bit, 12-bit)
- Color space conversion (BT.601, BT.709, BT.2020)
- Image allocation and wrapping
- Display rectangle configuration
- Image flip operations

**Input Format**:

- First 32 bytes: configuration header
- Remaining data: YUV pixel data

```c
/*
 * Fuzzer for libvpx image format conversion and manipulation
 * ==========================================================
 * 
 * This fuzzer targets the image format conversion, color space handling,
 * and pixel format manipulation functions in libvpx.
 * 
 * Attack Surface:
 * - vpx_img_alloc() - Image allocation with various formats
 * - vpx_img_wrap() - Wrapping external buffers
 * - vpx_img_set_rect() - Setting display rectangles
 * - vpx_img_flip() - Vertical flipping
 * - Color space conversions (I420, I422, I444, NV12, etc.)
 * - High bit depth formats (10-bit, 12-bit)
 * 
 * Build:
 * ------
 * $CC -fsanitize=fuzzer,address -I./libvpx -I./libvpx/build \
 *     vpx_image_fuzzer.c -o vpx_image_fuzzer \
 *     ./libvpx/build/libvpx.a -lpthread -lm
 * 
 * Run:
 * ----
 * ./vpx_image_fuzzer -max_len=65536 corpus/
 */

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "vpx/vpx_image.h"

#define MAX_WIDTH 4096
#define MAX_HEIGHT 4096
#define MIN_SIZE 16

// Fuzz input structure (first 32 bytes)
typedef struct {
    uint8_t format;           // Image format selector
    uint8_t width_high;       // Width high byte
    uint8_t width_low;        // Width low byte
    uint8_t height_high;      // Height high byte
    uint8_t height_low;       // Height low byte
    uint8_t align;            // Alignment
    uint8_t bit_depth;        // Bit depth (8, 10, 12)
    uint8_t operations;       // Operations to perform
    uint8_t rect_x;           // Rectangle x
    uint8_t rect_y;           // Rectangle y
    uint8_t rect_w;           // Rectangle width
    uint8_t rect_h;           // Rectangle height
    uint8_t color_space;      // Color space
    uint8_t color_range;      // Color range
    uint8_t flip;             // Flip operation
    uint8_t reserved[17];     // Reserved for future use
} fuzz_config_t;

static vpx_img_fmt_t get_image_format(uint8_t selector, uint8_t bit_depth) {
    // Select format based on bit depth
    if (bit_depth == 10 || bit_depth == 12) {
        switch (selector % 4) {
            case 0: return VPX_IMG_FMT_I42016;
            case 1: return VPX_IMG_FMT_I42216;
            case 2: return VPX_IMG_FMT_I44416;
            case 3: return VPX_IMG_FMT_I44016;
        }
    }
    
    // 8-bit formats
    switch (selector % 7) {
        case 0: return VPX_IMG_FMT_I420;
        case 1: return VPX_IMG_FMT_I422;
        case 2: return VPX_IMG_FMT_I444;
        case 3: return VPX_IMG_FMT_I440;
        case 4: return VPX_IMG_FMT_YV12;
        case 5: return VPX_IMG_FMT_NV12;
        default: return VPX_IMG_FMT_I420;
    }
}

static vpx_color_space_t get_color_space(uint8_t selector) {
    switch (selector % 8) {
        case 0: return VPX_CS_UNKNOWN;
        case 1: return VPX_CS_BT_601;
        case 2: return VPX_CS_BT_709;
        case 3: return VPX_CS_SMPTE_170;
        case 4: return VPX_CS_SMPTE_240;
        case 5: return VPX_CS_BT_2020;
        case 6: return VPX_CS_RESERVED;
        case 7: return VPX_CS_SRGB;
        default: return VPX_CS_UNKNOWN;
    }
}

static vpx_color_range_t get_color_range(uint8_t selector) {
    return (selector & 1) ? VPX_CR_FULL_RANGE : VPX_CR_STUDIO_RANGE;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < sizeof(fuzz_config_t)) {
        return 0;
    }

    const fuzz_config_t *config = (const fuzz_config_t *)data;
    data += sizeof(fuzz_config_t);
    size -= sizeof(fuzz_config_t);

    // Parse dimensions with bounds checking
    unsigned int width = ((unsigned int)config->width_high << 8) | config->width_low;
    unsigned int height = ((unsigned int)config->height_high << 8) | config->height_low;
    
    // Clamp to reasonable values
    if (width < MIN_SIZE) width = MIN_SIZE;
    if (width > MAX_WIDTH) width = MAX_WIDTH;
    if (height < MIN_SIZE) height = MIN_SIZE;
    if (height > MAX_HEIGHT) height = MAX_HEIGHT;

    // Parse alignment (1, 2, 4, 8, 16, 32, 64)
    unsigned int align = 1 << (config->align % 7);

    // Parse bit depth
    uint8_t bit_depth = 8;
    if (config->bit_depth % 3 == 1) bit_depth = 10;
    else if (config->bit_depth % 3 == 2) bit_depth = 12;

    // Get image format
    vpx_img_fmt_t fmt = get_image_format(config->format, bit_depth);

    // Test 1: Allocate image
    vpx_image_t *img = vpx_img_alloc(NULL, fmt, width, height, align);
    if (!img) {
        return 0;
    }

    // Set color space and range
    img->cs = get_color_space(config->color_space);
    img->range = get_color_range(config->color_range);

    // Test 2: Fill image with fuzz data
    if (size > 0) {
        for (int plane = 0; plane < 3; plane++) {
            if (!img->planes[plane]) continue;
            
            unsigned int plane_width = img->d_w;
            unsigned int plane_height = img->d_h;
            
            if (plane > 0) {
                if (img->x_chroma_shift > 0) {
                    plane_width = (plane_width + 1) >> img->x_chroma_shift;
                }
                if (img->y_chroma_shift > 0) {
                    plane_height = (plane_height + 1) >> img->y_chroma_shift;
                }
            }

            // Handle NV12 special case
            if (fmt == VPX_IMG_FMT_NV12 && plane > 1) break;
            if (fmt == VPX_IMG_FMT_NV12 && plane == 1) {
                plane_width = (plane_width + 1) & ~1;
            }

            size_t bytes_per_pixel = (img->fmt & VPX_IMG_FMT_HIGHBITDEPTH) ? 2 : 1;
            size_t row_bytes = plane_width * bytes_per_pixel;
            
            for (unsigned int y = 0; y < plane_height && size > 0; y++) {
                unsigned char *row = img->planes[plane] + y * img->stride[plane];
                size_t copy_size = (size < row_bytes) ? size : row_bytes;
                memcpy(row, data, copy_size);
                data += copy_size;
                size -= copy_size;
            }
        }
    }

    // Test 3: Set display rectangle
    if (config->operations & 0x01) {
        unsigned int rect_x = config->rect_x % width;
        unsigned int rect_y = config->rect_y % height;
        unsigned int rect_w = (config->rect_w % (width - rect_x)) + 1;
        unsigned int rect_h = (config->rect_h % (height - rect_y)) + 1;
        vpx_img_set_rect(img, rect_x, rect_y, rect_w, rect_h);
    }

    // Test 4: Flip image
    if (config->flip & 0x01) {
        vpx_img_flip(img);
    }

    // Test 5: Allocate second image for format conversion test
    if (config->operations & 0x02) {
        vpx_img_fmt_t fmt2 = get_image_format(config->format + 1, bit_depth);
        vpx_image_t *img2 = vpx_img_alloc(NULL, fmt2, width, height, align);
        if (img2) {
            // Simulate format conversion by copying data
            for (int plane = 0; plane < 3; plane++) {
                if (!img->planes[plane] || !img2->planes[plane]) continue;
                
                unsigned int h = img->d_h;
                if (plane > 0 && img->y_chroma_shift > 0) {
                    h = (h + 1) >> img->y_chroma_shift;
                }
                
                for (unsigned int y = 0; y < h; y++) {
                    // Just touch the memory to trigger any issues
                    unsigned char *row =
                        img->planes[plane] +
                        (ptrdiff_t)y * (ptrdiff_t)img->stride[plane];
                    volatile uint8_t dummy = row[0];
                    (void)dummy;
                }
            }
            vpx_img_free(img2);
        }
    }

    // Test 6: Test wrapping external buffer
    if (config->operations & 0x04) {
        // Allocate a buffer
        size_t pixels = (size_t)width * (size_t)height;
        if (pixels > SIZE_MAX / 8) {
            vpx_img_free(img);
            return 0;
        }
        // Use a larger upper bound to avoid wrap metadata pointing past the end.
        size_t buffer_size = pixels * 8 + align;
        uint8_t *buffer = (uint8_t *)malloc(buffer_size);
        if (buffer) {
            vpx_image_t wrapped_img;
            vpx_image_t *wrap_result = vpx_img_wrap(&wrapped_img, fmt, 
                                                     width, height, 1, buffer);
            if (wrap_result) {
                // Touch the wrapped image
                uintptr_t base = (uintptr_t)buffer;
                uintptr_t end = base + buffer_size;
                for (int plane = 0; plane < 3; plane++) {
                    if (wrap_result->planes[plane]) {
                        uintptr_t p = (uintptr_t)wrap_result->planes[plane];
                        if (p >= base && p < end) {
                            volatile uint8_t dummy = wrap_result->planes[plane][0];
                            (void)dummy;
                        }
                    }
                }
            }
            free(buffer);
        }
    }

    // Cleanup
    vpx_img_free(img);
    
    return 0;
}
```



## Poc

https://github.com/nanhang-950/fuzz_vuln/blob/main/libvpx/fuzzers/crash-883ef121deb9e622bd4b1bfc5a7b634047492381

## ASAN Info

```shell
❯ ./vpx_codec_control_fuzzer_vp8 ./crash-d3a1ed54d420320f1049ec89a1cd4e850b4f7142
❯ ./vpx_codec_control_fuzzer_vp9 ./crash-33724cd6b286c15eb766589eeaaea79e90ff0691
❯ ./vpx_y4m_fuzzer ./crash-a8816fc673feac79c1d9a04fc160c46c5865396c
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

