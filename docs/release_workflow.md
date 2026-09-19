# Release workflow / 发布流程

Current Stable: 1.23; rollback: 1.20. Authoritative handoff: ../AI_DEVELOPMENT_HANDOFF.md.
Plan A creates and verifies the local Windows plug-in. Plan B is a one-time full-source snapshot and is frozen after completion.
Plan C synchronizes public source and bilingual history, reuses the verified Plan A Windows output, builds only three macOS formats from a known commit, verifies all four formats, and creates the actual desktop folder.
Plan D publishes the verified assets to this same repository's Release and updates README direct download links.
GitHub Windows builds are optional: manually dispatch with build_windows=true only when cloud reproduction is explicitly requested. The default is false.
Document-only corrections reuse the existing verified artifacts and do not trigger another build.

当前 Stable 为 1.23，回滚版本为 1.20。权威交接文件：../AI_DEVELOPMENT_HANDOFF.md。
Plan B 完成后冻结，后续阶段不得读取、核验或刷新。Plan C 复用 Plan A Windows 成品，云端默认只构建三类 macOS，并生成桌面实际成品目录。Plan D 在同一项目仓库发布 Release 和真实下载入口。
本次发布补交不修改音频源码、不重新构建、不重新安装；原 Windows CI 已额外运行的事实保留，不能声称它未发生。
