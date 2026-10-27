# QQDeBreath authoritative development handoff

Current binary-source commit: 475fb76076f3c73ccd1dc14ce7e410ab676d0309 (tag v1.27).
Current release: https://github.com/Ziqing-Gu/QQDeBreath-ARA-VST3-AU/releases/tag/v1.27
Current completion: Windows build/tests reused and installed with hash parity; Plan B/C/D complete. Three Mac builds passed architecture/version/signature checks. Nine Release assets and public downloads verified; original bilingual PDFs unchanged.
No original Studio One 7 project acceptance is claimed.

Current Stable: 1.29 (internal 1.0.29), user-designated on 2026-10-27.
Historical Candidate records below are preserved; current release: 1.29 Stable.
Previous designated Stable and rollback: 1.26.
Historical 1.23 rollback: 1.20; 1.21 and 1.22 were rejected candidates. Current rollback choices are listed above.
Platforms: Windows x64 VST3; macOS arm64 VST3, x86_64 VST3, Universal 2 AU; ARA-capable shared plug-in.
Development: JUCE 8.0.13, CMake, MSVC 2022 / Xcode.
Source of truth for 1.25 binary code: bb7a0352de9393cfd5f5a7ddbf74f1c22c89aead (tag v1.25). Historical 1.23 binary code: 45904d540e40247dc2606ee64c6b8d2d85fb4404.
Tags v1.23 and v1.25 remain unchanged; post-build documentation commits do not change the binary source.

## 2026-09-20 — 1.23 Stable transport fix
User requirement: a stopped waveform click followed by Play must start the selected audio; a later DAW seek restores the original timeline.
Based on 1.20; see DEVELOPMENT_HISTORY.md for the full preserved intermediate history.
1.21 reset too aggressively on stop and was rejected. 1.22 retained more audition state but still lost a stopped selection at Play startup; it was rejected.
1.23 holds the selection pending until the first actual playing block anchors it to the host clock. Later host relocation clears audition offsets. The ordinary VST3 UI reads an audio-thread clock snapshot.
ARA rendering, analysis and existing EQ/monitoring paths are unchanged.
Validation: Windows build, deterministic sample-accurate transport probes and existing regression/loading checks passed. The user confirmed the Windows behavior. Three macOS CI builds and architecture/signature checks passed; no new real Mac/ARA host acceptance is claimed.
Rollback: 1.20, not the failed 1.21/1.22 candidates.
Known limitation: stopped relocation can only be observed when the host publishes updated transport information.

## 2026-09-20 — Plan C / D documentation and delivery remediation
No plugin code or binary changed. Restored the pre-existing bilingual 1.20 manual unchanged and identified its scope in the current installation guides.
Completed direct asset download instructions, bilingual candidate history and actual desktop delivery. Internal evidence stays outside this public repository.
The earlier execution used obsolete plan definitions, revisited completed Plan B snapshots and ran unnecessary Windows CI jobs. These historical mistakes cannot be undone; this corrective pass does not access those backups or repeat builds.
Plan B completion is inherited from the earlier completion record only. It remains frozen. Do not inspect, hash, refresh or repurpose it.
Future workflow: Plan C uses local verified Windows output plus three macOS builds; Windows cloud build requires an explicit optional dispatch. Plan D publishes verified artifacts to this repository and verifies README links.
Next work: only user-requested development; preserve the accepted waveform/DAW-seek behavior and record new candidate/rollback status before editing.

## 1.24 Candidate — 2026-09-22 / Export Stems

中文：修复 Export Stems 在界面线程重复遍历整段音频和全部区域造成的长时间无响应。区域边界、相邻关系、峰值及增益按快照预计算，仅处理有效范围；保留 1.23 的重叠区域优先级及 Fade/Norm/Gain/全局和区域 EQ 规则。三轨渲染、已加载 ARA 本地 WAV 读取及文件写入在可取消后台任务中执行，显示进度。导出使用开始时的数据与设置，后续编辑不改变正在导出的内容。完成三轨临时写入后才替换目标文件；取消保留原文件。关闭编辑器会取消并等待任务安全退出。普通 VST3 播放定位与 ARA 播放路径未修改。

English: Fixed excessive synchronous work in Export Stems. Region boundaries, adjacency, peaks and gains are cached for the export snapshot, with rendering restricted to relevant ranges. The 1.23 overlap priority and Fade/Norm/Gain/global and per-region EQ semantics are preserved. Rendering, loaded ARA local-WAV reads and file writing run in a cancellable worker with progress. Later edits do not change the captured export. All three temporary WAVs are written before replacing targets; cancellation preserves existing stems. Closing the editor cancels and joins the worker safely. Ordinary VST3 transport and ARA playback paths are unchanged.

Status: Windows delivery and formal Plan B completed; public source tagged. Mac artifacts and Release checks are recorded with this version.

Implementation boundary: shared/StemExport.{h,cpp} owns offline rendering and the worker. PluginEditor captures settings/regions and recorded samples (one locked copy); for ARA it opens the already loaded local WAV reader, whose sample reads run on the worker. No worker host/ARA/processor/editor access, no detached thread or UI callback. The UI timer observes atomic completion and joins before destruction. Data remains in memory as before; extremely long sources still require source plus three full-length stem buffers. A stalled OS file operation can delay cancellation/close. Three final replacements are sequential, not a cross-file atomic transaction; an individual replacement error explicitly reports potentially replaced earlier stems. Preserve these limits in future handoffs.
Regression oracle: tests/LegacyStemExport123.h is a frozen extraction of 1.23 export DSP, intentionally slow and never linked into the plug-in. StemExportProbe compares samples and exercises real WAVs and worker lifetime. Do not use EasyTool's summing rules as a replacement for DeBreath's max-weight/tie-order rules.

Validation record (2026-09-22): Windows x64 VST3 build passed. 36 comparisons against frozen 1.23 DSP at 44.1/48/96 kHz passed with zero sample difference; background recorded/local-WAV export, WAV contents, cancellation preserving existing files, temporary-file cleanup, error handling and worker destruction passed. Existing editor/monitor/instance and transport probes passed; Steinberg loading check exited 0. Synthetic 1-second/100-region render: 1.21323 s legacy vs 0.000698 s candidate; 120 seconds/1000 regions: 0.0781656 s (render only, not disk I/O). No real DAW or macOS acceptance claimed. Installed 1.23 is unchanged.
Plan A deliverable: QQDeBreath-1.24 / QQDeBreath.vst3 in the local outputs area. No install overwrite, Plan B, Plan C/D or public release was performed.

## 2026-09-22 — 1.24 installation and user acceptance
After the Plan A record above, 1.24 was installed over the system VST3 with file parity verification. The user confirmed Export Stems is fixed. This does not itself designate a new Stable. Continue from 1.24 for 1.25 responsiveness work; preserve its export and transport fixes.

## 1.25 Candidate — 2026-09-22 / Parameter and Auto Apply responsiveness

中文：优化 Gain、Norm Target 及全局/区域 EQ Auto Apply 的拖动响应。Gain/Norm Target 通过缩放处理后波形缓存更新显示，不再重复扫描和处理整段音频。EQ 波形计算移至只保留最新请求的后台线程；拖动时保留上一份完整波形，最新计算完成后更新。试听参数仍独立更新。缓存区域峰值、用区间索引绘制波形，并将动态频谱限制到当前窗口涉及的区域；拖动不再触发无关的全文件静态频谱计算。连续区域 EQ 拖动的 ARA 更新采用限频而非不断推迟。Auto Apply 关闭时的预览/Apply 规则不变。

English: Improved Gain, Norm Target and global/selected EQ Auto Apply responsiveness. Gain/Target scale cached display data without rescanning or reprocessing the source. EQ waveform rendering uses a background worker with latest-request cancellation, keeping the last complete display until the newest result is ready. Audition parameters update independently. Region peaks are cached, painting uses an interval index, and dynamic spectra consider only regions overlapping the current window. Dragging no longer triggers unrelated whole-file static spectra. Continuous selected EQ edits throttle ARA updates instead of indefinitely postponing them. Manual preview/Apply behavior is preserved.

Scope: source-owned ARA background analysis, selection/history improvements and faster native analysis.

Status: Candidate; Windows build and focused display/export/transport regression validation passed. User confirmed 1.24 export fixed; designated Stable remains 1.23. No new macOS or real DAW acceptance claimed for 1.25.

Implementation: WaveformDisplayRender worker owns only immutable mono source snapshots, copied region/settings data and result buffers; it never accesses GUI/processor/ARA objects. New requests replace pending work and cooperatively cancel outdated work; the waveform timer adopts completed results. Destruction cancels/joins, source reload/clear invalidates results. Gain/Target are excluded from the expensive cache key. A separate fixed contribution preserves the zero-peak Norm fallback even when adjoining fades contain nonzero audio. Waveform region revisions avoid serializing every region EQ on each drag. Peak memo keys use source-local bounds and are cleared on source change. Static full-spectrum analysis remains for explicit page/Apply operations; dynamic spectra use cached source/peaks and nearby regions. Initial source loading still builds a mono snapshot and geometry caches; long source load time is outside this parameter-drag fix.
Reference inspected: QQ Super Compression 1.2.0 parameter callbacks and editor timer; reused the separation of parameter updates and display work, not its audio DSP. Test-only LegacyWaveform124 preserves the pre-change display for numerical comparison. Preserve tests/LegacyStemExport123 and the 1.24 export probe.

1.25 validation: Windows x64 VST3 built. Waveform regression: 24 cases at 44.1/48/96 kHz, each checking three scalar settings plus global and selected EQ; maximum relative display error 1.63409e-7. Scalar changes enqueue zero full renders. Latest EQ wins, same-length source replacement/clear and worker close passed. Synthetic 120-second/500-region test: 1000 scalar updates 125.27 ms; 500 EQ submissions 142.042 ms; paint 11.2246 ms. These measure internal display entry points, not real DAW end-to-end latency. 1.24 stem-export, editor-state and transport regressions and Steinberg load check passed. Audio processor, EQ DSP and stem-export source hashes unchanged.
1.25 delivery: local QQDeBreath-1.25 output folder, VST3 bundle plus source and verification records. System installation remains 1.24; no new installation, Stable promotion, Plan B/C/D or GitHub publication in this change.

## 2026-09-22 — 1.25 Stable and Plan B
The user reported the responsiveness improvement works well, designated 1.25 Stable and requested Plan B. Existing Windows 1.25 build, regression results and installation are inherited; no recompile, reinstall or repeated tests for promotion. Plan B is the complete source snapshot named QQDeBreathTool ARA VST3 AU 1.25 Stable in the established QQDeBreathTool source-backup tree, including project/ARA SDK, JUCE dependency source and the verified Windows bundle. Completion evidence is saved beside the active output records. Once reported complete this snapshot is frozen: do not revisit, refresh, hash, overwrite or clean it during later tasks. Earlier 1.23 backups were not accessed.

## 2026-09-22 — 1.25 Plan C / D continuation
User requested Plan C and D after Stable + Plan B completed. Plan B remains frozen and was not accessed. Public plug-in repository: Ziqing-Gu/QQDeBreath-ARA-VST3-AU, not the standalone repository. Build source commit: bb7a0352de9393cfd5f5a7ddbf74f1c22c89aead; macOS workflow run: 35668163947. Windows uses the existing user-accepted local 1.25 binary. All compiled first-party sources match that public commit, allowing only CRLF/LF normalization; the 764 ARA SDK files also match the recorded submodule checkout. Original bilingual 1.20 manuals are retained unchanged. Documentation follow-ups do not change the binary source.

1.25 delivery status: all three macOS jobs completed successfully; Windows Actions was skipped. The four platform archives and original bilingual illustrated manuals are delivered with current bilingual installation guides. Public Release v1.25 contains nine verified assets including complete dependency source; all uploaded names, byte counts and SHA-256 digests match the local verified assets. New Mac host acceptance is not claimed.


## 1.26 Stable — 2026-10-24 / ARA multi-source rendering

中文：修复同一 ARA 文档超过 32 个音源后旧音源被淘汰、离线渲染为静音的问题。缓存随音源生命周期管理，局部恢复只更新指定音源，保留有效原始音频；支持持久 ID 重映射、工程级独立存档和未分析音源。素材在宿主模型线程允许读取时完成预载，不在实时回调中读取宿主音频。ARA 区域边界、邻接、增益及归一化系数预计算，保留原来的重叠优先级和 EQ 规则；离线渲染不再依赖 Playing 标志。

English: Fixed older sources becoming silent when an ARA document exceeds 32 audio sources. Caches now follow source lifetime; filtered restore preserves unaffected edits and valid raw audio, including source-ID remapping, document-only archives and untouched sources. Missing audio is preloaded on the host model thread before playback, without host reads in the realtime callback. ARA region bounds, adjacency and gains are precomputed while retaining legacy overlap and EQ semantics. Offline rendering no longer requires the Playing flag.

Status: Stable, explicitly designated by the user on 2026-10-24. Existing Windows build and five regression probes are reused; 1.26 is installed with file-hash parity. Previous Stable/rollback: 1.25. Original Studio One 7 project validation has not been reported; Stable designation does not imply a new DAW test or macOS build.

Tradeoff: First source loading and undo reactivation can wait for synchronous host reads; active audio is still cached in full, so memory follows active source duration/channel count. Sources are released on undo deactivation or destruction. Host read failure does not publish partial audio and is retried at a later model edit/load request. Real Studio One 7 acceptance has not been reported. macOS builds are verified separately in the release record.

兼容与升级：存档 schema 仍为 version 1；保留已有分析与 EQ 规则。旧版已渲染出的静音文件需要从原始事件重新渲染。
Compatibility and upgrade: archive schema remains version 1 and existing analysis/EQ rules are preserved. Previously rendered silent files must be rendered again from original events.


1.26 validation: real ARA 22 renderers / 44 sources, 44 audible and 0 silent; 48 legacy DSP cases match exactly. Existing transport, editor, export and waveform probes passed. Windows installation matches the validated binary. User-designated Stable and Plan B completion are recorded locally; the completed backup remains frozen and must not be accessed by later plans.
1.26 implementation: source lifecycle callbacks own raw-audio preload and invalidation on the model thread; no host reads occur in the realtime renderer. Filtered archive restore preserves unrelated state and valid audio. Prepared immutable region metadata retains legacy overlap/tie/EQ semantics. Tests/LegacyAra125.h is a test-only frozen DSP oracle.
Plan C/D: reuse the verified Windows binary, build the three macOS variants from this public source, and publish versioned artifacts. The user requested the existing illustrated manuals remain unchanged. No original-project Studio One acceptance is claimed.


## 1.27 Stable — 2026-10-24 / Apple Clang portability

中文：将存档恢复时清除失效导出路径的赋值明确写为 `juce::File {}`，解决 Apple Clang 的重载歧义。保留 1.26 的多音源缓存、恢复和渲染修复；音频处理算法未改变。界面版本为 ARA 1.27，内部元数据为 1.0.27。

English: Explicitly assign `juce::File {}` when clearing an obsolete exported-WAV path during archive restore, resolving Apple Clang's ambiguous assignment. The 1.26 multi-source cache, archive and render fixes remain intact; audio processing algorithms are unchanged. UI version: ARA 1.27; internal metadata: 1.0.27.

Validation: Windows Release build, all five regression probes and actual VST3 factory loading passed. 22 renderers / 44 sources: 44 audible and 0 silent. 48 legacy ARA DSP cases match sample-for-sample. PE and module metadata are 1.0.27. macOS build and signature checks are recorded with this release. No original Studio One 7 project acceptance is claimed.

Status: Stable, explicitly designated by the user on 2026-10-24. Windows is installed with verified file-hash parity; completed Windows tests are reused. Previous Stable/rollback: 1.26. Mac build and publication results are recorded separately. Original bilingual PDF manuals remain unchanged.


1.26 macOS build failed on ambiguous juce::File assignment; 1.27 resolves it. The original 1.26 tag and Windows history are preserved. No 1.26 macOS artifacts or formal Release were published.


## 2026-10-27 — Maintenance direction / 后续维护方向

用户决定把后续 ARA/VST3/AU 功能与修复集中到 QQEasyTool。QQDeBreath 插件版停留在 1.29；它可能是最后一版。保留现有源码、成品和历史下载，除非用户以后明确重新授权，不再修改 QQDeBreath 插件产品。本次已授权的 1.29 发布收尾继续完成。独立应用不在此次停止维护范围内。

Future ARA/VST3/AU development is focused on QQEasyTool. QQDeBreath plug-ins remain at 1.29, which may be the final plug-in release. Existing source, binaries and historical downloads remain available. Further QQDeBreath plug-in product changes require a new explicit user request. The already-authorized 1.29 publication is completed; the standalone application is separate.

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


Current release: Stable; new Plan B completed and frozen. Reuse the delivered Windows binary. Mac builds and final release verification are recorded separately. History retains 64 steps per source in the current session; no full undo-chain persistence across restart is claimed.
