下面是你提供内容的中文翻译：

------

# libyuv 攻击面与模糊测试计划

## 威胁模型

该项目通常运行在信任边界位置，例如：摄像头帧、网络媒体流以及容器解复用后的缓冲区。攻击者可能控制：

- 压缩的 MJPEG 字节流
- 像素格式元数据（FourCC、分辨率、裁剪参数）
- 来自上游解析器的帧尺寸 / 步长（stride）组合

主要风险包括：内存破坏、整数溢出，以及在大量 SIMD 优化转换路径中的越界读写（OOB read/write）。

------

## 攻击面分级

### Tier 1：字节流解析器（最高优先级）

- `ValidateJpeg()`
- `MJPGSize()`
- `MJPGToI420()`
- `MJPGToNV12()`
- `MJPGToNV21()`
- `MJPGToARGB()`
- `MJpegDecoder::{LoadFrame, DecodeToBuffers, DecodeToCallback}`

原因：这些函数直接处理不可信的压缩字节流，并驱动指针运算以及 libjpeg 回调逻辑。

------

### Tier 2：元数据驱动的转换函数

- `ConvertToI420()`
- `ConvertToARGB()`

原因：这些函数是分发枢纽，会触发多种格式特定实现。它们通常依赖调用者提供正确的 width/height/crop 参数契约，因此在没有结构化元数据约束的情况下进行随机模糊测试信号较低。

------

### Tier 3：原始 plane/row 原语

- 位于 `source/convert*.cc`、`source/scale*.cc`、`source/row*.cc` 中的颜色转换和缩放内核

原因：攻击面非常广，但更适合通过满足尺寸/步长前置条件的结构化封装进行模糊测试。

------

## 已实现的模糊测试器

- 目标文件：`fuzz/mjpeg_fuzzer.cc`
- 构建目标：`libyuv_mjpeg_fuzzer`（通过 `-DLIBYUV_BUILD_FUZZERS=ON` 启用）
- 覆盖路径：
  - 公共 API 路径：`ValidateJpeg + MJPGSize + MJPGTo* + ConvertTo*`
  - 底层路径：`MJpegDecoder` 的 load/decode 回调路径以及 buffer 解码路径

该测试 harness 会限制分辨率和内存分配规模，以避免低价值的 OOM 噪声，同时仍然能够深入覆盖解析与解码逻辑。

------

## 构建与运行

```bash
cmake -S . -B build-fuzz -DLIBYUV_BUILD_FUZZERS=ON -DCMAKE_CXX_COMPILER=clang++
cmake --build build-fuzz -j
mkdir corpus
mv ../unit_test/testdata/*jpg ./corpus
LSAN_OPTIONS=detect_leaks=0 libyuv_mjpeg_fuzzer corpus
```

`LSAN_OPTIONS=detect_leaks=0` 仅在某些受限环境中需要（例如在 ptrace 受限环境下 LeakSanitizer 无法运行时）。

------

## 推荐的下一步模糊测试目标

1. 为 `ConvertToI420` 编写结构化元数据模糊器，针对原始格式
   （`I420/NV12/NV21/YUY2/UYVY/ARGB`），并建立经过验证的尺寸与步长模型。
2. 为 `ConvertToARGB` 编写差分模糊器（通过多种等价转换路径对同一输入进行解码并比较一致性）。
3. 针对架构特定内核进行模糊测试，使用固定尺寸的语料切片，以重点覆盖 SIMD row/scale 实现路径。

------

如果你需要，我也可以把它整理成更正式的安全评估文档风格版本。