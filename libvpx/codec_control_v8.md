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

**Variant**: `vpx_codec_control_fuzzer_vp8` – VP8-specific

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
 * $CC -fsanitize=fuzzer,address -DENCODER=vp8 -DDECODER=vp8 \
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

https://github.com/nanhang-950/fuzz_vuln/blob/main/libvpx/crash-d3a1ed54d420320f1049ec89a1cd4e850b4f7142

## ASAN Info

```
❯ ./vpx_codec_control_fuzzer_vp8 ./crash-d3a1ed54d420320f1049ec89a1cd4e850b4f7142
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

