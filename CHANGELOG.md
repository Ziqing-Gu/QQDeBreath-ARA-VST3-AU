# QQDeBreath release history / 发布记录

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

