# QQDeBreath release history / 发布记录

## 1.25 Stable — 2026-09-22 / Parameter and Auto Apply responsiveness

中文：优化 Gain、Norm Target 及全局/区域 EQ Auto Apply 的拖动响应。Gain/Norm Target 通过缩放处理后波形缓存更新显示，不再重复扫描和处理整段音频。EQ 波形计算移至只保留最新请求的后台线程；拖动时保留上一份完整波形，最新计算完成后更新。试听参数仍独立更新。缓存区域峰值、用区间索引绘制波形，并将动态频谱限制到当前窗口涉及的区域；拖动不再触发无关的全文件静态频谱计算。连续区域 EQ 拖动的 ARA 更新采用限频而非不断推迟。Auto Apply 关闭时的预览/Apply 规则不变。

English: Improved Gain, Norm Target and global/selected EQ Auto Apply responsiveness. Gain/Target scale cached display data without rescanning or reprocessing the source. EQ waveform rendering uses a background worker with latest-request cancellation, keeping the last complete display until the newest result is ready. Audition parameters update independently. Region peaks are cached, painting uses an interval index, and dynamic spectra consider only regions overlapping the current window. Dragging no longer triggers unrelated whole-file static spectra. Continuous selected EQ edits throttle ARA updates instead of indefinitely postponing them. Manual preview/Apply behavior is preserved.

Scope: display/editor only; audio processor, EQ DSP, analysis and 1.24 stem-export implementation remain unchanged. No new audio-parameter smoothing algorithm is introduced. Waveform updates may lag behind EQ dragging by design. Gain/Target cache scaling can differ from the previous display by float rounding; it does not change playback or exported audio. Original playback synchronization and fixed Export Stems behavior are retained.

Status: Stable, explicitly designated by the user on 2026-09-22 after accepting 1.25 responsiveness. Windows validation is inherited unchanged; no new macOS acceptance is claimed.


## 1.24 Candidate — 2026-09-22 / Export Stems

中文：修复 Export Stems 在界面线程重复遍历整段音频和全部区域造成的长时间无响应。区域边界、相邻关系、峰值及增益按快照预计算，仅处理有效范围；保留 1.23 的重叠区域优先级及 Fade/Norm/Gain/全局和区域 EQ 规则。三轨渲染、已加载 ARA 本地 WAV 读取及文件写入在可取消后台任务中执行，显示进度。导出使用开始时的数据与设置，后续编辑不改变正在导出的内容。完成三轨临时写入后才替换目标文件；取消保留原文件。关闭编辑器会取消并等待任务安全退出。普通 VST3 播放定位与 ARA 播放路径未修改。

English: Fixed excessive synchronous work in Export Stems. Region boundaries, adjacency, peaks and gains are cached for the export snapshot, with rendering restricted to relevant ranges. The 1.23 overlap priority and Fade/Norm/Gain/global and per-region EQ semantics are preserved. Rendering, loaded ARA local-WAV reads and file writing run in a cancellable worker with progress. Later edits do not change the captured export. All three temporary WAVs are written before replacing targets; cancellation preserves existing stems. Closing the editor cancels and joins the worker safely. Ordinary VST3 transport and ARA playback paths are unchanged.

Status: Candidate, not user-accepted Stable. Stable/rollback remains 1.23. Plan A Windows build and automated export/transport regression checks passed; no new macOS build or real DAW acceptance claimed.


Recent release interval: 1.20 → 1.21, 1.22, 1.23. Earlier history remains in README.md and DEVELOPMENT_HISTORY.md.
最近发行区间：1.20 → 1.21、1.22、1.23；更早历史完整保留在 README.md 和 DEVELOPMENT_HISTORY.md。

## 1.23 更新 / What's new in 1.23

**Stable — 2026-09-20：用户已确认本版播放行为正确。/ The user confirmed the playback behavior and designated this version Stable.**

### 中文

- 修复“停止时点击插件波形，按播放却回到 DAW 位置”的问题。所选音频位置保持为待播放状态，在第一个实际播放音频块建立时间对应关系，从点击处开始播放。
- 停止期间重复的音频回调、启动时的宿主时间变化、零长度处理块和音频引擎重新准备不会吞掉待播放选择。
- 后续 DAW 定位跳变仍取消内部试听偏移和内部循环，恢复原始工程时间线；停止时宿主报告的新定位也会取消待播放选择。
- 普通 VST3 界面使用音频线程发布的位置快照，避免在界面线程直接读取宿主播放时钟。ARA、分析、监听及 EQ 处理保持原有行为。

### English

- Fixed a stopped waveform selection being lost when Play starts. The selected local sample remains pending until the first rendered playing block, where it is anchored to the actual host clock.
- Repeated stopped callbacks, a different startup timestamp, zero-length blocks, and audio-engine preparation do not consume the pending selection.
- Subsequent DAW seeks still cancel audition offsets and internal loops. A new host-reported stopped position cancels the pending selection before playback.
- The ordinary VST3 editor reads a published audio-thread transport snapshot instead of querying the host playhead from the UI thread. ARA, analysis, monitoring, and EQ processing retain their existing behavior.

## 1.22 更新 / What's new in 1.22

**2026-09-19 — 候选版，用户未认可；停止/启动复位行为不符合要求，已由 1.23 取代。 / Candidate not accepted by the user; its stop/start reset behavior did not meet the requirement and was superseded by 1.23.**

### 中文

- **保留波形点击试听：** 普通 VST3 中点击插件波形仍从对应位置播放；正常连续播放、原地停止/继续以及音频引擎重新准备均保留试听偏移。
- **DAW 定位优先：** 在 DAW 重新定位或时间线循环回跳时，清除内部试听偏移和内部 EQ 循环，播放指针与实际音频一起回到录音在工程中的原始时间线。
- **修正 1.21 的过度复位：** 停止时根据最近的宿主播放位置判断是否跳转，不再与最初点击波形的时间比较；宿主暂停音频回调时也支持停止状态下重新定位。
- **其他功能保留：** ARA 处理、分析、监听、Fade、Norm、Gain、EQ 和工程状态格式保持原有行为。

### English

- **Waveform audition preserved:** Ordinary VST3 waveform clicks still select the audio to play. Continuous playback, pause/resume in place, and audio-engine preparation retain the audition offset.
- **DAW relocation takes priority:** A DAW seek or timeline cycle wrap clears the audition offset and internal EQ loop, aligning both the cursor and actual audio with the recording's original timeline.
- **Corrected the 1.21 reset conditions:** Stopped transport is compared with the latest host position, not the original waveform-click timestamp. Stopped seeks also work when the host suspends audio callbacks.
- **Other behavior preserved:** ARA, analysis, monitoring, Fade, Norm, Gain, EQ, and project-state format retain their existing behavior.

## 1.21 更新 / What's new in 1.21

**2026-09-19 — 候选版，用户未认可；停止/启动复位行为不符合要求，已由 1.23 取代。 / Candidate not accepted by the user; its stop/start reset behavior did not meet the requirement and was superseded by 1.23.**

### 中文

- **普通 VST3 恢复 DAW 定位同步：** 在插件内点击波形试听后，DAW 前跳、后跳、停止或循环回跳会清除内部试听偏移及内部 EQ 循环，使播放指针和实际音频重新对齐录音在工程中的原始位置。
- **保留插件内试听：** 连续播放期间保留波形点击位置；停止时新选择的试听位置可在原位置启动播放，DAW 再次定位后恢复跟随。
- **停止时也能归位：** 对暂停音频回调的宿主，编辑器在停止状态检测定位变化并同步指针。音频引擎重新准备时清除旧试听偏移。
- **回归验证：** 新增宿主定位测试，检查 44.1/48/96 kHz、采样数/秒时间戳、变长音频块和非零录音起点的指针与实际音频采样对齐。

### English

- **Plain VST3 follows DAW relocation again:** After an internal waveform seek, a DAW seek, stop, or cycle wrap clears the temporary preview offset and internal EQ loop. Both the cursor and rendered audio return to the recording's original timeline position.
- **Internal audition preserved:** Continuous playback retains the selected preview position. A new selection made while stopped can start at the same host position; relocating the DAW returns control to its timeline.
- **Stopped-host support:** The editor also detects stopped relocation when the host suspends audio callbacks. Preparing the audio engine clears stale preview offsets.
- **Regression coverage:** Transport tests verify cursor and rendered sample alignment at 44.1/48/96 kHz, with sample/seconds timestamps, variable block sizes, and a nonzero recording origin.

