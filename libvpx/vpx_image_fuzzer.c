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
