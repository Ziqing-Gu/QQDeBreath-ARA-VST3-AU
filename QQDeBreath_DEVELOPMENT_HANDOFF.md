# QQDeBreath current stable baseline

- Current baseline: **1.23 Stable**, internal VST3 version **1.0.23**.
- User confirmed correct behavior on 2026-09-20.
- Preserve stopped waveform selections until the first actual playing audio block; Play must start from the selected local sample.
- Later DAW relocation restores original timeline alignment. A host-reported seek while stopped cancels a pending selection.
- Read STABLE_RELEASE.json, VERSION_ARA_1_23.txt, and the final entry of DEVELOPMENT_HISTORY.md before changing transport behavior.
- Historical 1.20 handoff is preserved inside DEVELOPMENT_HISTORY.md; append future development entries.
- Active source: repository root.
- Verified output: local Plan A record; not stored in this source tree.
- Formal Plan B: completed historical snapshot; frozen and not inspected by subsequent plans.
- Large files and local paths are retained only in the maintainer's local release records.
- 2026-09-20: QQEasyTool 1.04 is now Stable; the user authorized Plan C and Plan D for both plugin projects. Publish QQDeBreath only to Ziqing-Gu/QQDeBreath-ARA-VST3-AU.
- Preserve LICENSE, LICENSE_POLICY_CHANGE.md, README notices and all third-party licenses.

Authority update: AI_DEVELOPMENT_HANDOFF.md is the current authoritative handoff; this file remains as historical context.
