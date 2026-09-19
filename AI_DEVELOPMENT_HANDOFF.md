# QQDeBreath authoritative development handoff

Current Stable: 1.23 (internal version 1.0.23), designated by the user on 2026-09-20.
Current Candidate: none.
Previous stable / rollback: 1.20; 1.21 and 1.22 are rejected candidates, not rollback baselines.
Platforms: Windows x64 VST3; macOS arm64 VST3, x86_64 VST3, Universal 2 AU; ARA-capable shared plug-in.
Development: JUCE 8.0.13, CMake, MSVC 2022 / Xcode.
Source of truth for this release's binary code: 45904d540e40247dc2606ee64c6b8d2d85fb4404.
The tag v1.23 remains unchanged; later commits may correct documentation/workflow only.

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
