## 1.29 Stable — Background ARA analysis and faster sparse-audio analysis / 2026-10-27

中文：修正上一版把 ARA 分析绑定到编辑器、切换音源时取消分析的问题。分析现在由 ARA 文档按音源管理，切轨或关闭插件窗口后继续，完成后只写回原音源。另一轨可启动独立任务，两条低优先级工作线程由同一插件的实例共享，额外任务排队。复用已有原始音频缓存；工作线程不调用宿主音频读取或修改 ARA 模型。文档定时器在模型线程提交结果，保留各音源参数和分析前的撤销状态；显式取消、宿主归档恢复、音源内容失效和手工区域修改阻止过期结果覆盖。组合选择从当前音源结果刷新，避免旧组合快照遮住后台完成结果。进度显示排队状态或已耗时，不再使用估算百分比。

English: ARA analysis is now owned by the document/source instead of the editor. Switching tracks or closing the editor keeps queued/running work alive; completion is committed to the originating source. Two low-priority workers are shared across instances of this plug-in, with additional jobs queued. Workers use immutable cached audio and never read host audio or mutate the ARA model. A document timer commits on the model thread, preserving per-source parameters and pre-analysis Undo. Explicit cancellation, host archive restore, invalidated audio and manual region edits reject stale work. Group views rebuild from live source regions. The UI shows queued/elapsed time.

性能 / Performance: Native analysis uses exact order-statistic selection instead of full-array sorting, reuses computed spectral statistics for fully digital-silent frames, and suppresses denormal arithmetic in filter tails. Low-level nonzero audio still receives full analysis. Detection model, thresholds and region rules remain unchanged. Ordinary VST3/AU uses the same optimized analyzer; background continuity across ARA track selection applies to ARA mode.

验证 / Validation: Eight original user WAVs were compared against the previous analyzer for each product (16 comparisons total). All region types and start/end sample/time boundaries matched. Overall measured speedup was 1.40–5.47x. These are native analyzer timings, not Cubase end-to-end UI timings. Input SHA-256 hashes were rechecked unchanged. The deterministic ARA regression includes two queued source jobs surviving selection/editor destruction, document-timer completion, group refresh, per-source parameters, cached input with no host reread, pre-analysis Undo, explicit Cancel and host archive Undo rejection. Existing selection/history, multi-instance audio, transport, waveform and product-specific probes pass. Audio processBlock and the ARA playback renderer are unchanged; factory load, internal/PE/moduleinfo 1.0.29 and installed SHA-256 parity pass.

## 1.28 — Cubase ARA selection, Reload and edit history / 2026-10-27

中文：ARA 编辑器响应宿主事件/轨道选择并切换对应波形、区域和参数，无需关闭窗口。显式 EditorView 选择不再被播放实例的区域归属过滤；选择改变跳过旧的一秒恢复间隔，直接使用已有音频缓存显示波形。Reload 保留现有编辑和参数。插件内部撤销/重做现覆盖区域编辑、Gain、Norm、EQ 和监听参数；历史按音源保存在处理器/ARA 文档中，Reload、切轨和重开窗口后仍可用。未应用预览先回滚，切轨前保存待提交修改；旧分析请求的迟到结果不会覆盖新轨。

English: The ARA editor follows host event/track selection without closing its window, restoring the selected source's waveform, regions and parameters. Explicit EditorView selection takes priority over playback-renderer assignment; selection changes bypass the previous retry delay and reuse cached source audio. Reload preserves edits. Internal Undo/Redo covers region edits, Gain, Norm, EQ and monitor parameters, with per-source history owned by the processor/ARA document rather than an editor window. Unapplied previews roll back, pending edits flush before switching, and stale analysis completions cannot replace a different source.

宿主集成 / Host integration: ARA edits queue source/modification content notifications; host archive Undo/Redo refreshes an already-open editor without echoing the restored state as a new edit. Ordinary VST3/AU custom-state edits notify the host through JUCE, and host state restore refreshes the editor. Exact Cubase history-panel entries and host-specific Undo grouping have not been validated in the user's real project. Native host history remains host-dependent; the plugin's own Undo/Redo works independently.

验证 / Validation: Real ARA EditorView regression covers immediate selection callbacks, distinct A/B waveforms, 20 rapid switches, sequence-only selection, two views, multi-event selection, pending/preview edits, unavailable source retry, late analysis rejection, Reload/reopen Undo/Redo and filtered host archive Undo/Redo with no feedback notifications. Ordinary shared editor tests cover region/Gain/Norm/EQ history, reopen, custom-state notifications and host state restore. Existing audio/transport/waveform/ARA multi-instance probes pass; processBlock and ARA renderer code remain unchanged. Windows VST3 factory load, PE/moduleinfo 1.0.28 and installed SHA-256 parity pass.

# QQDeBreath release history / 发布记录

## 1.27 Stable — 2026-10-24 / Apple Clang portability

中文：将存档恢复时清除失效导出路径的赋值明确写为 `juce::File {}`，解决 Apple Clang 的重载歧义。保留 1.26 的多音源缓存、恢复和渲染修复；音频处理算法未改变。界面版本为 ARA 1.27，内部元数据为 1.0.27。

English: Explicitly assign `juce::File {}` when clearing an obsolete exported-WAV path during archive restore, resolving Apple Clang's ambiguous assignment. The 1.26 multi-source cache, archive and render fixes remain intact; audio processing algorithms are unchanged. UI version: ARA 1.27; internal metadata: 1.0.27.

Validation: Windows Release build, all five regression probes and actual VST3 factory loading passed. 22 renderers / 44 sources: 44 audible and 0 silent. 48 legacy ARA DSP cases match sample-for-sample. PE and module metadata are 1.0.27. macOS build and signature checks are recorded with this release. No original Studio One 7 project acceptance is claimed.

Status: Stable, explicitly designated by the user on 2026-10-24. Windows is installed with verified file-hash parity; completed Windows tests are reused. Previous Stable/rollback: 1.26. Mac build and publication results are recorded separately. Original bilingual PDF manuals remain unchanged.

## 1.26 Stable — 2026-10-24 / ARA multi-source rendering

中文：修复同一 ARA 文档超过 32 个音源后旧音源被淘汰、离线渲染为静音的问题。缓存随音源生命周期管理，局部恢复只更新指定音源，保留有效原始音频；支持持久 ID 重映射、工程级独立存档和未分析音源。素材在宿主模型线程允许读取时完成预载，不在实时回调中读取宿主音频。ARA 区域边界、邻接、增益及归一化系数预计算，保留原来的重叠优先级和 EQ 规则；离线渲染不再依赖 Playing 标志。

English: Fixed older sources becoming silent when an ARA document exceeds 32 audio sources. Caches now follow source lifetime; filtered restore preserves unaffected edits and valid raw audio, including source-ID remapping, document-only archives and untouched sources. Missing audio is preloaded on the host model thread before playback, without host reads in the realtime callback. ARA region bounds, adjacency and gains are precomputed while retaining legacy overlap and EQ semantics. Offline rendering no longer requires the Playing flag.

Status: Stable, explicitly designated by the user on 2026-10-24. Existing Windows build and five regression probes are reused; 1.26 is installed with file-hash parity. Previous Stable/rollback: 1.25. Original Studio One 7 project validation has not been reported; Stable designation does not imply a new DAW test or macOS build.

Tradeoff: First source loading and undo reactivation can wait for synchronous host reads; active audio is still cached in full, so memory follows active source duration/channel count. Sources are released on undo deactivation or destruction. Host read failure does not publish partial audio and is retried at a later model edit/load request. Real Studio One 7 acceptance has not been reported. macOS builds are verified separately in the release record.

兼容与升级：存档 schema 仍为 version 1；保留已有分析与 EQ 规则。旧版已渲染出的静音文件需要从原始事件重新渲染。
Compatibility and upgrade: archive schema remains version 1 and existing analysis/EQ rules are preserved. Previously rendered silent files must be rendered again from original events.

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

