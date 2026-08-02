# QQDeBreath ARA / VST3 / AU 1.20

## 中文更新

- 修复 ARA 工程关闭后重新打开时播放出现噼啪声的问题。
- 工程状态恢复后，Document Controller 会自动启动后台缓存预载，读取当前项目中已保存状态对应的 ARA Audio Source。
- 播放器的 realtime callback 不再在缓存缺失时调用 ARA sample-access API，避免在音频线程中执行不确定耗时的宿主读取。
- 缓存尚未完成时，插件输出安全静音块；缓存发布后自动恢复正常处理。
- 分析器、模型、Breath / Noize 区域、监听、Fade、Norm、Gain、EQ、工程保存和三轨导出逻辑保持不变。

## English Changes

- Fixed crackle during ARA playback after closing and reopening a DAW project.
- After project state restoration, the Document Controller automatically starts a worker-thread preload for every restored ARA Audio Source required by the project.
- The realtime playback callback no longer calls the ARA sample-access API when its immutable source cache is missing.
- While the cache is not ready, the renderer outputs a safe silent block; playback resumes automatically when the cache is published.
- The analyzer, model, Breath / Noize regions, monitoring, Fade, Norm, Gain, EQ, project state, and three-stem export behavior are unchanged.

## 验证 / Validation

- Windows x64 VST3 Release build: pending local build.
- Reopen-project cache warmup: requires host verification in Cubase, REAPER, or Studio One.
- Realtime callback source-reader fallback: removed from the renderer.