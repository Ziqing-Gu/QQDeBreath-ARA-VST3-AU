# QQDeBreath ARA / VST3 / AU 1.18

Date: 2026-07-31

## 更新 / Changes

### 中文

- 移除 JUCE 在编辑器右下角绘制的内部缩放手柄。
- 保留宿主提供的窗口缩放和原有编辑器尺寸限制。
- 未修改分析器、regions、监听、Fade、Norm、Gain、EQ、ARA 状态或三轨导出。

### English

- Removed the JUCE-drawn lower-right editor resize grip.
- Preserved host-provided resizing and the existing editor size limits.
- Analyzer, regions, monitoring, Fade, Norm, Gain, EQ, ARA state, and stem export are unchanged.

## 验证 / Validation

### 中文

- Windows x64 VST3 Release 构建：通过
- 编辑器状态与监听路由回归探针：通过

### English

- Windows x64 VST3 Release build: PASS
- Editor state and monitor-routing regression probe: PASS