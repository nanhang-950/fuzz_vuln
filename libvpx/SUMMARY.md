# libvpx Fuzzing 项目总结

### Fuzzer 源代码 (5个)

1. **vpx_image_fuzzer.c** (图像格式处理)
   - 测试像素格式转换 (I420, I422, I444, NV12, YV12)
   - 测试高位深度格式 (10-bit, 12-bit)
   - 测试色彩空间转换
   - 测试图像操作 (分配、包装、翻转、矩形设置)

2. **vpx_codec_control_fuzzer.c** (控制接口)
   - 测试编码器控制参数 (VP8/VP9)
   - 测试解码器控制参数
   - 测试无效控制 ID 和参数
   - 支持 VP8 和 VP9 两个变体

3. **vpx_multiframe_fuzzer.c** (多帧序列)
   - 测试关键帧和帧间帧序列
   - 测试参考帧管理
   - 测试帧损坏和错误恢复
   - 测试时间层和并行解码
   - 支持 VP8 和 VP9 两个变体

4. **vpx_y4m_fuzzer.c** (Y4M 格式)
   - 测试 Y4M 文件头解析
   - 测试帧头解析
   - 测试各种色彩空间和交错模式
   - 测试畸形输入处理

5. **vpx_webm_fuzzer.cc** (WebM 容器)
   - 测试 EBML 头部解析
   - 测试 Matroska segment 结构
   - 测试 cluster 和 block 解析
   - 注意: 需要额外依赖

## 🎯 覆盖的攻击面

### 已覆盖 (新增)

1. ✅ **图像格式转换** - vpx_image_fuzzer
   - 像素格式转换
   - 色彩空间处理
   - 高位深度支持

2. ✅ **编码器控制接口** - vpx_codec_control_fuzzer
   - VP8/VP9 编码器参数
   - VP8/VP9 解码器参数
   - 无效参数处理

3. ✅ **多帧序列处理** - vpx_multiframe_fuzzer
   - 帧间依赖
   - 参考帧管理
   - 错误恢复

4. ✅ **Y4M 格式解析** - vpx_y4m_fuzzer
   - 文件头解析
   - 帧头解析
   - 参数验证

5. ✅ **WebM 容器解析** - vpx_webm_fuzzer
   - EBML 解析
   - Matroska 结构
   - 元数据处理

### 原有覆盖

- VP8/VP9 解码器 (vpx_dec_fuzzer.cc)
- VP8/VP9 编码器 (vpx_enc_fuzzer.c)

### 未覆盖 (建议未来工作)

- 外部帧缓冲管理
- 多线程压力测试
- 速率控制算法
- SVC (可伸缩视频编码)

## 🔍 关键特性

### 1. 结构感知 Fuzzing
- 使用字典文件提供格式特定的 token
- 解析输入结构而非盲目变异
- 提高代码覆盖率

### 2. 多层次测试
- 低层: 图像格式、像素操作
- 中层: 编解码器控制、帧序列
- 高层: 容器格式、文件解析

### 3. 错误注入
- 帧损坏模拟
- 参数边界测试
- 状态机错误测试

### 4. Sanitizer 支持
- AddressSanitizer (内存错误)
- UndefinedBehaviorSanitizer (未定义行为)
- 可选 MemorySanitizer

## 🚀 使用流程

### 快速开始

### 持续 Fuzzing

```bash
# 使用 tmux 长时间运行
tmux new-session -d -s fuzz1 './fuzzers_bin/vpx_image_fuzzer corpus1/'
tmux new-session -d -s fuzz2 './fuzzers_bin/vpx_multiframe_fuzzer_vp9 corpus2/'
tmux new-session -d -s fuzz3 './fuzzers_bin/vpx_codec_control_fuzzer_vp9 corpus3/'

# 查看状态
tmux ls
```

## 📈 性能指标

### 预期吞吐量

- **vpx_image_fuzzer**: ~10,000 exec/s
- **vpx_codec_control_fuzzer**: ~5,000 exec/s
- **vpx_multiframe_fuzzer**: ~1,000 exec/s
- **vpx_y4m_fuzzer**: ~2,000 exec/s

### 建议运行时间

- 初步测试: 1-2 小时
- 深度测试: 24-48 小时
- 持续测试: 7+ 天

## 🔧 技术细节

### 编译选项

```bash
-fsanitize=fuzzer          # LibFuzzer 引擎
-fsanitize=address         # 内存错误检测
-fsanitize=undefined       # 未定义行为检测
-g                         # 调试符号
-O1                        # 轻度优化 (平衡速度和调试)
```

### 内存限制

```c
-DVPX_MAX_ALLOCABLE_MEMORY=1073741824  // 1GB
--size-limit=12288x12288                // 最大分辨率
```

### 输入限制

- vpx_image_fuzzer: 64KB
- vpx_codec_control_fuzzer: 4KB
- vpx_multiframe_fuzzer: 128KB
- vpx_y4m_fuzzer: 256KB

