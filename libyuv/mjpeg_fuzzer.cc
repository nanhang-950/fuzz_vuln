/*
 * Copyright 2026 The LibYuv Project Authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS. All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "libyuv/convert.h"
#include "libyuv/convert_argb.h"
#include "libyuv/mjpeg_decoder.h"
#include "libyuv/video_common.h"

namespace {

constexpr size_t kMetadataSize = 5;
constexpr int kMaxDimension = 1024;
constexpr int64_t kMaxPixels =
    static_cast<int64_t>(kMaxDimension) * kMaxDimension;
constexpr size_t kMaxPlaneBytes = 8u * 1024u * 1024u;
constexpr size_t kMaxArgbBytes = 16u * 1024u * 1024u;

bool IsReasonableFrameSize(int width, int height) {
  if (width <= 0 || height <= 0 || width > kMaxDimension ||
      height > kMaxDimension) {
    return false;
  }
  return static_cast<int64_t>(width) * height <= kMaxPixels;
}

int FallbackDimension(uint8_t msb, uint8_t lsb) {
  const int value = (static_cast<int>(msb) << 8) | static_cast<int>(lsb);
  return (value % kMaxDimension) + 1;
}

struct CallbackState {
  uint8_t digest;
};

void DigestCallback(void* opaque,
                    const uint8_t* const* data,
                    const int* strides,
                    int rows) {
  CallbackState* state = static_cast<CallbackState*>(opaque);
  if (!state || !data || !strides || rows <= 0 || !data[0] || strides[0] <= 0) {
    return;
  }
  state->digest ^= data[0][0];
}

void FuzzPublicApi(const uint8_t* sample,
                   size_t sample_size,
                   int fallback_width,
                   int fallback_height,
                   uint8_t control) {
  libyuv::ValidateJpeg(sample, sample_size);

  int width = 0;
  int height = 0;
  if (libyuv::MJPGSize(sample, sample_size, &width, &height) != 0 ||
      !IsReasonableFrameSize(width, height)) {
    width = fallback_width;
    height = fallback_height;
  }

  int dst_height = height;
  if ((control & 1) != 0 && height > 1) {
    dst_height -= control % height;
    if (dst_height <= 0) {
      dst_height = 1;
    }
  }

  const int half_width = (width + 1) / 2;
  const int half_height = (dst_height + 1) / 2;
  const size_t y_size = static_cast<size_t>(width) * dst_height;
  const size_t uv_size = static_cast<size_t>(half_width) * half_height;
  const size_t uv_interleaved_size = uv_size * 2u;
  const size_t argb_size = y_size * 4u;
  if (y_size > kMaxPlaneBytes || uv_size > kMaxPlaneBytes ||
      uv_interleaved_size > kMaxPlaneBytes || argb_size > kMaxArgbBytes) {
    return;
  }

  std::vector<uint8_t> y(y_size);
  std::vector<uint8_t> u(uv_size);
  std::vector<uint8_t> v(uv_size);
  std::vector<uint8_t> uv(uv_interleaved_size);
  std::vector<uint8_t> vu(uv_interleaved_size);
  std::vector<uint8_t> argb(argb_size);

  libyuv::MJPGToI420(sample, sample_size, y.data(), width, u.data(), half_width,
                     v.data(), half_width, width, height, width, dst_height);
  libyuv::MJPGToNV12(sample, sample_size, y.data(), width, uv.data(),
                     half_width * 2, width, height, width, dst_height);
  libyuv::MJPGToNV21(sample, sample_size, y.data(), width, vu.data(),
                     half_width * 2, width, height, width, dst_height);
  libyuv::MJPGToARGB(sample, sample_size, argb.data(), width * 4, width, height,
                     width, dst_height);

  libyuv::ConvertToI420(sample, sample_size, y.data(), width, u.data(),
                        half_width, v.data(), half_width, 0, 0, width, height,
                        width, dst_height, libyuv::kRotate0,
                        libyuv::FOURCC_MJPG);
  libyuv::ConvertToARGB(sample, sample_size, argb.data(), width * 4, 0, 0,
                        width, height, width, dst_height, libyuv::kRotate0,
                        libyuv::FOURCC_MJPG);
}

void FuzzDecoder(const uint8_t* sample, size_t sample_size, uint8_t control) {
  libyuv::MJpegDecoder decoder;
  if (!decoder.LoadFrame(sample, sample_size)) {
    return;
  }

  const int width = decoder.GetWidth();
  const int height = decoder.GetHeight();
  if (!IsReasonableFrameSize(width, height)) {
    decoder.UnloadFrame();
    return;
  }

  const int num_components = decoder.GetNumComponents();
  if (num_components <= 0 || num_components > 4) {
    decoder.UnloadFrame();
    return;
  }

  std::array<std::vector<uint8_t>, 4> components;
  std::array<uint8_t*, 4> planes = {nullptr, nullptr, nullptr, nullptr};
  bool valid_layout = true;
  for (int i = 0; i < num_components; ++i) {
    const int component_size = decoder.GetComponentSize(i);
    if (component_size <= 0 ||
        static_cast<size_t>(component_size) > kMaxPlaneBytes) {
      valid_layout = false;
      break;
    }
    components[i].resize(static_cast<size_t>(component_size));
    planes[i] = components[i].data();
  }

  if (valid_layout) {
    int dst_height = height;
    if ((control & 2) != 0 && height > 1) {
      dst_height -= (control >> 1) % height;
      if (dst_height <= 0) {
        dst_height = 1;
      }
    }
    decoder.DecodeToBuffers(planes.data(), width, dst_height);
    CallbackState state = {0};
    decoder.DecodeToCallback(&DigestCallback, &state, width, dst_height);
  }

  decoder.UnloadFrame();
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (!data || size <= kMetadataSize) {
    return 0;
  }

  const uint8_t control = data[0];
  const int fallback_width = FallbackDimension(data[1], data[2]);
  const int fallback_height = FallbackDimension(data[3], data[4]);
  const uint8_t* sample = data + kMetadataSize;
  const size_t sample_size = size - kMetadataSize;
  if (sample_size < 2) {
    return 0;
  }

  FuzzPublicApi(sample, sample_size, fallback_width, fallback_height, control);
  FuzzDecoder(sample, sample_size, control);
  return 0;
}

