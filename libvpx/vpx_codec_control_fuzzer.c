/*
 * Fuzzer for libvpx codec control interface
 * ==========================================
 * 
 * This fuzzer targets the vpx_codec_control() interface which allows
 * runtime configuration of encoder and decoder parameters. This is a
 * critical attack surface as it accepts arbitrary control IDs and
 * parameters.
 * 
 * Attack Surface:
 * - vpx_codec_control_() with various control IDs
 * - VP8/VP9 specific controls
 * - Encoder controls (bitrate, quality, threading, etc.)
 * - Decoder controls (postprocessing, loop filter, etc.)
 * - Invalid control ID handling
 * - Type mismatches and invalid parameters
 * 
 * Build:
 * ------
 * $CC -fsanitize=fuzzer,address -DENCODER=vp9 -DDECODER=vp9 \
 *     -I./libvpx -I./libvpx/build \
 *     vpx_codec_control_fuzzer.c -o vpx_codec_control_fuzzer \
 *     ./libvpx/build/libvpx.a -lpthread -lm
 * 
 * Run:
 * ----
 * ./vpx_codec_control_fuzzer -max_len=4096 corpus/
 */

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "vpx/vpx_encoder.h"
#include "vpx/vpx_decoder.h"
#include "vpx/vp8cx.h"
#include "vpx/vp8dx.h"

#define VPXC_INTERFACE(name) VPXC_INTERFACE_(name)
#define VPXC_INTERFACE_(name) vpx_codec_##name##_cx()

#define VPXD_INTERFACE(name) VPXD_INTERFACE_(name)
#define VPXD_INTERFACE_(name) vpx_codec_##name##_dx()

// Common VP8/VP9 encoder control IDs
static const int encoder_control_ids[] = {
    VP8E_SET_CPUUSED,
    VP8E_SET_ENABLEAUTOALTREF,
    VP8E_SET_NOISE_SENSITIVITY,
    VP8E_SET_SHARPNESS,
    VP8E_SET_STATIC_THRESHOLD,
    VP8E_SET_TOKEN_PARTITIONS,
    VP8E_SET_ARNR_MAXFRAMES,
    VP8E_SET_ARNR_STRENGTH,
    VP8E_SET_ARNR_TYPE,
    VP8E_SET_TUNING,
    VP8E_SET_CQ_LEVEL,
    VP8E_SET_MAX_INTRA_BITRATE_PCT,
    VP8E_SET_SCREEN_CONTENT_MODE,
    VP9E_SET_LOSSLESS,
    VP9E_SET_TILE_COLUMNS,
    VP9E_SET_TILE_ROWS,
    VP9E_SET_FRAME_PARALLEL_DECODING,
    VP9E_SET_AQ_MODE,
    VP9E_SET_FRAME_PERIODIC_BOOST,
    VP9E_SET_NOISE_SENSITIVITY,
    VP9E_SET_TUNE_CONTENT,
    VP9E_SET_COLOR_SPACE,
    VP9E_SET_MIN_GF_INTERVAL,
    VP9E_SET_MAX_GF_INTERVAL,
    VP9E_SET_TARGET_LEVEL,
    VP9E_SET_ROW_MT,
    VP9E_SET_TPL,
    VP9E_SET_POSTENCODE_DROP,
};

// Decoder control IDs: VP8 and VP9 have different sets (no VP8_SET_DBG_* in this libvpx)
#if defined(VP9) && VP9
static const int decoder_control_ids[] = {
    VP8_SET_POSTPROC,
    VP9D_SET_LOOP_FILTER_OPT,
    VP9D_SET_ROW_MT,
    VP9_INVERT_TILE_DECODE_ORDER,
    VP9_SET_BYTE_ALIGNMENT,
    VP9_SET_SKIP_LOOP_FILTER,
};
#else
static const int decoder_control_ids[] = {
    VP8_SET_POSTPROC,
    VP8_SET_REFERENCE,
    VP8_COPY_REFERENCE,
};
#endif

typedef struct {
    uint8_t test_encoder;      // Test encoder (1) or decoder (0)
    uint8_t control_id_idx;    // Index into control ID array
    uint8_t param_type;        // Parameter type selector
    uint8_t reserved;
    int32_t int_param;         // Integer parameter
    uint32_t uint_param;       // Unsigned integer parameter
    uint8_t struct_data[64];   // Data for struct parameters
} fuzz_control_t;

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < sizeof(fuzz_control_t)) {
        return 0;
    }

    const fuzz_control_t *fuzz = (const fuzz_control_t *)data;
    data += sizeof(fuzz_control_t);
    size -= sizeof(fuzz_control_t);

    if (fuzz->test_encoder) {
        // Test encoder controls
        vpx_codec_ctx_t encoder;
        vpx_codec_enc_cfg_t cfg;
        
        if (vpx_codec_enc_config_default(VPXC_INTERFACE(ENCODER), &cfg, 0) != VPX_CODEC_OK) {
            return 0;
        }

        // Set basic config
        cfg.g_w = 320;
        cfg.g_h = 240;
        cfg.g_timebase.num = 1;
        cfg.g_timebase.den = 30;
        cfg.rc_target_bitrate = 200;

        if (vpx_codec_enc_init(&encoder, VPXC_INTERFACE(ENCODER), &cfg, 0) != VPX_CODEC_OK) {
            return 0;
        }

        // Select control ID
        size_t num_controls = sizeof(encoder_control_ids) / sizeof(encoder_control_ids[0]);
        int control_id = encoder_control_ids[fuzz->control_id_idx % num_controls];

        // Try different parameter types
        switch (fuzz->param_type % 8) {
            case 0: // Integer parameter
                vpx_codec_control_(&encoder, control_id, fuzz->int_param);
                break;
            
            case 1: // Unsigned integer parameter
                vpx_codec_control_(&encoder, control_id, fuzz->uint_param);
                break;
            
            case 2: // Small positive integer
                vpx_codec_control_(&encoder, control_id, fuzz->uint_param % 100);
                break;
            
            case 3: // Boolean (0 or 1)
                vpx_codec_control_(&encoder, control_id, fuzz->uint_param & 1);
                break;
            
            case 4: // Negative integer
                vpx_codec_control_(&encoder, control_id, -(int)fuzz->uint_param);
                break;
            
            case 5: // Large integer
                vpx_codec_control_(&encoder, control_id, INT32_MAX);
                break;
            
            case 6: // Zero
                vpx_codec_control_(&encoder, control_id, 0);
                break;
            
            case 7: // Struct parameter (VP8 postproc config)
                if (control_id == VP8_SET_POSTPROC) {
                    vp8_postproc_cfg_t pp;
                    memcpy(&pp, fuzz->struct_data, sizeof(pp) < sizeof(fuzz->struct_data) ? 
                           sizeof(pp) : sizeof(fuzz->struct_data));
                    vpx_codec_control_(&encoder, control_id, &pp);
                }
                break;
        }

        // Try multiple controls in sequence
        if (size >= 4) {
            for (size_t i = 0; i < size && i < 10; i++) {
                int ctrl_idx = data[i] % num_controls;
                int param = (i + 1) * 10;
                vpx_codec_control_(&encoder, encoder_control_ids[ctrl_idx], param);
            }
        }

        vpx_codec_destroy(&encoder);
    } else {
        // Test decoder controls
        vpx_codec_ctx_t decoder;
        vpx_codec_dec_cfg_t cfg = { 1, 0, 0 };
        
        if (vpx_codec_dec_init(&decoder, VPXD_INTERFACE(DECODER), &cfg, 0) != VPX_CODEC_OK) {
            return 0;
        }

        // Select control ID
        size_t num_controls = sizeof(decoder_control_ids) / sizeof(decoder_control_ids[0]);
        int control_id = decoder_control_ids[fuzz->control_id_idx % num_controls];

        // Try different parameter types
        switch (fuzz->param_type % 6) {
            case 0: // Integer parameter
                vpx_codec_control_(&decoder, control_id, fuzz->int_param);
                break;
            
            case 1: // Boolean
                vpx_codec_control_(&decoder, control_id, fuzz->uint_param & 1);
                break;
            
            case 2: // Small integer
                vpx_codec_control_(&decoder, control_id, fuzz->uint_param % 16);
                break;
            
            case 3: // VP8 postproc config
                if (control_id == VP8_SET_POSTPROC) {
                    vp8_postproc_cfg_t pp;
                    pp.post_proc_flag = fuzz->struct_data[0];
                    pp.deblocking_level = fuzz->struct_data[1] % 16;
                    pp.noise_level = fuzz->struct_data[2] % 16;
                    vpx_codec_control_(&decoder, control_id, &pp);
                }
                break;
            
            case 4: // Integer (e.g. VP9_SET_BYTE_ALIGNMENT, VP9D_SET_ROW_MT)
                vpx_codec_control_(&decoder, control_id, (int)(fuzz->uint_param % 1024));
                break;
            
            case 5: // Zero
                vpx_codec_control_(&decoder, control_id, 0);
                break;
        }

        // Try invalid control IDs
        if (fuzz->param_type & 0x80) {
            // Random control ID
            vpx_codec_control_(&decoder, fuzz->int_param, fuzz->uint_param);
            
            // Very large control ID
            vpx_codec_control_(&decoder, INT32_MAX, 0);
            
            // Negative control ID
            vpx_codec_control_(&decoder, -1, 0);
        }

        vpx_codec_destroy(&decoder);
    }

    return 0;
}
