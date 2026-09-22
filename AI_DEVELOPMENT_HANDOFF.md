# QQDeBreath authoritative development handoff

Current Stable: 1.25 (internal 1.0.25), designated by the user on 2026-09-22.
Current Candidate: none.
Previous designated Stable: 1.23. Immediate rollback package: 1.24 (export fix accepted).
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

Status: Candidate, not user-accepted Stable. Stable/rollback remains 1.23. Plan A Windows build and automated export/transport regression checks passed; no new macOS build or real DAW acceptance claimed.

Implementation boundary: shared/StemExport.{h,cpp} owns offline rendering and the worker. PluginEditor captures settings/regions and recorded samples (one locked copy); for ARA it opens the already loaded local WAV reader, whose sample reads run on the worker. No worker host/ARA/processor/editor access, no detached thread or UI callback. The UI timer observes atomic completion and joins before destruction. Data remains in memory as before; extremely long sources still require source plus three full-length stem buffers. A stalled OS file operation can delay cancellation/close. Three final replacements are sequential, not a cross-file atomic transaction; an individual replacement error explicitly reports potentially replaced earlier stems. Preserve these limits in future handoffs.
Regression oracle: tests/LegacyStemExport123.h is a frozen extraction of 1.23 export DSP, intentionally slow and never linked into the plug-in. StemExportProbe compares samples and exercises real WAVs and worker lifetime. Do not use EasyTool's summing rules as a replacement for DeBreath's max-weight/tie-order rules.

Validation record (2026-09-22): Windows x64 VST3 build passed. 36 comparisons against frozen 1.23 DSP at 44.1/48/96 kHz passed with zero sample difference; background recorded/local-WAV export, WAV contents, cancellation preserving existing files, temporary-file cleanup, error handling and worker destruction passed. Existing editor/monitor/instance and transport probes passed; Steinberg loading check exited 0. Synthetic 1-second/100-region render: 1.21323 s legacy vs 0.000698 s candidate; 120 seconds/1000 regions: 0.0781656 s (render only, not disk I/O). No real DAW or macOS acceptance claimed. Installed 1.23 is unchanged.
Plan A deliverable: QQDeBreath-1.24 / QQDeBreath.vst3 in the local outputs area. No install overwrite, Plan B, Plan C/D or public release was performed.

## 2026-09-22 — 1.24 installation and user acceptance
After the Plan A record above, 1.24 was installed over the system VST3 with file parity verification. The user confirmed Export Stems is fixed. This does not itself designate a new Stable. Continue from 1.24 for 1.25 responsiveness work; preserve its export and transport fixes.

## 1.25 Candidate — 2026-09-22 / Parameter and Auto Apply responsiveness

中文：优化 Gain、Norm Target 及全局/区域 EQ Auto Apply 的拖动响应。Gain/Norm Target 通过缩放处理后波形缓存更新显示，不再重复扫描和处理整段音频。EQ 波形计算移至只保留最新请求的后台线程；拖动时保留上一份完整波形，最新计算完成后更新。试听参数仍独立更新。缓存区域峰值、用区间索引绘制波形，并将动态频谱限制到当前窗口涉及的区域；拖动不再触发无关的全文件静态频谱计算。连续区域 EQ 拖动的 ARA 更新采用限频而非不断推迟。Auto Apply 关闭时的预览/Apply 规则不变。

English: Improved Gain, Norm Target and global/selected EQ Auto Apply responsiveness. Gain/Target scale cached display data without rescanning or reprocessing the source. EQ waveform rendering uses a background worker with latest-request cancellation, keeping the last complete display until the newest result is ready. Audition parameters update independently. Region peaks are cached, painting uses an interval index, and dynamic spectra consider only regions overlapping the current window. Dragging no longer triggers unrelated whole-file static spectra. Continuous selected EQ edits throttle ARA updates instead of indefinitely postponing them. Manual preview/Apply behavior is preserved.

Scope: display/editor only; audio processor, EQ DSP, analysis and 1.24 stem-export implementation remain unchanged. No new audio-parameter smoothing algorithm is introduced. Waveform updates may lag behind EQ dragging by design. Gain/Target cache scaling can differ from the previous display by float rounding; it does not change playback or exported audio. Original playback synchronization and fixed Export Stems behavior are retained.

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
