# Release workflow / 发布流程

Current Stable: 1.27; rollback: 1.26. Authoritative handoff: ../AI_DEVELOPMENT_HANDOFF.md.
Plan A creates and verifies the local Windows plug-in. Plan B is a one-time full-source snapshot and is frozen after completion.
Plan C synchronizes public source and bilingual history, reuses the verified Plan A Windows output, builds only three macOS formats from a known commit, verifies all four formats, and creates the actual desktop folder.
Plan D publishes the verified assets to this same repository's Release and updates README direct download links.
GitHub Windows builds are optional: manually dispatch with build_windows=true only when cloud reproduction is explicitly requested. The default is false.
Document-only corrections reuse the existing verified artifacts and do not trigger another build.

当前 Stable 为 1.27，回滚版本为 1.26。权威交接文件：../AI_DEVELOPMENT_HANDOFF.md。
Plan B 完成后冻结，后续阶段不得读取、核验或刷新。Plan C 复用 Plan A Windows 成品，云端默认只构建三类 macOS，并生成桌面实际成品目录。Plan D 在同一项目仓库发布 Release 和真实下载入口。
本次发布补交不修改音频源码、不重新构建、不重新安装；原 Windows CI 已额外运行的事实保留，不能声称它未发生。

1.26: Windows binaries and five local regression probes are reused. Only macOS arm64 VST3, x86_64 VST3 and Universal 2 AU are built by default. Numeric version changes also refresh the Windows PE resource through the CMake Info.txt dependency. Illustrated manuals are unchanged original editions.


## 1.27 Stable — 2026-10-24 / Apple Clang portability

中文：将存档恢复时清除失效导出路径的赋值明确写为 `juce::File {}`，解决 Apple Clang 的重载歧义。保留 1.26 的多音源缓存、恢复和渲染修复；音频处理算法未改变。界面版本为 ARA 1.27，内部元数据为 1.0.27。

English: Explicitly assign `juce::File {}` when clearing an obsolete exported-WAV path during archive restore, resolving Apple Clang's ambiguous assignment. The 1.26 multi-source cache, archive and render fixes remain intact; audio processing algorithms are unchanged. UI version: ARA 1.27; internal metadata: 1.0.27.

Validation: Windows Release build, all five regression probes and actual VST3 factory loading passed. 22 renderers / 44 sources: 44 audible and 0 silent. 48 legacy ARA DSP cases match sample-for-sample. PE and module metadata are 1.0.27. macOS build and signature checks are recorded with this release. No original Studio One 7 project acceptance is claimed.

Status: Stable, explicitly designated by the user on 2026-10-24. Windows is installed with verified file-hash parity; completed Windows tests are reused. Previous Stable/rollback: 1.26. Mac build and publication results are recorded separately. Original bilingual PDF manuals remain unchanged.


1.26 macOS build failed on ambiguous juce::File assignment; 1.27 resolves it. The original 1.26 tag and Windows history are preserved. No 1.26 macOS artifacts or formal Release were published.
