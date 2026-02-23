# libvpx Fuzzing 快速参考

## 📋 Fuzzer 速查表

| Fuzzer | 目标 | 最大长度 | 字典 |
|--------|------|----------|------|
| vpx_image_fuzzer | 图像格式 | 64KB | vpx_image_fuzzer.dict |
| vpx_codec_control_fuzzer | 控制接口 | 4KB | - |
| vpx_multiframe_fuzzer | 多帧序列 | 128KB | vpx_multiframe_fuzzer.dict |
| vpx_y4m_fuzzer | Y4M 格式 | 256KB | vpx_y4m_fuzzer.dict |
| vpx_webm_fuzzer | WebM 容器 | 256KB | - |

## 🎯 常用命令

### 基本运行
```bash
./fuzzers_bin/FUZZER_NAME corpus/
```

### 使用字典
```bash
./fuzzers_bin/vpx_y4m_fuzzer -dict=vpx_y4m_fuzzer.dict corpus/
```

### 限制运行时间
```bash
./fuzzers_bin/vpx_image_fuzzer -max_total_time=3600 corpus/
```

### 并行运行
```bash
./fuzzers_bin/vpx_multiframe_fuzzer_vp9 -jobs=4 -workers=4 corpus/
```

### 最小化语料库
```bash
./fuzzers_bin/vpx_image_fuzzer -merge=1 corpus_min/ corpus/
```

### 重现崩溃
```bash
./fuzzers_bin/vpx_image_fuzzer crash-abc123
```

## 🔍 调试命令

### 使用 GDB
```bash
gdb --args ./fuzzers_bin/vpx_image_fuzzer crash-abc123
```

### 使用 LLDB
```bash
lldb -- ./fuzzers_bin/vpx_image_fuzzer crash-abc123
```

### 查看覆盖率
```bash
./fuzzers_bin/vpx_image_fuzzer -print_coverage=1 corpus/
```

### 查看语料库统计
```bash
./fuzzers_bin/vpx_image_fuzzer -print_corpus_stats=1 corpus/
```

## 📊 监控

### 查看进度
```bash
# Fuzzer 会定期输出统计信息
# #12345: cov: 1234 ft: 5678 corp: 90 exec/s: 1000 ...
```

### 关键指标
- **cov**: 代码覆盖率 (边缘数)
- **ft**: 特征数
- **corp**: 语料库大小
- **exec/s**: 每秒执行次数

## 🛠️ 故障排除

### 问题: Fuzzer 无法启动
```bash
# 检查 libvpx 是否已构建
ls -lh ./libvpx/build/libvpx.a

# 重新构建
./build_fuzzers.sh ./libvpx/build
```

### 问题: 执行速度慢
```bash
# 使用更少的 sanitizer
CFLAGS="-fsanitize=fuzzer,address" ./build_fuzzers.sh

# 增加优化级别
CFLAGS="-fsanitize=fuzzer,address -O2" ./build_fuzzers.sh
```

### 问题: 内存不足
```bash
# 限制最大输入长度
./fuzzers_bin/vpx_multiframe_fuzzer_vp9 -max_len=32768 corpus/

# 限制 RSS 内存
./fuzzers_bin/vpx_multiframe_fuzzer_vp9 -rss_limit_mb=2048 corpus/
```

## 运行 Fuzzer

### 高级选项

```bash
# 限制最大输入长度
./fuzzers_bin/vpx_multiframe_fuzzer_vp9 \
    -max_len=131072 \
    corpus/

# 设置超时时间 (毫秒)
./fuzzers_bin/vpx_codec_control_fuzzer_vp9 \
    -timeout=1000 \
    corpus/

# 并行运行 (4 个工作进程)
./fuzzers_bin/vpx_image_fuzzer \
    -jobs=4 \
    -workers=4 \
    corpus/

# 最小化语料库
./fuzzers_bin/vpx_image_fuzzer \
    -merge=1 \
    corpus_minimized/ \
    corpus/
```

### 创建初始语料库

```bash
# 从 libvpx 测试文件创建
mkdir corpus
cp libvpx/test/*.ivf corpus/ 2>/dev/null || true
cp libvpx/test/*.y4m corpus/ 2>/dev/null || true

# 创建简单的种子文件
echo "YUV4MPEG2 W320 H240 F30:1 Ip A1:1 C420" > corpus/seed.y4m
dd if=/dev/zero bs=115200 count=1 >> corpus/seed.y4m
```

## 攻击面分析

详细的攻击面分析请参见 `libvpx_attack_surface_analysis.md`。

### 关键攻击面

1. **解码器比特流解析** (高危)
   - 恶意构造的 VP8/VP9 比特流
   - 整数溢出/下溢
   - 缓冲区溢出

2. **图像格式处理** (中危)
   - 像素格式转换错误
   - YUV 平面计算错误
   - 色彩空间转换漏洞

3. **控制接口** (中危)
   - 无效控制 ID
   - 参数类型不匹配
   - 状态机错误

4. **容器格式解析** (中危)
   - IVF/WebM/Y4M 解析错误
   - 元数据注入
   - 帧边界错误

5. **多线程处理** (中危)
   - 竞态条件
   - 数据竞争



## 🎓 最佳实践

### 1. 使用多种 Sanitizer

```bash
# AddressSanitizer (内存错误)
CFLAGS="-fsanitize=fuzzer,address"

# UndefinedBehaviorSanitizer (未定义行为)
CFLAGS="-fsanitize=fuzzer,undefined"

# MemorySanitizer (未初始化内存)
CFLAGS="-fsanitize=fuzzer,memory"

# 组合使用
CFLAGS="-fsanitize=fuzzer,address,undefined"
```

### 2. 持续运行

```bash
# 使用 tmux 或 screen 长时间运行
tmux new-session -d -s fuzzing \
    './fuzzers_bin/vpx_image_fuzzer corpus/'

# 或使用 nohup
nohup ./fuzzers_bin/vpx_multiframe_fuzzer_vp9 corpus/ \
    > fuzzing.log 2>&1 &
```

### 3. 监控和分析

```bash
# 查看崩溃
ls -lh crash-*

# 重现崩溃
./fuzzers_bin/vpx_image_fuzzer crash-abc123

# 使用 GDB 调试
gdb --args ./fuzzers_bin/vpx_image_fuzzer crash-abc123
```

### 4. 语料库管理

```bash
# 定期合并和最小化语料库
./fuzzers_bin/vpx_image_fuzzer \
    -merge=1 \
    corpus_new/ \
    corpus_old/

# 删除重复的测试用例
./fuzzers_bin/vpx_image_fuzzer \
    -merge=1 \
    corpus_minimized/ \
    corpus/
```

### 5. 分布式 Fuzzing

```bash
# 在多台机器上运行，共享语料库
# 机器 1
./fuzzers_bin/vpx_image_fuzzer shared_corpus/

# 机器 2
./fuzzers_bin/vpx_image_fuzzer shared_corpus/

# 定期同步语料库
rsync -av machine1:shared_corpus/ shared_corpus/
```
