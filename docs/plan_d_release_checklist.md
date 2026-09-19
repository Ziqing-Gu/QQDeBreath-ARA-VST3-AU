# Current Plan definitions / 现行计划定义

The checklist below is historical (old numbering). Since 2026-09-01, cross-platform desktop packaging is Plan C; Plan D is Release publication and README direct downloads. See [current release workflow](release_workflow.md).

以下保留历史清单；现行跨平台桌面交付属于 Plan C，Plan D 为 Release 发布与下载入口。Plan B 完成后冻结，不得作为后续同步目录。

# Plan D Release Checklist

Plan D must include all Plan A, Plan B, and Plan C deliverables, plus:

1. Build and verify Windows x64 VST3.
2. Build and verify macOS Apple Silicon VST3.
3. Build and verify macOS Intel x86_64 VST3.
4. Build and verify macOS Universal 2 AU/ARA.
5. Create one desktop release folder with `Win` and `Mac` subfolders. Put the Windows VST3 archive in `Win`, put the Apple Silicon VST3, Intel VST3, and Universal 2 AU archives in `Mac`, and keep the Chinese/English installation guides at the release-folder root.
6. Include Chinese and English installation guides covering both Windows and macOS installation. Name them `QQDeBreath <version> Windows与macOS安装使用说明（中文）.txt` and `QQDeBreath-<version>-Windows-macOS-INSTALL.txt`.
7. Verify architectures and SHA-256 checksums internally, but do not place architecture proof text files or `SHA256SUMS.txt` in the desktop end-user package.
8. Update the bilingual GitHub README with the current version and release changes.
