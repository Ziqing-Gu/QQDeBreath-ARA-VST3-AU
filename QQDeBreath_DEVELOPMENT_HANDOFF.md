# QQDeBreath current stable baseline

- Current baseline: **1.23 Stable**, internal VST3 version **1.0.23**.
- User confirmed correct behavior on 2026-09-20.
- Preserve stopped waveform selections until the first actual playing audio block; Play must start from the selected local sample.
- Later DAW relocation restores original timeline alignment. A host-reported seek while stopped cancels a pending selection.
- Read STABLE_RELEASE.json, VERSION_ARA_1_23.txt, and the final entry of DEVELOPMENT_HISTORY.md before changing transport behavior.
- Historical 1.20 handoff is preserved inside DEVELOPMENT_HISTORY.md; append future development entries.
- Active source: D:\Codex\Workspaces\QQDeBreathTool-QQEasyTool\QQDeBreathTool\ARA-VST3-AU
- Verified output: D:\Codex\Outputs\QQDeBreath-1.23
- Formal Plan B source snapshot: D:\备份文件\Vibe Coding\QQDeBreathTool\源代码\QQDeBreathTool ARA VST3 AU 1.23 Stable
- Large builds stay in D:\Codex\Temp. D:\Codex\Archives is an additional safeguard, not the formal Plan B backup.
- 2026-09-20: QQEasyTool 1.04 is now Stable; the user authorized Plan C and Plan D for both plugin projects. Publish QQDeBreath only to Ziqing-Gu/QQDeBreath-ARA-VST3-AU.
- Preserve LICENSE, LICENSE_POLICY_CHANGE.md, README notices and all third-party licenses.
