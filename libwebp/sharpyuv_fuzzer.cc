// Copyright 2026 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// add CMakeLists.txt "add_webp_fuzztest(sharpyuv_fuzzer sharpyuv)"
//
////////////////////////////////////////////////////////////////////////////////

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include "./fuzz_utils.h"
#include "sharpyuv/sharpyuv.h"
#include "sharpyuv/sharpyuv_csp.h"

namespace {

size_t SafeSize(size_t size) {
  static constexpr size_t kMaxBytes = 4u * 1024u * 1024u;
  return std::min(size, kMaxBytes);
}

void FillFromBlob(std::string_view blob, std::vector<uint8_t>* dst) {
  if (dst == nullptr || dst->empty()) return;
  if (blob.empty()) {
    std::fill(dst->begin(), dst->end(), 0);
    return;
  }
  for (size_t i = 0; i < dst->size(); ++i) {
    (*dst)[i] = static_cast<uint8_t>(blob[i % blob.size()]);
  }
}

void SharpYuvTest(std::string_view blob, int width, int height,
                  int rgb_bit_depth, int yuv_bit_depth, int matrix_type,
                  int transfer_type, bool use_options,
                  bool use_small_output_buffers) {
  if (width <= 0 || height <= 0) return;

  const int rgb_bytes_per_sample = (rgb_bit_depth <= 8) ? 1 : 2;
  const int yuv_bytes_per_sample = (yuv_bit_depth <= 8) ? 1 : 2;

  const int rgb_step = rgb_bytes_per_sample;
  const int rgb_stride = width * rgb_step;
  const int y_stride = width * yuv_bytes_per_sample;
  const int uv_width = (width + 1) / 2;
  const int uv_height = (height + 1) / 2;
  const int uv_stride = uv_width * yuv_bytes_per_sample;

  size_t rgb_plane_size = SafeSize(static_cast<size_t>(rgb_stride) * height);
  size_t y_plane_size = SafeSize(static_cast<size_t>(y_stride) * height);
  size_t uv_plane_size = SafeSize(static_cast<size_t>(uv_stride) * uv_height);
  if (use_small_output_buffers) {
    if (y_plane_size > 0) --y_plane_size;
    if (uv_plane_size > 0) --uv_plane_size;
  }

  std::vector<uint8_t> r(rgb_plane_size);
  std::vector<uint8_t> g(rgb_plane_size);
  std::vector<uint8_t> b(rgb_plane_size);
  std::vector<uint8_t> y(y_plane_size);
  std::vector<uint8_t> u(uv_plane_size);
  std::vector<uint8_t> v(uv_plane_size);
  FillFromBlob(blob, &r);
  FillFromBlob(blob.substr(std::min<size_t>(1, blob.size())), &g);
  FillFromBlob(blob.substr(std::min<size_t>(2, blob.size())), &b);

  const SharpYuvMatrixType safe_matrix_type =
      static_cast<SharpYuvMatrixType>(
          static_cast<uint32_t>(matrix_type) % kSharpYuvMatrixNum);
  const SharpYuvConversionMatrix* const matrix =
      SharpYuvGetConversionMatrix(safe_matrix_type);
  if (matrix == nullptr) return;

  if (use_options) {
    SharpYuvOptions options;
    if (!SharpYuvOptionsInit(matrix, &options)) return;
    options.transfer_type =
        static_cast<SharpYuvTransferFunctionType>(transfer_type);
    (void)SharpYuvConvertWithOptions(
        r.data(), g.data(), b.data(), rgb_step, rgb_stride, rgb_bit_depth,
        y.empty() ? nullptr : y.data(), y_stride, u.empty() ? nullptr : u.data(),
        uv_stride, v.empty() ? nullptr : v.data(), uv_stride, yuv_bit_depth,
        width, height, &options);
  } else {
    (void)SharpYuvConvert(
        r.data(), g.data(), b.data(), rgb_step, rgb_stride, rgb_bit_depth,
        y.empty() ? nullptr : y.data(), y_stride, u.empty() ? nullptr : u.data(),
        uv_stride, v.empty() ? nullptr : v.data(), uv_stride, yuv_bit_depth,
        width, height, matrix);
  }
}

}  // namespace

FUZZ_TEST(SharpYuv, SharpYuvTest)
    .WithDomains(
        fuzztest::String().WithMaxSize(fuzz_utils::kMaxWebPFileSize + 1),
        /*width=*/fuzztest::InRange<int>(1, 257),
        /*height=*/fuzztest::InRange<int>(1, 257),
        /*rgb_bit_depth=*/fuzztest::ElementOf<int>({8, 10, 12, 16}),
        /*yuv_bit_depth=*/fuzztest::ElementOf<int>({8, 10, 12}),
        /*matrix_type=*/fuzztest::InRange<int>(0,
                                               static_cast<int>(kSharpYuvMatrixNum)),
        /*transfer_type=*/fuzztest::Arbitrary<int>(),
        /*use_options=*/fuzztest::Arbitrary<bool>(),
        /*use_small_output_buffers=*/fuzztest::Arbitrary<bool>());

