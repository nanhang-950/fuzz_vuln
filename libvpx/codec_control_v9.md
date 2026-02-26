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

**Objective**: Codec Control Interface

**Test Content**:

- VP8/VP9 encoder control parameters
- VP8/VP9 decoder control parameters
- Handling of invalid control IDs
- Parameter type mismatch
- Extreme parameter values
- Control sequence combinations

**Variant**: `vpx_codec_control_fuzzer_vp9` – VP9-specific

```c
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

        // Use control-specific parameter types to avoid varargs type mismatch.
        if (control_id == VP8_SET_POSTPROC) {
            vp8_postproc_cfg_t pp;
            pp.post_proc_flag = fuzz->struct_data[0];
            pp.deblocking_level = fuzz->struct_data[1] % 16;
            pp.noise_level = fuzz->struct_data[2] % 16;
            vpx_codec_control_(&decoder, control_id, &pp);
        } else {
            switch (fuzz->param_type % 5) {
                case 0: // Integer parameter
                    vpx_codec_control_(&decoder, control_id, fuzz->int_param);
                    break;
                
                case 1: // Boolean
                    vpx_codec_control_(&decoder, control_id, fuzz->uint_param & 1);
                    break;
                
                case 2: // Small integer
                    vpx_codec_control_(&decoder, control_id, fuzz->uint_param % 16);
                    break;
                
                case 3: // Integer (e.g. VP9_SET_BYTE_ALIGNMENT, VP9D_SET_ROW_MT)
                    vpx_codec_control_(&decoder, control_id, (int)(fuzz->uint_param % 1024));
                    break;
                
                case 4: // Zero
                    vpx_codec_control_(&decoder, control_id, 0);
                    break;
            }
        }

        // Try invalid control IDs
        if (fuzz->param_type & 0x80) {
            // Use out-of-range control IDs with integer payloads.
            vpx_codec_control_(&decoder, 0x7ffff000, 0);
            vpx_codec_control_(&decoder, INT32_MAX, 0);
            vpx_codec_control_(&decoder, INT32_MIN, 0);
        }

        vpx_codec_destroy(&decoder);
    }

    return 0;
}
```

## Poc

https://github.com/nanhang-950/fuzz_vuln/blob/main/libvpx/crash-33724cd6b286c15eb766589eeaaea79e90ff0691

## ASAN Info

```
❯ ./vpx_codec_control_fuzzer_vp9 ./crash-33724cd6b286c15eb766589eeaaea79e90ff0691
INFO: Running with entropic power schedule (0xFF, 100).
INFO: Seed: 3776836915
INFO: Loaded 1 modules   (105 inline 8-bit counters): 105 [0x59f52e3e9588, 0x59f52e3e95f1),
INFO: Loaded 1 PC tables (105 PCs): 105 [0x59f52e3e95f8,0x59f52e3e9c88),
INFO: -max_len is not provided; libFuzzer will not generate inputs larger than 4096 bytes
INFO: A corpus is not provided, starting from an empty corpus
#2      INITED cov: 2 ft: 2 corp: 1/1b exec/s: 0 rss: 31Mb
#7732   NEW    cov: 12 ft: 12 corp: 2/79b lim: 80 exec/s: 0 rss: 47Mb L: 78/78 MS: 5 ChangeBit-CMP-InsertRepeatedBytes-EraseBytes-InsertRepeatedBytes- DE: "\001\000\000\000\000\000\000L"-
#7733   NEW    cov: 17 ft: 17 corp: 3/159b lim: 80 exec/s: 0 rss: 60Mb L: 80/80 MS: 1 CrossOver-
#7751   NEW    cov: 19 ft: 19 corp: 4/239b lim: 80 exec/s: 0 rss: 187Mb L: 80/80 MS: 3 ChangeBinInt-CMP-ChangeBinInt- DE: "\377\377\377\377"-
#7758   REDUCE cov: 19 ft: 19 corp: 4/235b lim: 80 exec/s: 0 rss: 200Mb L: 76/80 MS: 2 EraseBytes-CopyPart-
#7785   NEW    cov: 20 ft: 20 corp: 5/315b lim: 80 exec/s: 0 rss: 365Mb L: 80/80 MS: 2 ChangeByte-CopyPart-
#7953   REDUCE cov: 20 ft: 20 corp: 5/313b lim: 80 exec/s: 0 rss: 367Mb L: 76/80 MS: 3 EraseBytes-InsertRepeatedBytes-InsertByte-
#7987   REDUCE cov: 29 ft: 29 corp: 6/391b lim: 80 exec/s: 7987 rss: 367Mb L: 78/80 MS: 4 CopyPart-ChangeBit-InsertByte-ShuffleBytes-
#8208   REDUCE cov: 29 ft: 29 corp: 6/390b lim: 80 exec/s: 8208 rss: 367Mb L: 77/80 MS: 1 EraseBytes-
#8224   NEW    cov: 31 ft: 31 corp: 7/470b lim: 80 exec/s: 8224 rss: 367Mb L: 80/80 MS: 1 CrossOver-
#8303   REDUCE cov: 33 ft: 33 corp: 8/549b lim: 80 exec/s: 4151 rss: 367Mb L: 79/80 MS: 4 ShuffleBytes-CMP-ChangeByte-ShuffleBytes- DE: "\001\000"-
#8446   REDUCE cov: 33 ft: 33 corp: 8/546b lim: 80 exec/s: 4223 rss: 367Mb L: 77/80 MS: 3 ShuffleBytes-ChangeByte-EraseBytes-
#8493   NEW    cov: 40 ft: 40 corp: 9/626b lim: 80 exec/s: 4246 rss: 367Mb L: 80/80 MS: 2 CopyPart-CMP- DE: "\011\000\000\000\000\000\000\000"-
#8502   REDUCE cov: 44 ft: 44 corp: 10/706b lim: 80 exec/s: 4251 rss: 367Mb L: 80/80 MS: 4 CopyPart-ChangeByte-ShuffleBytes-CrossOver-
#8527   REDUCE cov: 44 ft: 44 corp: 10/704b lim: 80 exec/s: 4263 rss: 367Mb L: 78/80 MS: 5 ChangeBinInt-CopyPart-ChangeByte-EraseBytes-InsertRepeatedBytes-
#8564   REDUCE cov: 45 ft: 45 corp: 11/781b lim: 80 exec/s: 4282 rss: 367Mb L: 77/80 MS: 2 CopyPart-ChangeBinInt-
#8577   REDUCE cov: 46 ft: 46 corp: 12/858b lim: 80 exec/s: 4288 rss: 367Mb L: 77/80 MS: 3 ChangeByte-CrossOver-PersAutoDict- DE: "\011\000\000\000\000\000\000\000"-
#8709   REDUCE cov: 46 ft: 46 corp: 12/857b lim: 80 exec/s: 4354 rss: 367Mb L: 79/80 MS: 2 EraseBytes-InsertRepeatedBytes-
#8744   NEW    cov: 49 ft: 49 corp: 13/937b lim: 80 exec/s: 2914 rss: 367Mb L: 80/80 MS: 5 ShuffleBytes-PersAutoDict-CrossOver-ChangeByte-ChangeBit- DE: "\001\000"-
#8831   REDUCE cov: 51 ft: 51 corp: 14/1014b lim: 80 exec/s: 2943 rss: 367Mb L: 77/80 MS: 2 ChangeBit-ChangeBinInt-
#8871   REDUCE cov: 51 ft: 51 corp: 14/1011b lim: 80 exec/s: 2957 rss: 367Mb L: 77/80 MS: 5 CrossOver-InsertByte-InsertRepeatedBytes-ShuffleBytes-CrossOver-
#8969   REDUCE cov: 51 ft: 51 corp: 14/1010b lim: 80 exec/s: 2989 rss: 367Mb L: 77/80 MS: 3 EraseBytes-CrossOver-CopyPart-
#9295   NEW    cov: 53 ft: 53 corp: 15/1087b lim: 80 exec/s: 2323 rss: 367Mb L: 77/80 MS: 1 ChangeBinInt-
#9424   REDUCE cov: 55 ft: 55 corp: 16/1167b lim: 80 exec/s: 2356 rss: 367Mb L: 80/80 MS: 4 InsertByte-EraseBytes-InsertRepeatedBytes-InsertRepeatedBytes-
#9483   REDUCE cov: 55 ft: 55 corp: 16/1165b lim: 80 exec/s: 2370 rss: 367Mb L: 77/80 MS: 4 InsertByte-CopyPart-CopyPart-CrossOver-
#9525   REDUCE cov: 55 ft: 55 corp: 16/1164b lim: 80 exec/s: 1905 rss: 367Mb L: 76/80 MS: 2 CrossOver-EraseBytes-
#9873   REDUCE cov: 55 ft: 55 corp: 16/1163b lim: 80 exec/s: 1974 rss: 367Mb L: 76/80 MS: 3 EraseBytes-PersAutoDict-InsertByte- DE: "\377\377\377\377"-
#9878   REDUCE cov: 55 ft: 55 corp: 16/1159b lim: 80 exec/s: 1975 rss: 367Mb L: 76/80 MS: 5 ChangeBit-ChangeBinInt-PersAutoDict-CopyPart-EraseBytes- DE: "\011\000\000\000\000\000\000\000"-
#10102  REDUCE cov: 55 ft: 55 corp: 16/1158b lim: 80 exec/s: 1683 rss: 367Mb L: 76/80 MS: 4 PersAutoDict-EraseBytes-CrossOver-InsertByte- DE: "\001\000"-
#10109  REDUCE cov: 56 ft: 56 corp: 17/1235b lim: 80 exec/s: 1684 rss: 367Mb L: 77/80 MS: 2 ChangeByte-InsertByte-
#10393  REDUCE cov: 56 ft: 56 corp: 17/1234b lim: 80 exec/s: 1484 rss: 367Mb L: 76/80 MS: 4 EraseBytes-ShuffleBytes-CrossOver-InsertByte-
#10499  REDUCE cov: 56 ft: 56 corp: 17/1233b lim: 80 exec/s: 1499 rss: 367Mb L: 79/80 MS: 1 EraseBytes-
#10704  REDUCE cov: 56 ft: 56 corp: 17/1230b lim: 80 exec/s: 1529 rss: 367Mb L: 76/80 MS: 5 EraseBytes-CMP-CrossOver-ChangeBit-InsertByte- DE: "\377\377X\365.8\2408"-
#11003  REDUCE cov: 56 ft: 56 corp: 17/1229b lim: 80 exec/s: 1375 rss: 367Mb L: 76/80 MS: 4 EraseBytes-ChangeByte-ChangeBit-CrossOver-
#11613  REDUCE cov: 56 ft: 59 corp: 18/1313b lim: 86 exec/s: 1290 rss: 367Mb L: 84/84 MS: 5 CopyPart-InsertRepeatedBytes-ChangeBinInt-PersAutoDict-InsertByte- DE: "\377\377X\365.8\2408"-
#11683  REDUCE cov: 56 ft: 59 corp: 18/1312b lim: 86 exec/s: 1168 rss: 367Mb L: 76/84 MS: 5 CopyPart-EraseBytes-CMP-CrossOver-InsertRepeatedBytes- DE: "\377\377"-
#11847  REDUCE cov: 57 ft: 60 corp: 19/1395b lim: 86 exec/s: 1184 rss: 367Mb L: 83/84 MS: 4 CopyPart-ChangeBit-CrossOver-ChangeBit-
#13253  REDUCE cov: 57 ft: 60 corp: 19/1392b lim: 98 exec/s: 946 rss: 367Mb L: 80/84 MS: 1 EraseBytes-
#13655  REDUCE cov: 57 ft: 60 corp: 19/1391b lim: 98 exec/s: 910 rss: 367Mb L: 78/84 MS: 2 ChangeBinInt-EraseBytes-
#13772  REDUCE cov: 57 ft: 60 corp: 19/1389b lim: 98 exec/s: 918 rss: 367Mb L: 78/84 MS: 2 ShuffleBytes-EraseBytes-
#13871  REDUCE cov: 57 ft: 60 corp: 19/1387b lim: 98 exec/s: 924 rss: 367Mb L: 76/84 MS: 4 CopyPart-ChangeBit-CMP-EraseBytes- DE: "\377\377"-
#16384  pulse  cov: 57 ft: 60 corp: 19/1387b lim: 122 exec/s: 712 rss: 367Mb
#18334  REDUCE cov: 57 ft: 60 corp: 19/1385b lim: 142 exec/s: 654 rss: 367Mb L: 76/84 MS: 3 EraseBytes-CopyPart-CrossOver-
#18710  REDUCE cov: 57 ft: 60 corp: 19/1384b lim: 142 exec/s: 645 rss: 367Mb L: 76/84 MS: 1 EraseBytes-
#18716  REDUCE cov: 57 ft: 60 corp: 19/1383b lim: 142 exec/s: 645 rss: 367Mb L: 76/84 MS: 1 EraseBytes-
#20245  REDUCE cov: 57 ft: 60 corp: 19/1382b lim: 156 exec/s: 595 rss: 367Mb L: 76/84 MS: 4 CrossOver-EraseBytes-EraseBytes-InsertRepeatedBytes-
#20367  REDUCE cov: 57 ft: 60 corp: 19/1381b lim: 156 exec/s: 599 rss: 367Mb L: 76/84 MS: 2 CopyPart-EraseBytes-
AddressSanitizer:DEADLYSIGNAL
=================================================================
==16824==ERROR: AddressSanitizer: SEGV on unknown address 0x00004c000038 (pc 0x59f52e2b5530 bp 0x00004c000000 sp 0x7ffec7062b38 T0)
==16824==The signal is caused by a READ memory access.
    #0 0x59f52e2b5530 in image2yuvconfig /mnt/h/Security/Binary/Reports/fuzz_vuln/libvpx/libvpx/build/../vp9/vp9_iface_common.c:80:18
    #1 0x59f52e2653c6 in ctrl_set_reference /mnt/h/Security/Binary/Reports/fuzz_vuln/libvpx/libvpx/build/../vp9/vp9_dx_iface.c:457:5
    #2 0x59f52e202caf in vpx_codec_control_ /mnt/h/Security/Binary/Reports/fuzz_vuln/libvpx/libvpx/build/../vpx/src/vpx_codec.c:106:15
    #3 0x59f52e2010e9 in LLVMFuzzerTestOneInput /mnt/h/Security/Binary/Reports/fuzz_vuln/libvpx/vpx_codec_control_fuzzer.c:237:13
    #4 0x59f52e104eba in fuzzer::Fuzzer::ExecuteCallback(unsigned char const*, unsigned long) (/mnt/h/Security/Binary/Reports/fuzz_vuln/libvpx/fuzzers/vpx_codec_control_fuzzer_vp9+0x55eba) (BuildId: bdc74c98eb19f96060df35b6fdce7b4eef9857ae)
    #5 0x59f52e1044c9 in fuzzer::Fuzzer::RunOne(unsigned char const*, unsigned long, bool, fuzzer::InputInfo*, bool, bool*) (/mnt/h/Security/Binary/Reports/fuzz_vuln/libvpx/fuzzers/vpx_codec_control_fuzzer_vp9+0x554c9) (BuildId: bdc74c98eb19f96060df35b6fdce7b4eef9857ae)
    #6 0x59f52e105f95 in fuzzer::Fuzzer::MutateAndTestOne() (/mnt/h/Security/Binary/Reports/fuzz_vuln/libvpx/fuzzers/vpx_codec_control_fuzzer_vp9+0x56f95) (BuildId: bdc74c98eb19f96060df35b6fdce7b4eef9857ae)
    #7 0x59f52e106a85 in fuzzer::Fuzzer::Loop(std::vector<fuzzer::SizedFile, std::allocator<fuzzer::SizedFile>>&) (/mnt/h/Security/Binary/Reports/fuzz_vuln/libvpx/fuzzers/vpx_codec_control_fuzzer_vp9+0x57a85) (BuildId: bdc74c98eb19f96060df35b6fdce7b4eef9857ae)
    #8 0x59f52e0f3045 in fuzzer::FuzzerDriver(int*, char***, int (*)(unsigned char const*, unsigned long)) (/mnt/h/Security/Binary/Reports/fuzz_vuln/libvpx/fuzzers/vpx_codec_control_fuzzer_vp9+0x44045) (BuildId: bdc74c98eb19f96060df35b6fdce7b4eef9857ae)
    #9 0x59f52e11f3a6 in main (/mnt/h/Security/Binary/Reports/fuzz_vuln/libvpx/fuzzers/vpx_codec_control_fuzzer_vp9+0x703a6) (BuildId: bdc74c98eb19f96060df35b6fdce7b4eef9857ae)
    #10 0x7316a6806ca7 in __libc_start_call_main csu/../sysdeps/nptl/libc_start_call_main.h:58:16
    #11 0x7316a6806d64 in __libc_start_main csu/../csu/libc-start.c:360:3
    #12 0x59f52e0e73a0 in _start (/mnt/h/Security/Binary/Reports/fuzz_vuln/libvpx/fuzzers/vpx_codec_control_fuzzer_vp9+0x383a0) (BuildId: bdc74c98eb19f96060df35b6fdce7b4eef9857ae)

==16824==Register values:
rax = 0x0000000000000018  rbx = 0x00005160005e2690  rcx = 0x0000000000000000  rdx = 0x00007ffec7062c30
rdi = 0x000000004c000008  rsi = 0x00007ffec7062b40  rbp = 0x000000004c000000  rsp = 0x00007ffec7062b38
 r8 = 0x000059f52e3ea200   r9 = 0x0000000000000000  r10 = 0x0000000000000004  r11 = 0x0000000000000000
r12 = 0x00007ffec7062b40  r13 = 0x00007316a4c35c00  r14 = 0x0000000000000001  r15 = 0x00000e635497eb80
AddressSanitizer can not provide additional info.
SUMMARY: AddressSanitizer: SEGV /mnt/h/Security/Binary/Reports/fuzz_vuln/libvpx/libvpx/build/../vp9/vp9_iface_common.c:80:18 in image2yuvconfig
==16824==ABORTING
MS: 3 CopyPart-CrossOver-PersAutoDict- DE: "\001\000\000\000\000\000\000L"-; base unit: 05fa2e75d1612c5270d65fa7ee3c2e92599ab181
0x0,0x0,0xff,0xff,0x1,0x0,0x0,0x0,0x0,0x0,0x0,0x4c,0xff,0xff,0x0,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x3a,0xff,0x8,0x48,0x8,0x8,0x8,0x8,0x8,0x8,0xff,0xff,0x58,0xf5,0x2e,0x38,0xa0,0x38,0x8,0x2e,0x38,0xa0,0x38,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0xff,0xff,0xff,0xff,0xff,0x8a,
\000\000\377\377\001\000\000\000\000\000\000L\377\377\000\377\377\377\377\377\377\377:\377\010H\010\010\010\010\010\010\377\377X\365.8\2408\010.8\2408\010\010\010\010\010\010\010\010\010\010\010\010\010\010\010\010\010\010\010\010\000\000\000\000\000\000\000\000\010\010\010\010\010\010\010\010\010\010\010\010\010\010\010\377\377\377\377\377\212
artifact_prefix='./'; Test unit written to ./crash-33724cd6b286c15eb766589eeaaea79e90ff0691
Base64: AAD//wEAAAAAAABM//8A/////////zr/CEgICAgICAj//1j1LjigOAguOKA4CAgICAgICAgICAgICAgICAgICAgAAAAAAAAAAAgICAgICAgICAgICAgICP//////ig==
```

