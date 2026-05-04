## project address

project：https://android.googlesource.com/platform/external/libaom

version：v3.12.0 

## info

OS：Ubuntu22.04 TLS

Build:

```shell
# deps: git cmake make clang clang++

WORK_ROOT=/tmp/libaom_v3120
BUILD_DIR=${WORK_ROOT}/build-clang
COMMON_FLAGS="-UNDEBUG -DDO_RANGE_CHECK_CLAMP=1 -DAOM_MAX_ALLOCABLE_MEMORY=1073741824"

git clone --branch v3.12.0 --depth 1 https://aomedia.googlesource.com/aom "${WORK_ROOT}"
cp ./av1_enc_fuzzer.cc "${WORK_ROOT}/examples/av1_enc_fuzzer.cc"
mkdir -p "${BUILD_DIR}"

cmake -S "${WORK_ROOT}" -B "${BUILD_DIR}" \
  -DCMAKE_C_COMPILER="$(command -v clang)" \
  -DCMAKE_CXX_COMPILER="$(command -v clang++)" \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCONFIG_PIC=1 \
  -DCONFIG_AV1_DECODER=0 \
  -DENABLE_EXAMPLES=0 \
  -DENABLE_DOCS=0 \
  -DENABLE_TESTS=0 \
  -DCONFIG_SIZE_LIMIT=1 \
  -DDECODE_HEIGHT_LIMIT=12288 \
  -DDECODE_WIDTH_LIMIT=12288 \
  -DAOM_EXTRA_C_FLAGS="${COMMON_FLAGS}" \
  -DAOM_EXTRA_CXX_FLAGS="${COMMON_FLAGS}" \
  -DSANITIZE=fuzzer-no-link,address

make -C "${BUILD_DIR}" -j"$(nproc)"

clang++ -std=c++11 -I"${WORK_ROOT}" -I"${BUILD_DIR}" \
  -g -fsanitize=fuzzer,address \
  "${WORK_ROOT}/examples/av1_enc_fuzzer.cc" \
  -o "${BUILD_DIR}/av1_enc_fuzzer" \
  "${BUILD_DIR}/libaom.a" -lpthread -lm
```

## fuzzer

**Objective**: AV1 Encoder Public API and Mid-stream Reconfiguration

**Test Content**:

- `aom_codec_enc_init()` initialization under different usage modes
- Encoder control configuration via `AOM_CODEC_CONTROL_TYPECHECKED`
- Single-frame and two-frame encode flows
- Resolution reconfiguration via `aom_codec_enc_config_set()`
- Row-mt and all-intra encoder paths
- Packet draining and teardown via `aom_codec_get_cx_data()` / `aom_codec_destroy()`

**Input Format**:

- First 16 bytes: mode flags, encoder controls, dimensions, bitrate, and thread counts
- Remaining data: one or two I420 frames

**Variant**: `av1_enc_fuzzer`

```c++
/*
 * Copyright (c) 2026, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

/*
 * LibFuzzer harness for the AV1 encoder public API.
 *
 * Targeted path:
 *   aom_codec_enc_init() -> aom_codec_enc_config_set() -> aom_codec_encode()
 *   -> aom_codec_get_cx_data() -> aom_codec_destroy()
 *
 * See build_av1_dec_fuzzer.sh for the general libaom fuzzing build pattern.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "config/aom_config.h"
#include "aom/aom_encoder.h"
#include "aom/aom_image.h"
#include "aom/aomcx.h"

namespace {

constexpr unsigned int kMaxDimension = 1024;
constexpr size_t kMinHeaderSize = 16;

struct FuzzReader {
  const uint8_t *data;
  size_t size;
};

static aom_codec_iface_t *g_iface = nullptr;
static aom_codec_enc_cfg_t g_cfg_templates[3];
static bool g_have_template[3] = { false, false, false };

static uint8_t ReadU8(FuzzReader *reader) {
  if (reader->size == 0) return 0;
  const uint8_t value = *reader->data++;
  --reader->size;
  return value;
}

static uint16_t ReadU16(FuzzReader *reader) {
  if (reader->size < 2) return 0;
  const uint16_t value = (uint16_t)reader->data[0] |
                         (uint16_t)(reader->data[1] << 8);
  reader->data += 2;
  reader->size -= 2;
  return value;
}

static unsigned int UsageToIndex(unsigned int usage) {
  switch (usage) {
    case AOM_USAGE_GOOD_QUALITY: return 0;
    case AOM_USAGE_REALTIME: return 1;
    case AOM_USAGE_ALL_INTRA: return 2;
    default: return 0;
  }
}

static unsigned int PickUsage(uint8_t raw) {
  switch (raw % 3) {
    case 0: return AOM_USAGE_GOOD_QUALITY;
    case 1: return AOM_USAGE_REALTIME;
    default: return AOM_USAGE_ALL_INTRA;
  }
}

static aom_rc_mode PickRcMode(uint8_t raw) {
  switch (raw % 4) {
    case 0: return AOM_VBR;
    case 1: return AOM_CBR;
    case 2: return AOM_CQ;
    default: return AOM_Q;
  }
}

static unsigned int PickDimension(uint16_t raw) {
  return 1u + (raw % kMaxDimension);
}

static int PickCpuUsed(unsigned int usage, uint8_t raw) {
  const int max_cpu_used = usage == AOM_USAGE_REALTIME ? 11 : 9;
  return raw % (max_cpu_used + 1);
}

static void DrainPackets(aom_codec_ctx_t *codec) {
  aom_codec_iter_t iter = nullptr;
  while (aom_codec_get_cx_data(codec, &iter) != nullptr) {
  }
}

static bool InitDefaultConfig(unsigned int usage, aom_codec_enc_cfg_t *cfg) {
  const unsigned int requested_index = UsageToIndex(usage);
  if (g_have_template[requested_index]) {
    *cfg = g_cfg_templates[requested_index];
    return true;
  }

  for (unsigned int i = 0; i < 3; ++i) {
    if (g_have_template[i]) {
      *cfg = g_cfg_templates[i];
      return true;
    }
  }
  return false;
}

static size_t GetI420FrameSize(unsigned int width, unsigned int height) {
  const size_t y_plane = (size_t)width * height;
  const size_t uv_width = (size_t)(width + 1) / 2;
  const size_t uv_height = (size_t)(height + 1) / 2;
  const size_t uv_plane = uv_width * uv_height;
  return y_plane + uv_plane * 2;
}

static void CopyPlane(uint8_t *dst, int dst_stride, const uint8_t *src,
                      unsigned int width, unsigned int height) {
  for (unsigned int row = 0; row < height; ++row) {
    memcpy(dst + (size_t)row * dst_stride, src + (size_t)row * width, width);
  }
}

static bool BuildImage(FuzzReader *reader, unsigned int width,
                       unsigned int height, aom_image_t *image) {
  const size_t frame_size = GetI420FrameSize(width, height);
  if (frame_size == 0 || reader->size < frame_size) return false;

  if (aom_img_alloc(image, AOM_IMG_FMT_I420, width, height, 1) == nullptr) {
    return false;
  }

  memset(image->img_data, 0, image->sz);

  const size_t y_plane = (size_t)width * height;
  const unsigned int uv_width = (width + 1) / 2;
  const unsigned int uv_height = (height + 1) / 2;
  const size_t uv_plane = (size_t)uv_width * uv_height;

  const uint8_t *src_y = reader->data;
  const uint8_t *src_u = src_y + y_plane;
  const uint8_t *src_v = src_u + uv_plane;

  CopyPlane(image->planes[AOM_PLANE_Y], image->stride[AOM_PLANE_Y], src_y,
            width, height);
  CopyPlane(image->planes[AOM_PLANE_U], image->stride[AOM_PLANE_U], src_u,
            uv_width, uv_height);
  CopyPlane(image->planes[AOM_PLANE_V], image->stride[AOM_PLANE_V], src_v,
            uv_width, uv_height);

  reader->data += frame_size;
  reader->size -= frame_size;
  return true;
}

static void ApplyControls(aom_codec_ctx_t *codec, unsigned int usage,
                          uint8_t mode_flags, uint8_t cpu_used_raw,
                          uint8_t ctl0, uint8_t ctl1) {
  const int cpu_used = PickCpuUsed(usage, cpu_used_raw);
  const unsigned int lossless = (mode_flags >> 2) & 1;
  const unsigned int row_mt = (mode_flags >> 3) & 1;
  const unsigned int aq_mode = ctl0 % 4;
  const unsigned int deltaq_mode = ctl1 % 4;
  const unsigned int tile_columns = (ctl0 >> 4) & 0x3;
  const unsigned int tile_rows = (ctl1 >> 4) & 0x3;
  const unsigned int enable_cdef = (ctl0 >> 2) & 1;
  const unsigned int enable_restoration = (ctl1 >> 2) & 1;

  (void)AOM_CODEC_CONTROL_TYPECHECKED(codec, AOME_SET_CPUUSED, cpu_used);
  (void)AOM_CODEC_CONTROL_TYPECHECKED(codec, AV1E_SET_LOSSLESS, lossless);
  (void)AOM_CODEC_CONTROL_TYPECHECKED(codec, AV1E_SET_ROW_MT, row_mt);
  (void)AOM_CODEC_CONTROL_TYPECHECKED(codec, AV1E_SET_AQ_MODE, aq_mode);
  (void)AOM_CODEC_CONTROL_TYPECHECKED(codec, AV1E_SET_DELTAQ_MODE,
                                      deltaq_mode);
  (void)AOM_CODEC_CONTROL_TYPECHECKED(codec, AV1E_SET_TILE_COLUMNS,
                                      tile_columns);
  (void)AOM_CODEC_CONTROL_TYPECHECKED(codec, AV1E_SET_TILE_ROWS, tile_rows);
  (void)AOM_CODEC_CONTROL_TYPECHECKED(codec, AV1E_SET_ENABLE_CDEF,
                                      enable_cdef);
  (void)AOM_CODEC_CONTROL_TYPECHECKED(codec, AV1E_SET_ENABLE_RESTORATION,
                                      enable_restoration);
}

static bool EncodeFrame(aom_codec_ctx_t *codec, FuzzReader *reader,
                        unsigned int width, unsigned int height,
                        aom_codec_pts_t pts,
                        aom_enc_frame_flags_t flags) {
  aom_image_t image;
  memset(&image, 0, sizeof(image));

  if (!BuildImage(reader, width, height, &image)) {
    return false;
  }

  (void)aom_codec_encode(codec, &image, pts, 1, flags);
  DrainPackets(codec);

  aom_img_free(&image);
  return true;
}

__attribute__((constructor)) static void InitEncoderTemplates(void) {
  g_iface = aom_codec_av1_cx();
  if (g_iface == nullptr) return;

  const unsigned int usages[3] = { AOM_USAGE_GOOD_QUALITY, AOM_USAGE_REALTIME,
                                   AOM_USAGE_ALL_INTRA };
  for (unsigned int i = 0; i < 3; ++i) {
    g_have_template[i] =
        aom_codec_enc_config_default(g_iface, &g_cfg_templates[i], usages[i]) ==
        AOM_CODEC_OK;
  }
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  if (g_iface == nullptr || size < kMinHeaderSize) return 0;

  FuzzReader reader = { data, size };
  const uint8_t mode_flags = ReadU8(&reader);
  const uint8_t cpu_used_raw = ReadU8(&reader);
  const uint8_t ctl0 = ReadU8(&reader);
  const uint8_t ctl1 = ReadU8(&reader);
  const unsigned int usage = PickUsage(mode_flags);
  const unsigned int width0 = PickDimension(ReadU16(&reader));
  const unsigned int height0 = PickDimension(ReadU16(&reader));
  const unsigned int bitrate = 1u + (ReadU16(&reader) % 4000u);
  const unsigned int width1 = PickDimension(ReadU16(&reader));
  const unsigned int height1 = PickDimension(ReadU16(&reader));
  const unsigned int threads0 = ReadU8(&reader) % 9;
  const unsigned int threads1 = ReadU8(&reader) % 9;

  aom_codec_enc_cfg_t cfg;
  if (!InitDefaultConfig(usage, &cfg)) return 0;

  cfg.g_usage = usage;
  cfg.g_w = width0;
  cfg.g_h = height0;
  cfg.g_threads = threads0;
  cfg.g_forced_max_frame_width = kMaxDimension;
  cfg.g_forced_max_frame_height = kMaxDimension;
  cfg.g_timebase.num = 1;
  cfg.g_timebase.den = 1000000;
  cfg.g_pass = AOM_RC_ONE_PASS;
  cfg.g_lag_in_frames = 0;
  cfg.g_error_resilient =
      (mode_flags & 0x80) ? AOM_ERROR_RESILIENT_DEFAULT : 0;
  cfg.rc_end_usage = PickRcMode(ctl0);
  cfg.rc_target_bitrate = bitrate;

  aom_codec_ctx_t codec;
  memset(&codec, 0, sizeof(codec));
  if (aom_codec_enc_init(&codec, g_iface, &cfg, 0) != AOM_CODEC_OK) return 0;

  ApplyControls(&codec, usage, mode_flags, cpu_used_raw, ctl0, ctl1);

  const aom_enc_frame_flags_t first_flags =
      (mode_flags & 0x20) ? AOM_EFLAG_FORCE_KF : 0;
  if (!EncodeFrame(&codec, &reader, width0, height0, 0, first_flags)) {
    (void)aom_codec_destroy(&codec);
    return 0;
  }

  if (mode_flags & 0x10) {
    cfg.g_w = width1;
    cfg.g_h = height1;
    cfg.g_threads = threads1;
    cfg.rc_end_usage = PickRcMode(ctl1);
    if (aom_codec_enc_config_set(&codec, &cfg) == AOM_CODEC_OK) {
      const aom_enc_frame_flags_t second_flags =
          (mode_flags & 0x40) ? AOM_EFLAG_FORCE_KF : 0;
      (void)EncodeFrame(&codec, &reader, width1, height1, 1, second_flags);
    }
  }

  for (int flush_round = 0; flush_round < 8; ++flush_round) {
    if (aom_codec_encode(&codec, nullptr, 0, 0, 0) != AOM_CODEC_OK) break;
    aom_codec_iter_t iter = nullptr;
    bool got_packet = false;
    while (aom_codec_get_cx_data(&codec, &iter) != nullptr) {
      got_packet = true;
    }
    if (!got_packet) break;
  }

  (void)aom_codec_destroy(&codec);
  return 0;
}
```

## Poc

local artifact：`minimized-from-fe624f98b204c3f6d2e510759941f71406be4e5d`

## ASAN Info

```text
❯ ./av1_enc_fuzzer ./crash-fe624f98b204c3f6d2e510759941f71406be4e5d
INFO: Running with entropic power schedule (0xFF, 100).
INFO: Seed: 107676128
INFO: Loaded 1 modules   (85719 inline 8-bit counters): 85719 [0x617ad0d965b0, 0x617ad0dab487),
INFO: Loaded 1 PC tables (85719 PCs): 85719 [0x617ad0dab488,0x617ad0efa1f8),
./av1_enc_fuzzer: Running 1 inputs 1 time(s) each.
Running: ./crash-fe624f98b204c3f6d2e510759941f71406be4e5d
=================================================================
==27615==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x51e000000a4a at pc 0x617acfabc834 bp 0x76d883178bd0 sp 0x76d883178bc8
WRITE of size 2 at 0x51e000000a4a thread T4
    #0 0x617acfabc833 in av1_calc_mb_wiener_var_row /mnt/h/Security/Reports/fuzz_vuln/libaom/av1/encoder/allintra_vis.c:401:30
    #1 0x617acfbf6a16 in cal_mb_wiener_var_hook /mnt/h/Security/Reports/fuzz_vuln/libaom/av1/encoder/ethread.c:2829:5
    #2 0x617ad06155c8 in execute /mnt/h/Security/Reports/fuzz_vuln/libaom/aom_util/aom_thread.c:197:27
    #3 0x617ad06155c8 in thread_loop /mnt/h/Security/Reports/fuzz_vuln/libaom/aom_util/aom_thread.c:82:7
    #4 0x617acfa3b21a in asan_thread_start(void*) asan_interceptors.cpp.o
    #5 0x76d889a6fb7a in start_thread nptl/pthread_create.c:448:8
    #6 0x76d889aed7b7 in __GI___clone3 misc/../sysdeps/unix/sysv/linux/x86_64/clone3.S:78

0x51e000000a4a is located 19 bytes after 2487-byte region [0x51e000000080,0x51e000000a37)
allocated by thread T0 here:
    #0 0x617acfa3d8c3 in malloc (/mnt/h/Security/漏洞挖掘/fuzz_vuln/libaom/av1_enc_fuzzer+0x3948c3) (BuildId: f2935cb7e63b10152097404bf4e8acb9d33d78d8)
    #1 0x617acfa87c64 in aom_memalign /mnt/h/Security/Reports/fuzz_vuln/libaom/aom_mem/aom_mem.c:59:22
    #2 0x617acfa87c64 in aom_malloc /mnt/h/Security/Reports/fuzz_vuln/libaom/aom_mem/aom_mem.c:67:40
    #3 0x617acfa87c64 in aom_calloc /mnt/h/Security/Reports/fuzz_vuln/libaom/aom_mem/aom_mem.c:72:19
    #4 0x617acfab8b74 in av1_init_mb_wiener_var_buffer /mnt/h/Security/Reports/fuzz_vuln/libaom/av1/encoder/allintra_vis.c:76:3
    #5 0x617acfb7e426 in encode_frame_to_data_rate /mnt/h/Security/Reports/fuzz_vuln/libaom/av1/encoder/encoder.c:3806:5
    #6 0x617acfb7e426 in av1_encode /mnt/h/Security/Reports/fuzz_vuln/libaom/av1/encoder/encoder.c:4053:9
    #7 0x617ad078bfda in denoise_and_encode /mnt/h/Security/Reports/fuzz_vuln/libaom/av1/encoder/encode_strategy.c:904:7
    #8 0x617ad078bfda in av1_encode_strategy /mnt/h/Security/Reports/fuzz_vuln/libaom/av1/encoder/encode_strategy.c:1687:14
    #9 0x617acfb899ff in av1_get_compressed_data /mnt/h/Security/Reports/fuzz_vuln/libaom/av1/encoder/encoder.c:4779:22
    #10 0x617acfa95e6d in encoder_encode /mnt/h/Security/Reports/fuzz_vuln/libaom/av1/av1_cx_iface.c:3444:20
    #11 0x617acfa8449f in aom_codec_encode /mnt/h/Security/Reports/fuzz_vuln/libaom/aom/src/aom_encoder.c:191:11
    #12 0x617acfa82652 in (anonymous namespace)::EncodeFrame(aom_codec_ctx*, (anonymous namespace)::FuzzReader*, unsigned int, unsigned int, long, long) /mnt/h/Security/Reports/fuzz_vuln/libaom/examples/build-fuzzers/../av1_enc_fuzzer.cc:205:9
    #13 0x617acfa81cc9 in LLVMFuzzerTestOneInput /mnt/h/Security/Reports/fuzz_vuln/libaom/examples/build-fuzzers/../av1_enc_fuzzer.cc:270:8
    #14 0x617acf98366a in fuzzer::Fuzzer::ExecuteCallback(unsigned char const*, unsigned long) (/mnt/h/Security/漏洞挖掘/fuzz_vuln/libaom/av1_enc_fuzzer+0x2da66a) (BuildId: f2935cb7e63b10152097404bf4e8acb9d33d78d8)
    #15 0x617acf96b563 in fuzzer::RunOneTest(fuzzer::Fuzzer*, char const*, unsigned long) (/mnt/h/Security/漏洞挖掘/fuzz_vuln/libaom/av1_enc_fuzzer+0x2c2563) (BuildId: f2935cb7e63b10152097404bf4e8acb9d33d78d8)
    #16 0x617acf9716a1 in fuzzer::FuzzerDriver(int*, char***, int (*)(unsigned char const*, unsigned long)) (/mnt/h/Security/漏洞挖掘/fuzz_vuln/libaom/av1_enc_fuzzer+0x2c86a1) (BuildId: f2935cb7e63b10152097404bf4e8acb9d33d78d8)
    #17 0x617acf99db56 in main (/mnt/h/Security/漏洞挖掘/fuzz_vuln/libaom/av1_enc_fuzzer+0x2f4b56) (BuildId: f2935cb7e63b10152097404bf4e8acb9d33d78d8)
    #18 0x76d889a06ca7 in __libc_start_call_main csu/../sysdeps/nptl/libc_start_call_main.h:58:16

Thread T4 created by T0 here:
    #0 0x617acfa22dd5 in pthread_create (/mnt/h/Security/漏洞挖掘/fuzz_vuln/libaom/av1_enc_fuzzer+0x379dd5) (BuildId: f2935cb7e63b10152097404bf4e8acb9d33d78d8)
    #1 0x617ad06148f3 in reset /mnt/h/Security/Reports/fuzz_vuln/libaom/aom_util/aom_thread.c:172:11
    #2 0x617acfbda697 in av1_create_workers /mnt/h/Security/Reports/fuzz_vuln/libaom/av1/encoder/ethread.c:1105:12
    #3 0x617acfa95568 in encoder_encode /mnt/h/Security/Reports/fuzz_vuln/libaom/av1/av1_cx_iface.c:3363:7
    #4 0x617acfa8449f in aom_codec_encode /mnt/h/Security/Reports/fuzz_vuln/libaom/aom/src/aom_encoder.c:191:11
    #5 0x617acfa82652 in (anonymous namespace)::EncodeFrame(aom_codec_ctx*, (anonymous namespace)::FuzzReader*, unsigned int, unsigned int, long, long) /mnt/h/Security/Reports/fuzz_vuln/libaom/examples/build-fuzzers/../av1_enc_fuzzer.cc:205:9
    #6 0x617acfa81d97 in LLVMFuzzerTestOneInput /mnt/h/Security/Reports/fuzz_vuln/libaom/examples/build-fuzzers/../av1_enc_fuzzer.cc:283:13
    #7 0x617acf98366a in fuzzer::Fuzzer::ExecuteCallback(unsigned char const*, unsigned long) (/mnt/h/Security/漏洞挖掘/fuzz_vuln/libaom/av1_enc_fuzzer+0x2da66a) (BuildId: f2935cb7e63b10152097404bf4e8acb9d33d78d8)
    #8 0x617acf96b563 in fuzzer::RunOneTest(fuzzer::Fuzzer*, char const*, unsigned long) (/mnt/h/Security/漏洞挖掘/fuzz_vuln/libaom/av1_enc_fuzzer+0x2c2563) (BuildId: f2935cb7e63b10152097404bf4e8acb9d33d78d8)
    #9 0x617acf9716a1 in fuzzer::FuzzerDriver(int*, char***, int (*)(unsigned char const*, unsigned long)) (/mnt/h/Security/漏洞挖掘/fuzz_vuln/libaom/av1_enc_fuzzer+0x2c86a1) (BuildId: f2935cb7e63b10152097404bf4e8acb9d33d78d8)
    #10 0x617acf99db56 in main (/mnt/h/Security/漏洞挖掘/fuzz_vuln/libaom/av1_enc_fuzzer+0x2f4b56) (BuildId: f2935cb7e63b10152097404bf4e8acb9d33d78d8)
    #11 0x76d889a06ca7 in __libc_start_call_main csu/../sysdeps/nptl/libc_start_call_main.h:58:16

SUMMARY: AddressSanitizer: heap-buffer-overflow /mnt/h/Security/Reports/fuzz_vuln/libaom/av1/encoder/allintra_vis.c:401:30 in av1_calc_mb_wiener_var_row
Shadow bytes around the buggy address:
  0x51e000000780: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x51e000000800: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x51e000000880: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x51e000000900: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x51e000000980: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
=>0x51e000000a00: 00 00 00 00 00 00 07 fa fa[fa]fa fa fa fa fa fa
  0x51e000000a80: fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa
  0x51e000000b00: fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa
  0x51e000000b80: fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa
  0x51e000000c00: fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa
  0x51e000000c80: fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa
Shadow byte legend (one shadow byte represents 8 application bytes):
  Addressable:           00
  Partially addressable: 01 02 03 04 05 06 07
  Heap left redzone:       fa
  Freed heap region:       fd
  Stack left redzone:      f1
  Stack mid redzone:       f2
  Stack right redzone:     f3
  Stack after return:      f5
  Stack use after scope:   f8
  Global redzone:          f9
  Global init order:       f6
  Poisoned by user:        f7
  Container overflow:      fc
  Array cookie:            ac
  Intra object redzone:    bb
  ASan internal:           fe
  Left alloca redzone:     ca
  Right alloca redzone:    cb
==27615==ABORTING
```

## Crash Stack

```text
#0  av1_calc_mb_wiener_var_row
    /mnt/h/Security/Reports/fuzz_vuln/libaom/av1/encoder/allintra_vis.c:401
#1  cal_mb_wiener_var_hook
    /mnt/h/Security/Reports/fuzz_vuln/libaom/av1/encoder/ethread.c:2829
#2  execute
    /mnt/h/Security/Reports/fuzz_vuln/libaom/aom_util/aom_thread.c:197
#3  thread_loop
    /mnt/h/Security/Reports/fuzz_vuln/libaom/aom_util/aom_thread.c:82
```

Allocation Stack:

```text
#0  aom_calloc
#1  av1_init_mb_wiener_var_buffer
    /mnt/h/Security/Reports/fuzz_vuln/libaom/av1/encoder/allintra_vis.c:76
#2  encode_frame_to_data_rate
    /mnt/h/Security/Reports/fuzz_vuln/libaom/av1/encoder/encoder.c:3806
```

## PoC Trigger Conditions

According to the harness header parsing logic at:
`/mnt/h/Security/Reports/fuzz_vuln/libaom/examples/av1_enc_fuzzer.cc`:

- `mode_flags = 0x9e`
- `ctl1 = 0x07`
- Second frame reconfiguration enabled: `mode_flags & 0x10`
- `usage = AOM_USAGE_ALL_INTRA`
- `row_mt = 1`
- `deltaq_mode = 3`, i.e., `DELTA_Q_PERCEPTUAL_AI`

Decoding the first 16 bytes of the current PoC yields:

- First frame dimensions: `82 x 1`
- Second frame dimensions: `1 x 769`
- Second frame threads: `4`

That is, the PoC first initializes the Wiener statistics buffer with a small frame size,
then changes the dimensions to another geometry for the second frame,
and triggers concurrent writes to the old buffer under the row-mt path.

## Root Cause Analysis

### 1. Wiener Statistics Buffer Is Only Allocated When the Pointer Is Null

The logic in `av1_init_mb_wiener_var_buffer()` is:

- If `cpi->mb_weber_stats` is non-null, return directly
- No check whether the current frame dimensions have changed

Relevant location: `av1/encoder/allintra_vis.c:68-78`

This means that once allocated the first time, even if the resolution changes later,
it will not be reallocated.

### 2. Allocation Size Depends on `cpi->frame_info`

The allocation uses:

```c
aom_calloc(cpi->frame_info.mi_rows * cpi->frame_info.mi_cols, ...)
```

Relevant location: `av1/encoder/allintra_vis.c:76-88`

### 3. Write Phase Uses the Current Encoding State

In `av1_calc_mb_wiener_var_row()`, the write index is:

```c
&cpi->mb_weber_stats[(mi_row / mb_step) * cpi->frame_info.mi_cols +
                     (mi_col / mb_step)]
```

Relevant location: `av1/encoder/allintra_vis.c:397-405`

Meanwhile, the row-mt task dispatch uses the current:

- `cpi->common.mi_params.mi_rows`

Relevant location: `av1/encoder/ethread.c:2821-2823`

Therefore, after the second frame reconfiguration, while thread scheduling and encoding geometry have already updated to the new dimensions,
`mb_weber_stats` may still be the allocation result from the old dimensions.

### 4. Vulnerability Essence

This is a classic "dimension-dependent cache not invalidated after reconfiguration" issue:

- First frame: small allocation
- Second frame: large usage
- Result: indexing into the old buffer with new dimensions causes heap out-of-bounds write

## Affected Code

- `libaom/av1/encoder/allintra_vis.c:60`
- `libaom/av1/encoder/allintra_vis.c:397`
- `libaom/av1/encoder/ethread.c:2821`
- `libaom/av1/encoder/encoder.c:3804`
- `libaom/examples/av1_enc_fuzzer.cc:275`

## Security Impact

- Vulnerability type: heap out-of-bounds write, not merely out-of-bounds read
- Observed write size from the current sample: 2 bytes, but fundamentally this is memory corruption
- Trigger point is in the encoder's internal preprocessing stage, and is stably reproducible under the multi-threaded path
- If an attacker can control input to the encoding API and allow dynamic reconfiguration mid-stream, this issue has security value

Conservative assessment: should be treated as an "encoder-side triggerable memory corruption" issue.

## Fix Recommendations

### Recommendation 1

In `av1_init_mb_wiener_var_buffer()`, record the `mi_rows/mi_cols` corresponding to the currently allocated buffer,
and forcibly free and reallocate when dimensions change.

### Recommendation 2

Avoid mixing `cpi->frame_info` and `cm->mi_params` in this path.
Allocation, loop boundaries, and index strides should be unified based on the same dimension source for the current frame.

### Recommendation 3

After `aom_codec_enc_config_set()` triggers a resolution change, actively invalidate caches related to old frame dimensions,
including:

- `mb_weber_stats`
- `prep_rate_estimates`
- `ext_rate_distribution`
