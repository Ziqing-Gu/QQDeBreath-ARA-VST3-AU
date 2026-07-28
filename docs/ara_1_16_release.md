# QQDeBreath ARA / VST3 / AU 1.16

## 中文

### 修复

- 修复 VST3 监听中偶发的整路干声泄漏。录音缓存或分析缓存短暂繁忙时，实时线程不会等待锁，也不会绕过 `Voice / Breath / Noize` checkbox；已准备好的分析预览会输出保护性静音块。
- `Breath` 与 `Noize` 使用同一条保护路径。关闭任一路或三路全部关闭时，监听组合保持有效。
- 修复 ARA 多轨 Vocal 串线。编辑器现在只使用当前插件实例绑定的 playback regions，不再回退到 ARA 文档中的第一条 source 或第一份持久状态。
- ARA 的 regions、Monitor、Norm、Target、Global Gain、Global EQ 与波形显示参数改为按 source 保存和恢复；不同 Vocal 轨可以分别加载、分析和编辑。

### 回归验证

`qq_debreath_editor_state_probe` 现在验证：

- Breath 关闭时完整 Breath 区域静音；
- Noize 关闭时完整 Noize 区域静音；
- Voice / Breath / Noize 三路全部关闭时静音；
- 录音缓存锁或分析缓存锁竞争时不泄漏干声；
- 两个普通 VST3 processor 实例的参数互不覆盖；
- 编辑器关闭再打开后既有参数和 EQ 状态不变。

检测模型、region 边界、Fade / Norm / Gain / EQ 顺序和三轨导出算法未修改。

## English

### Fixes

- Fixed intermittent full-dry leakage in VST3 monitoring. If a prepared recording or analysis cache is briefly busy, the realtime thread neither waits nor bypasses the `Voice / Breath / Noize` checkboxes; it emits a protective silent block.
- The same protection covers both Breath and Noize, including the all-off monitor combination.
- Fixed ARA multitrack Vocal state crossover. The editor now resolves sources from playback regions assigned to its own plug-in instance and no longer falls back to the first source or first persisted state in the ARA document.
- ARA regions, Monitor, Norm, Target, Global Gain, Global EQ, and waveform display parameters are now persisted per source so separate Vocal tracks retain independent audio and edits.

### Regression coverage

`qq_debreath_editor_state_probe` covers Breath/Noize/all-off routing, recording and analysis lock contention, parameter isolation between plain VST3 processor instances, and editor recreation state.

The detector model, region boundaries, Fade / Norm / Gain / EQ order, and three-stem export algorithm are unchanged.