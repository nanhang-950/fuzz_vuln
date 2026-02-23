/*
 * Fuzzer for libvpx multi-frame sequence handling
 * ================================================
 * 
 * This fuzzer targets frame sequence handling, reference frame management,
 * and inter-frame dependencies in VP8/VP9 codecs. It tests scenarios like:
 * - Keyframe and inter-frame sequences
 * - Reference frame corruption
 * - Frame reordering
 * - Temporal layer switching
 * - Error resilience
 * 
 * Attack Surface:
 * - Frame reference management
 * - Temporal scalability
 * - Error concealment
 * - Frame buffer management
 * - GOP structure handling
 * 
 * Build:
 * ------
 * $CC -fsanitize=fuzzer,address -DDECODER=vp9 \
 *     -I./libvpx -I./libvpx/build \
 *     vpx_multiframe_fuzzer.c -o vpx_multiframe_fuzzer \
 *     ./libvpx/build/libvpx.a -lpthread -lm
 * 
 * Run:
 * ----
 * ./vpx_multiframe_fuzzer -max_len=131072 corpus/
 */

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "vpx/vp8dx.h"
#include "vpx/vpx_decoder.h"

#define VPXD_INTERFACE(name) VPXD_INTERFACE_(name)
#define VPXD_INTERFACE_(name) vpx_codec_##name##_dx()

#define MAX_FRAMES 100
#define MAX_FRAME_SIZE 65536
#define HEADER_SIZE 16

typedef struct {
    uint8_t num_frames;           // Number of frames to process
    uint8_t flags;                // Various flags
    uint8_t threads;              // Number of threads (1-64)
    uint8_t error_resilient;      // Error resilient mode
    uint8_t postproc_flags;       // Postprocessing flags
    uint8_t frame_parallel;       // Frame parallel decoding
    uint8_t loop_filter_opt;      // Loop filter optimization
    uint8_t reserved[9];
} sequence_config_t;

typedef struct {
    uint16_t frame_size;          // Size of this frame
    uint8_t frame_type;           // 0=keyframe, 1=interframe
    uint8_t flags;                // Frame-specific flags
    uint8_t corrupt_level;        // Corruption level (0-255)
    uint8_t skip_decode;          // Skip decoding this frame
    uint8_t reserved[2];
} frame_header_t;

static void corrupt_frame_data(uint8_t *data, size_t size, uint8_t level) {
    if (level == 0 || size == 0) return;
    
    // Corrupt random bytes based on corruption level
    size_t num_corruptions = (size * level) / 255;
    for (size_t i = 0; i < num_corruptions && i < size; i++) {
        size_t pos = (i * 997) % size; // Pseudo-random position
        data[pos] ^= 0xFF; // Flip all bits
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < sizeof(sequence_config_t)) {
        return 0;
    }

    const sequence_config_t *config = (const sequence_config_t *)data;
    data += sizeof(sequence_config_t);
    size -= sizeof(sequence_config_t);

    // Initialize decoder
    vpx_codec_ctx_t decoder;
    vpx_codec_dec_cfg_t dec_cfg;
    
    // Set thread count (1-64)
    dec_cfg.threads = (config->threads & 0x3F) + 1;
    dec_cfg.w = 0;
    dec_cfg.h = 0;

    // Set decoder flags
    vpx_codec_flags_t flags = 0;
    if (config->flags & 0x01) {
        flags |= VPX_CODEC_USE_POSTPROC;
    }
    if (config->flags & 0x02) {
        flags |= VPX_CODEC_USE_ERROR_CONCEALMENT;
    }
    if (config->flags & 0x04) {
        flags |= VPX_CODEC_USE_FRAME_THREADING;
    }

    vpx_codec_err_t err = vpx_codec_dec_init(&decoder, VPXD_INTERFACE(DECODER), 
                                              &dec_cfg, flags);
    if (err != VPX_CODEC_OK) {
        // Try without postproc if it failed
        flags &= ~VPX_CODEC_USE_POSTPROC;
        if (vpx_codec_dec_init(&decoder, VPXD_INTERFACE(DECODER), &dec_cfg, flags) != VPX_CODEC_OK) {
            return 0;
        }
    }

    // Configure decoder controls
    if (config->loop_filter_opt & 0x01) {
        vpx_codec_control_(&decoder, VP9D_SET_LOOP_FILTER_OPT, 1);
    }

    if (config->frame_parallel & 0x01) {
        vpx_codec_control_(&decoder, VP9D_SET_ROW_MT, 1);
    }

    // Configure postprocessing
    if (flags & VPX_CODEC_USE_POSTPROC) {
        vp8_postproc_cfg_t pp;
        pp.post_proc_flag = config->postproc_flags;
        pp.deblocking_level = (config->postproc_flags >> 4) & 0x0F;
        pp.noise_level = config->postproc_flags & 0x0F;
        vpx_codec_control_(&decoder, VP8_SET_POSTPROC, &pp);
    }

    // Process frame sequence
    uint8_t num_frames = config->num_frames;
    if (num_frames > MAX_FRAMES) num_frames = MAX_FRAMES;

    int frames_decoded = 0;
    int keyframes_seen = 0;
    
    for (uint8_t i = 0; i < num_frames && size > sizeof(frame_header_t); i++) {
        const frame_header_t *frame_hdr = (const frame_header_t *)data;
        data += sizeof(frame_header_t);
        size -= sizeof(frame_header_t);

        // Get frame size
        size_t frame_size = frame_hdr->frame_size;
        if (frame_size > size) frame_size = size;
        if (frame_size > MAX_FRAME_SIZE) frame_size = MAX_FRAME_SIZE;
        if (frame_size == 0) continue;

        // Allocate frame buffer
        uint8_t *frame_data = (uint8_t *)malloc(frame_size);
        if (!frame_data) break;

        memcpy(frame_data, data, frame_size);

        // Apply corruption if requested
        if (frame_hdr->corrupt_level > 0) {
            corrupt_frame_data(frame_data, frame_size, frame_hdr->corrupt_level);
        }

        // Skip decoding if requested (tests error handling)
        if (frame_hdr->skip_decode & 0x01) {
            free(frame_data);
            data += frame_size;
            size -= frame_size;
            continue;
        }

        // Decode frame
        err = vpx_codec_decode(&decoder, frame_data, frame_size, NULL, 0);
        
        // Track keyframes
        if (err == VPX_CODEC_OK) {
            vpx_codec_iter_t iter = NULL;
            vpx_image_t *img;
            while ((img = vpx_codec_get_frame(&decoder, &iter)) != NULL) {
                frames_decoded++;
                
                // Touch image data to trigger any memory issues
                if (img->planes[0]) {
                    volatile uint8_t dummy = img->planes[0][0];
                    (void)dummy;
                }
                
                // Check if this is a keyframe by examining stream info
                vpx_codec_stream_info_t si;
                si.sz = sizeof(si);
                if (vpx_codec_get_stream_info(&decoder, &si) == VPX_CODEC_OK) {
                    if (si.is_kf) {
                        keyframes_seen++;
                    }
                }
            }
        }

        // Test frame-specific controls
        if (frame_hdr->flags & 0x01) {
            // Toggle postprocessing mid-stream
            if (flags & VPX_CODEC_USE_POSTPROC) {
                vp8_postproc_cfg_t pp;
                pp.post_proc_flag = frame_hdr->flags;
                pp.deblocking_level = (frame_hdr->flags >> 4) & 0x0F;
                pp.noise_level = frame_hdr->flags & 0x0F;
                vpx_codec_control_(&decoder, VP8_SET_POSTPROC, &pp);
            }
        }

        if (frame_hdr->flags & 0x02) {
            // Test invert tile decode order
            vpx_codec_control_(&decoder, VP9_INVERT_TILE_DECODE_ORDER, 1);
        }

        free(frame_data);
        data += frame_size;
        size -= frame_size;

        // Test NULL frame decode (flush)
        if (frame_hdr->flags & 0x04) {
            vpx_codec_decode(&decoder, NULL, 0, NULL, 0);
            vpx_codec_iter_t iter = NULL;
            while (vpx_codec_get_frame(&decoder, &iter) != NULL) {
                frames_decoded++;
            }
        }
    }

    // Final flush
    vpx_codec_decode(&decoder, NULL, 0, NULL, 0);
    vpx_codec_iter_t iter = NULL;
    while (vpx_codec_get_frame(&decoder, &iter) != NULL) {
        frames_decoded++;
    }

    // Cleanup
    vpx_codec_destroy(&decoder);

    return 0;
}
