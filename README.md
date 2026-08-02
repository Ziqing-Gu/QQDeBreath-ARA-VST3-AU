# QQDeBreath ARA / VST3 / AU

当前插件版本 / Current plugin version: **1.19**

## 1.19 更新 / What's new in 1.19

### 中文

- **修复 macOS ARA 首次打开的界面尺寸握手：** 针对部分 Fender Studio / Studio One macOS 环境，编辑器首次嵌入时只显示在左侧、需要拖动宿主窗口才恢复的问题，增加了有限次数的延迟宿主布局同步。
- **兼容宿主晚到的尺寸和缩放信息：** ARA 编辑器在挂入宿主层级、首次显示以及宿主发送缩放因子后重新布局，不持续干扰用户正常调整窗口。
- **音频和工程行为不变：** 没有修改分析模型、区域边界、监听、Fade、Norm、Gain、EQ、ARA 工程状态或三轨导出逻辑。

### English

- **Fixed the first-open macOS ARA layout handshake:** On some Fender Studio / Studio One macOS setups, the editor could appear only in the left part of the host panel until the host window was dragged. The editor now performs a bounded deferred host-layout synchronization.
- **Handles late host bounds and scale information:** The ARA editor resynchronizes after hierarchy attachment, first visibility, and host scale-factor delivery without continuously fighting normal user resizing.
- **Audio and project behavior unchanged:** The analyzer, region boundaries, monitoring, Fade, Norm, Gain, EQ, ARA project state, and three-stem export logic were not changed.
## 1.18 更新 / What's new in 1.18

### 中文

- **移除插件内部缩放手柄：** 主界面右下角不再显示 JUCE 内部绘制的缩放角标。
- **保留宿主窗口缩放：** 插件仍保持可缩放状态与原有尺寸限制，宿主提供的窗口缩放和最大化能力不受影响。
- **声音与状态逻辑不变：** 未修改分析模型、regions、监听、Fade、Norm、Gain、EQ、ARA 状态或三轨导出。

### English

- **Internal resize grip removed:** The JUCE-drawn resize handle is no longer shown in the lower-right corner of the editor.
- **Host resizing preserved:** The editor remains resizable with the existing size limits, so host-provided resizing and maximization are unchanged.
- **Audio and state behavior unchanged:** No analyzer, region, monitor, Fade, Norm, Gain, EQ, ARA-state, or stem-export logic was changed.

## 1.16 更新 / What's new in 1.16

### 中文

- **修复监听关闭后的偶发干声泄漏：** VST3 预览缓存发生短暂锁竞争时，不再绕过 `Voice / Breath / Noize` 开关回退到整路干声。
- **Breath 与 Noize 同步修复：** Breath、Noize 以及三路全部关闭均遵守当前 checkbox 组合；实时线程仍不等待锁，缓存繁忙时使用保护性静音块。
- **ARA 多轨实例隔离：** ARA 编辑器优先使用当前插件实例实际绑定的 playback regions，不再在多轨工程中回退到文档第一条 audio source 或第一份编辑状态。
- **每轨独立状态：** 每个 ARA source 分别保存与恢复 regions、Monitor、Norm、Target、Global Gain、Global EQ 和波形显示参数，避免一轨覆盖另一轨。
- **新增回归测试：** 自动覆盖 Breath/Noize 监听关闭、三路全关、录音/分析锁竞争以及两个普通插件实例的参数隔离。
- **算法不变：** Breath 检测模型、region 边界、Fade/Norm/Gain/EQ 处理顺序和三轨导出算法未改变。

### English

- **Intermittent dry-monitor leak fixed:** VST3 preview no longer falls back to the full dry input when a recording or analysis cache lock is briefly busy.
- **Breath and Noize are both covered:** Breath-off, Noize-off, and all-off checkbox combinations are protected without waiting on the realtime thread; a busy prepared cache yields a protective silent block.
- **ARA multitrack instance isolation:** The editor now uses playback regions assigned to its own plug-in instance instead of falling back to the first source or first saved state in the ARA document.
- **Per-source project state:** Regions, Monitor, Norm, Target, Global Gain, Global EQ, and waveform display parameters are stored and restored independently for each ARA source.
- **Regression coverage:** Automated tests cover Breath/Noize routing, all-off monitoring, recording/analysis lock contention, and parameter isolation between two plain plug-in instances.
- **Algorithm unchanged:** The detector model, region boundaries, Fade/Norm/Gain/EQ order, and three-stem export algorithm are unchanged.
## 1.15 更新 / What's new in 1.15

### 中文

- **修复编辑器重开后参数复位：** 同一插件实例关闭窗口再重新打开时，`Breath Norm` 勾选状态与 `Target` 数值不再被全局默认预设覆盖。
- **全局默认值只应用一次：** `Set as Default` 保存的预设现在只会在真正的新插件实例中应用一次，不会因为宿主销毁并重建编辑器窗口而重复应用。
- **项目状态优先：** DAW 工程保存的当前实例状态继续优先于全局默认值；Global Gain、Global EQ、局部 Breath Gain/EQ 和已编辑区域都会按工程状态恢复。
- **新增状态回归探针：** 自动验证 Norm、Target、Global Gain、Global EQ 以及选中 Breath 的 Gain/EQ 在编辑器重建后保持不变。
- **声音不变：** 本次没有修改原生分析器、内嵌模型、监听 DSP、EQ、Fade、Gain 或三轨导出算法。

### English

- **Editor-reopen state fix:** Closing and reopening the editor for the same plug-in instance no longer resets the `Breath Norm` switch or `Target` value to the global default preset.
- **Global defaults are applied once:** A preset saved with `Set as Default` is now applied once to a genuinely new plug-in instance, not every time the host destroys and recreates its editor window.
- **Project state remains authoritative:** DAW-saved instance state continues to take priority over global defaults. Global Gain, Global EQ, per-Breath Gain/EQ, and edited regions are restored from the project.
- **State regression probe:** An automated probe now checks Norm, Target, Global Gain, Global EQ, and selected-Breath Gain/EQ across editor recreation.
- **No sound changes:** The native analyzer, embedded model, monitoring DSP, EQ, Fade, Gain, and three-stem export algorithms are unchanged.

## 1.14 更新 / What's new in 1.14

### 中文

- 将 QQEasyTool 中验证过的 ARA 监听修复移植回 QQDeBreath。
- 不再把 ARA 中合法的静音块错误替换为 Dry 音频。
- 未分析的 ARA Source 按 Voice 处理，并遵循 Voice 监听勾选状态。
- 复合 ARA Event 即使没有检测区域也会保存状态；`Clear Analysis` 会同步清理映射状态和实时状态。
- 分析算法、模型、EQ、Fade、Gain 和导出逻辑未改变。

### English

- Ported the proven QQEasyTool ARA monitoring fix back to QQDeBreath.
- Legitimate ARA silence is no longer replaced with dry audio.
- Unanalyzed ARA sources are treated as Voice and obey the Voice monitor switch.
- Composite ARA events persist state even when no region is detected; `Clear Analysis` clears mapped and realtime state consistently.
- Analysis, model, EQ, Fade, Gain, and export behavior are unchanged.

## 1.13 更新 / What's new in 1.13

### 中文

- 新增 `Ctrl+Z` 与 `Ctrl+Shift+Z` 区域 Undo/Redo 快捷键。
- Global 与 Selected `Auto Apply` 使用同一个同步偏好。
- ARA / VST3 编辑器可放大到宿主或屏幕允许的尺寸；窄窗口自动使用两行紧凑工具栏。
- Breath / Noize 区域可以按住区域主体拖动，移动会避让相邻区域，并记录为一次 Undo 操作。

### English

- Added `Ctrl+Z` and `Ctrl+Shift+Z` region Undo/Redo shortcuts.
- Global and Selected `Auto Apply` share one synchronized preference.
- ARA/VST3 editors can grow to the host or screen limit, while narrow windows use a compact two-row toolbar.
- Breath/Noize regions can be moved by dragging the region body. Movement respects neighboring regions and is stored as one Undo step.

## 项目简介 / About

### 中文

QQDeBreath 是一个面向人声编辑的 Breath / Noize 波形分离工具。它使用同一套 JUCE Processor 与 Editor 代码生成三种宿主工作方式：

- Cubase、Nuendo、REAPER、Studio One 等宿主中的 ARA Audio Extension；
- ARA 不可用时，可录制当前插件实例输入的普通 VST3；
- macOS Logic Pro 使用的 ARA 2 Audio Unit。

核心工作流是加载或录制音频、自动分析 Breath、在波形上编辑 Breath / Noize 区域、组合监听 Voice / Breath / Noize，最后导出 `Vocal Only.wav`、处理后的 `Breath.wav` 与 `Noize.wav`。它不是传统的 Threshold/Sensitivity 参数型降噪器，也不是完整频谱源分离软件。

### English

QQDeBreath is a waveform-oriented Breath/Noize separation and editing tool for vocals. One shared JUCE processor/editor implementation provides three host workflows:

- An ARA Audio Extension in hosts such as Cubase, Nuendo, REAPER, and Studio One.
- A plain VST3 recorder that captures the current plug-in instance input when ARA is unavailable.
- An ARA 2 Audio Unit for Logic Pro on macOS.

The core workflow is to load or record audio, analyze Breath regions, edit Breath/Noize regions on the waveform, monitor any combination of Voice/Breath/Noize, and export `Vocal Only.wav`, processed `Breath.wav`, and `Noize.wav`. It is not a conventional threshold/sensitivity noise reducer or a full spectral source-separation suite.

感谢网友 Jason 提供训练样本并参与测试。

Special thanks to Jason for providing training samples and helping test the detector.

## 宿主格式 / Host formats

### 中文

- Cubase、Nuendo、REAPER、Studio One：安装并使用 `QQDeBreath.vst3`。
- Logic Pro：安装 `QQDeBreath.component`（Audio Unit v2）。Logic 不加载 VST3。
- macOS AU 构建为 Universal 2，同时包含 `arm64` 与 `x86_64`。
- Apple Silicon 上使用第三方 ARA Audio Unit 时，Logic Pro 目前需要通过 Rosetta 运行；QQDeBreath 应位于第一个 Audio Effect 插槽。

### English

- Cubase, Nuendo, REAPER, and Studio One: install and use `QQDeBreath.vst3`.
- Logic Pro: install `QQDeBreath.component` (Audio Unit v2). Logic does not load VST3.
- The macOS AU is Universal 2 and contains both `arm64` and `x86_64`.
- On Apple Silicon, Logic Pro currently requires Rosetta for third-party ARA Audio Units; place QQDeBreath in the first Audio Effect slot.

## 分析后端 / Analysis backend

### 中文

1.16 的 Breath 检测路径已完全使用原生 C++，保留多 Event ARA Load、动态 Pre/Post 频谱、Loop 预览、Global Gain、`Set as Default` 与逐 Breath Adjust/EQ。正常分析不再调用 `qq_debreath_bridge.exe` 或 `QQDeBreathTool.exe analyze-for-plugin`。

内嵌分析器使用与 QQDeBreathTool 1.11 相同的模型数据：

```text
D655AF2BFB260866DE74319D179FDA0B007E6539711C572967BC0FC709E42AFE
```

旧 bridge 只作为调试和兼容工具保留在 CMake 工程中。

### English

The 1.16 Breath detection path is fully native C++. It retains multi-event ARA Load, dynamic Pre/Post spectra, loop preview, Global Gain, `Set as Default`, and per-Breath Adjust/EQ. Normal analysis no longer calls `qq_debreath_bridge.exe` or `QQDeBreathTool.exe analyze-for-plugin`.

The embedded analyzer uses the same model data as QQDeBreathTool 1.11:

```text
D655AF2BFB260866DE74319D179FDA0B007E6539711C572967BC0FC709E42AFE
```

The old bridge remains in CMake only as a debug and compatibility tool.

## 使用方法 / Workflow

### ARA 中文

1. 在 DAW 中选中一个或多个 Audio Event，将 QQDeBreath 作为 ARA Audio Extension 加载。
2. 打开插件并点击 `Load`。
3. 点击 `Analyze` 自动识别 Breath。
4. 在波形上检查、移动、缩放或重新划定 Breath / Noize 区域。
5. 使用 Voice / Breath / Noize 三个 checkbox 组合监听。
6. 根据需要调整 Fade、Breath Norm、Global Gain、Global EQ 和局部 Breath Adjust/EQ。
7. 点击 `Export Stems` 导出三轨。

### ARA English

1. Select one or more audio events in the DAW and apply QQDeBreath as an ARA Audio Extension.
2. Open the plug-in and click `Load`.
3. Click `Analyze` to detect Breath regions.
4. Inspect, move, resize, or redraw Breath/Noize regions on the waveform.
5. Monitor any combination of the Voice/Breath/Noize checkboxes.
6. Adjust Fade, Breath Norm, Global Gain, Global EQ, and per-Breath Adjust/EQ as needed.
7. Click `Export Stems` to export the three stems.

### 普通 VST3 中文

1. 将 QQDeBreath 作为普通 VST3 插入人声轨道。
2. 点击 `Record` 进入待录状态。
3. 在 DAW 中开始播放，插件才真正录制输入；停止 DAW 播放后，本次录制自动停止。
4. 再次录制时，已有时间位置会被新录音覆盖，之前未录制的位置会继续补录，不会简单追加到尾部。
5. 点击 `Analyze` 后进行区域编辑、组合监听与导出。

录音缓冲区按大块预分配，避免在实时音频线程中每个 block 扩容。

### Plain VST3 English

1. Insert QQDeBreath as a regular VST3 on the vocal track.
2. Click `Record` to arm recording.
3. Start DAW playback to begin capture; stopping DAW playback ends that recording pass automatically.
4. A later pass overwrites positions already recorded and fills positions that were previously empty instead of appending everything to the end.
5. Click `Analyze`, then edit regions, monitor the required components, and export.

The recording buffer is preallocated in large blocks to avoid per-block growth on the realtime audio thread.

## EQ、Gain 与默认状态 / EQ, Gain, and defaults

### 中文

- Global Gain 位于可选 Breath Norm 之后，范围为 `-60 dB` 到 `+30 dB`；主界面与 Global EQ 页面共享同一参数。
- Global EQ 处理全部 Breath；局部 Breath Adjust/EQ 只处理选中的 Breath，局部 Gain 范围为 `-30 dB` 到 `+30 dB`。
- Global/Selected EQ 都支持 Clear、Auto Apply 和 Apply；Pre/Post 频谱用于比较处理前后结果。
- `Set as Default` 保存新实例使用的全局启动预设，包括监听勾选、Follow、Fade、Fade In/Out、Breath Norm、Target、Global Gain、Global EQ 与 Auto Apply 偏好。保存前会弹出确认。
- 局部 Breath Gain/EQ 不包含在全局默认预设中。
- 已保存的 DAW 工程状态始终优先；关闭再打开同一插件窗口不会重新套用全局默认值。
- ARA 中显式点击 `Load` 是有意的重置边界：新 Source 从已保存的全局默认值开始，旧分析和局部设置会清除。

### English

- Global Gain follows optional Breath Norm and ranges from `-60 dB` to `+30 dB`; the main page and Global EQ page control the same parameter.
- Global EQ processes every Breath. Per-Breath Adjust/EQ affects only the selected Breath, with local Gain from `-30 dB` to `+30 dB`.
- Global and Selected EQ provide Clear, Auto Apply, and Apply controls; Pre/Post spectra compare the signal before and after processing.
- `Set as Default` stores the global startup preset for new instances: monitor checkboxes, Follow, Fade, Fade In/Out, Breath Norm, Target, Global Gain, Global EQ, and Auto Apply preferences. A confirmation dialog is shown before saving.
- Per-Breath Gain/EQ is not included in the global default preset.
- Saved DAW project state always takes priority. Closing and reopening the same editor window does not reapply global defaults.
- An explicit ARA `Load` is the intentional reset boundary: the new source starts from the saved global defaults, while old analysis and local settings are cleared.

## 技术说明 / Technical notes

### 中文

- Noize 仍是正式的手动区域类型和独立 stem；Breath EQ 不改变 Voice、Noize、检测结果或区域边界。
- ARA `Load` 可把采样率一致的多个 Event 组合成临时波形，并把区域映射回各自的 ARA Source 用于播放。
- ARA 播放会将 Audio Source 重采样到当前宿主播放采样率，避免 Source 与 REAPER 工程采样率不同时静音。
- ARA Source 会在 Load/工程恢复时预载入不可变内存缓存；实时播放不再每个音频 block 请求宿主 sample-access API。
- 实时线程只读取 revision-cached 的 Source/Region 快照，不等待 UI 锁；Breath Norm peak 在消息线程预计算；未变化的 EQ 状态不在实时回调中分配或序列化 JSON。
- 分析只在后台线程运行，不在 audio thread 中执行。

### English

- Noize remains a first-class manual region type and independent stem. Breath EQ does not change Voice, Noize, detection, or region boundaries.
- ARA `Load` can combine selected events with the same sample rate into a temporary waveform and map regions back to their individual ARA sources for playback.
- ARA playback resamples Audio Source samples to the current host playback rate, avoiding silence when a REAPER project and source use different sample rates.
- Each ARA source is preloaded into an immutable memory cache during Load/project restore; realtime playback no longer calls the host sample-access API for every block.
- The audio thread reads a revision-cached Source/Region snapshot without waiting for UI locks. Breath Norm peaks are prepared on the message thread, and unchanged EQ state does not allocate or serialize JSON in the realtime callback.
- Analysis runs only on the background analysis thread, never on the audio thread.

## 构建 / Build

### Windows 中文

在项目目录执行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\build-ara-vst3.ps1
```

本地构建输出通常位于：

```text
build-vs/QQDeBreath_artefacts/Release/VST3/QQDeBreath.vst3
```

系统安装路径通常为：

```text
C:\Program Files\Common Files\VST3\QQDeBreath.vst3
```

### Windows English

Run from the repository root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\build-ara-vst3.ps1
```

The local build output is normally:

```text
build-vs/QQDeBreath_artefacts/Release/VST3/QQDeBreath.vst3
```

The standard system install location is:

```text
C:\Program Files\Common Files\VST3\QQDeBreath.vst3
```

### macOS 中文 / English

```text
QQDeBreath_VST3 -> QQDeBreath.vst3
QQDeBreath_AU   -> QQDeBreath.component
```

标准安装位置 / Standard install locations:

```text
/Library/Audio/Plug-Ins/VST3/QQDeBreath.vst3
/Library/Audio/Plug-Ins/Components/QQDeBreath.component
```

GitHub Actions 会分别发布 Apple Silicon VST3、Intel VST3、Universal 2 AU 和 Windows x64 VST3。macOS 测试包采用 ad-hoc 签名，没有 Apple Developer ID 签名和公证，因此第一次扫描前可能需要在“系统设置 > 隐私与安全性”中手动允许。

GitHub Actions publishes separate Apple Silicon VST3, Intel VST3, Universal 2 AU, and Windows x64 VST3 artifacts. The macOS test builds are ad-hoc signed rather than Developer ID signed/notarized, so manual approval in System Settings > Privacy & Security may be required before the first host scan.

详细安装步骤见 / See the detailed installation guides:

- `docs/QQDeBreath 1.19 Windows与macOS安装使用说明（中文）.txt`
- `docs/QQDeBreath-1.19-Windows-macOS-INSTALL.txt`
