# QQDeBreath development history

## 1.25 Stable — 2026-09-22 / Parameter and Auto Apply responsiveness

中文：优化 Gain、Norm Target 及全局/区域 EQ Auto Apply 的拖动响应。Gain/Norm Target 通过缩放处理后波形缓存更新显示，不再重复扫描和处理整段音频。EQ 波形计算移至只保留最新请求的后台线程；拖动时保留上一份完整波形，最新计算完成后更新。试听参数仍独立更新。缓存区域峰值、用区间索引绘制波形，并将动态频谱限制到当前窗口涉及的区域；拖动不再触发无关的全文件静态频谱计算。连续区域 EQ 拖动的 ARA 更新采用限频而非不断推迟。Auto Apply 关闭时的预览/Apply 规则不变。

English: Improved Gain, Norm Target and global/selected EQ Auto Apply responsiveness. Gain/Target scale cached display data without rescanning or reprocessing the source. EQ waveform rendering uses a background worker with latest-request cancellation, keeping the last complete display until the newest result is ready. Audition parameters update independently. Region peaks are cached, painting uses an interval index, and dynamic spectra consider only regions overlapping the current window. Dragging no longer triggers unrelated whole-file static spectra. Continuous selected EQ edits throttle ARA updates instead of indefinitely postponing them. Manual preview/Apply behavior is preserved.

Scope: display/editor only; audio processor, EQ DSP, analysis and 1.24 stem-export implementation remain unchanged. No new audio-parameter smoothing algorithm is introduced. Waveform updates may lag behind EQ dragging by design. Gain/Target cache scaling can differ from the previous display by float rounding; it does not change playback or exported audio. Original playback synchronization and fixed Export Stems behavior are retained.

Status: Stable, explicitly designated by the user on 2026-09-22 after accepting 1.25 responsiveness. Windows validation is inherited unchanged; no new macOS acceptance is claimed.


## 1.24 Candidate — 2026-09-22 / Export Stems

中文：修复 Export Stems 在界面线程重复遍历整段音频和全部区域造成的长时间无响应。区域边界、相邻关系、峰值及增益按快照预计算，仅处理有效范围；保留 1.23 的重叠区域优先级及 Fade/Norm/Gain/全局和区域 EQ 规则。三轨渲染、已加载 ARA 本地 WAV 读取及文件写入在可取消后台任务中执行，显示进度。导出使用开始时的数据与设置，后续编辑不改变正在导出的内容。完成三轨临时写入后才替换目标文件；取消保留原文件。关闭编辑器会取消并等待任务安全退出。普通 VST3 播放定位与 ARA 播放路径未修改。

English: Fixed excessive synchronous work in Export Stems. Region boundaries, adjacency, peaks and gains are cached for the export snapshot, with rendering restricted to relevant ranges. The 1.23 overlap priority and Fade/Norm/Gain/global and per-region EQ semantics are preserved. Rendering, loaded ARA local-WAV reads and file writing run in a cancellable worker with progress. Later edits do not change the captured export. All three temporary WAVs are written before replacing targets; cancellation preserves existing stems. Closing the editor cancels and joins the worker safely. Ordinary VST3 transport and ARA playback paths are unchanged.

Status: Candidate, not user-accepted Stable. Stable/rollback remains 1.23. Plan A Windows build and automated export/transport regression checks passed; no new macOS build or real DAW acceptance claimed.


## Imported historical handoff for 1.20

# QQDeBreath 开发历史与 AI 交接说明

> 本文件用于把 QQDeBreath 当前开发状态、已确认的问题、用户决定和后续开发规则交给新的 AI 或开发者。接手项目后应先完整阅读本文件，再修改代码。

## 一、后续 AI 必须遵守的规则

1. 每次更新代码后，必须在源码根目录的 `DEVELOPMENT_HISTORY.md` 末尾追加本次开发经过，不得删除、覆盖或改写旧记录。
2. 如果源码根目录还没有 `DEVELOPMENT_HISTORY.md`，应创建它，并先将本文件中的历史信息整理进去。
3. 每次记录至少包含：日期、版本号、用户目标、问题表现、排查过程、根本原因、设计决定、修改文件、构建结果、测试结果、未完成事项和下一步建议。
4. 不能只写“修复了某问题”。必须说明为什么会发生、尝试过什么、最后为什么采用当前方案，以便新的 AI 能继续跟进。
5. 修改版本号时，必须同步检查 CMake 项目版本、`src/Version.h`、插件 Help/界面显示、GitHub Actions 产物名、安装说明和发布说明，不能只修改内部版本号。
6. 不要因为处理 macOS 安装问题而改动呼吸识别模型、ARA 音频处理、区域编辑、EQ、Fade、Norm、Gain、监听或导出算法。
7. 不要删除现有的 1.01～1.20 更新说明、阶段文档、旧版安装说明和历史实现。它们是项目开发过程的一部分。
8. 如果旧实现已不再参与构建，应在历史记录中标明“已停用/仅供参考”，不要在没有确认的情况下直接删除。

## 二、项目当前状态

- 当前源码版本：QQDeBreath ARA / VST3 / AU 1.20。
- 框架：JUCE 8.0.13。
- 插件同时提供普通 VST3 录制模式和 ARA 工作模式。
- macOS 构建包括：Apple Silicon arm64 VST3、Intel x86_64 VST3、Universal 2 AU。
- Windows 构建包括：x64 VST3。
- 1.20 的主要修复：ARA 工程关闭后重新打开时，后台预载已恢复 Audio Source 的缓存，实时线程不再在缓存缺失时读取宿主 ARA Source，以避免播放噼啪声。
- 现有识别模型、Breath/Noize 区域、监听、Fade、Norm、Gain、EQ、工程保存和三轨导出逻辑，应继续作为当前功能基线。

## 三、2026-08-12 macOS/SIP 问题调查

### 用户报告

一位 macOS 用户报告：开启 SIP（系统完整性保护）后，Cubase 扫描 QQDeBreath 时会报错并将插件排除；关闭 SIP 时曾经可以使用。

### 已确认的发布现状

QQDeBreath 1.20 的 GitHub Actions 对 macOS 插件执行的是 ad-hoc 临时签名：

```bash
codesign --force --deep --sign - "$plugin_path"
```

当前发布流程没有：

- Apple Developer Program 的 `Developer ID Application` 正式签名；
- Secure Timestamp；
- Apple Notary Service 公证；
- 公证票据 Staple；
- 完整的 Gatekeeper 发布验证。

因此，当前 macOS 包不能保证在所有开启正常安全保护的 Mac、Cubase版本和下载方式下无提示加载。

### 与 DB-5035 Qing Compressor 的对比结果

用户的 DB-5035 Qing Compressor 在同一位网友处没有报告相同问题，但其 macOS GitHub Actions 同样没有 Developer ID 签名和 Apple 公证，工作流中甚至没有显式 `codesign` 步骤。

所以不能得出“压缩器的签名方式正确，而 QQDeBreath 的签名方式错误”的结论。压缩器暂时能够加载，可能与以下因素有关：

- 它在 SIP 关闭期间已经被 Cubase 扫描并缓存为可用；
- 两个 ZIP 或插件的 `com.apple.quarantine` 下载隔离属性不同；
- 下载、传输和解压工具不同；
- Gatekeeper 或 Cubase 已保存过往放行/扫描结果；
- QQDeBreath 是 ARA 插件，会经过与普通 VST3 不完全相同的注册、初始化或扫描路径；
- 用户实际安装的二进制不一定与目前看到的源码工作流完全一致；
- 还可能存在尚未获得日志证实的 ARA 扫描期崩溃或架构选择错误。

重要结论：缺少正式签名和公证是已确认的发布限制，但在没有实际 Cubase 错误信息、扫描日志和用户机器上的二进制检查结果前，不能声称它是本次故障的唯一原因。

## 四、用户已经作出的决定

用户目前不准备购买或加入 Apple Developer Program，也不准备购买用于公开分发的 Developer ID 证书相关服务。

后续 AI 必须尊重这一决定：

- 不要把“购买 Apple Developer 证书”作为继续开发 QQDeBreath 的前置条件。
- 不要反复劝说用户购买证书。
- 不要承诺在没有 Developer ID 和公证的情况下，实现与正式商业插件完全相同的无提示安装体验。
- 不要把关闭 SIP 写成推荐安装步骤，也不要要求普通用户长期关闭 SIP。
- 可以继续维护 ad-hoc 测试包和可信来源下的手动安装说明，但必须明确它属于尽力兼容方案，不能保证所有 macOS/DAW环境都接受。
- Windows 版本、插件功能、ARA逻辑、识别算法、UI和其他开发工作可以正常继续，不受这一决定影响。

## 五、没有证书时允许采用的 macOS 方案

在用户明确知道风险且插件来自可信源码/官方发布位置时，可以在安装说明中保留以下顺序：

1. 确认下载了与 DAW 运行架构匹配的 VST3：原生 Apple Silicon DAW 使用 arm64；Rosetta/Intel DAW 使用 x86_64。
2. 将完整 `.vst3` 文件夹复制到 `/Library/Audio/Plug-Ins/VST3/`。
3. 如果 macOS 提示阻止，在“系统设置 → 隐私与安全性”中尝试“仍要打开”。
4. 对确认可信的插件，可删除该插件自身的下载隔离属性：

```bash
sudo xattr -dr com.apple.quarantine "/Library/Audio/Plug-Ins/VST3/QQDeBreath.vst3"
```

5. 重启 Cubase，在插件管理器中重新扫描或重新激活。

必须同时说明：上述方法不能代替 Developer ID 签名和 Apple 公证，也不保证在所有系统上有效。不要提供关闭 SIP 作为常规解决方案。

## 六、如果以后获得新的报错资料，应如何继续排查

如果用户以后拿到网友的原始报错窗口、Cubase扫描日志或实际插件包，后续 AI 不应直接假定还是签名问题。应分别检查：

```bash
codesign -dv --verbose=4 "/Library/Audio/Plug-Ins/VST3/QQDeBreath.vst3" 2>&1
codesign --verify --deep --strict --verbose=4 "/Library/Audio/Plug-Ins/VST3/QQDeBreath.vst3"
xattr -lr "/Library/Audio/Plug-Ins/VST3/QQDeBreath.vst3"
lipo -archs "/Library/Audio/Plug-Ins/VST3/QQDeBreath.vst3/Contents/MacOS/QQDeBreath"
```

并与能够加载的 DB-5035 插件执行同样的检查。然后区分以下问题：

- Gatekeeper/下载隔离；
- 无签名、ad-hoc签名或签名损坏；
- arm64/x86_64 架构不匹配；
- Cubase插件黑名单或扫描缓存；
- ARA工厂注册或宿主兼容性；
- 插件实例化时崩溃；
- 第三方依赖或 bundle 内容在签名后被修改。

只有获得证据后，才能决定是否修改代码。不要为了“试试看”改动 DSP 或 ARA核心处理。

## 七、后续正常开发流程

1. 修改前阅读本文件、`README.md`、最新版本发布说明和 `DEVELOPMENT_HISTORY.md`。
2. 明确本次修改是否涉及 DSP、ARA、UI、工程状态、导出、构建或安装。
3. 保持参数 ID、工程状态兼容性和已有音频结果，除非用户明确同意破坏兼容。
4. 完成代码修改后执行可用的构建和自动测试。
5. 无法在真实 Cubase、Logic或 macOS环境验证的项目，必须写成“待人工验证”，不能写成“已修复并验证”。
6. 更新所有用户可见版本号和说明。
7. 在 `DEVELOPMENT_HISTORY.md` 末尾追加本次完整经过。
8. 交付时附带一份面向 GitHub/Codex 的更新说明，列出改动、验证结果和未完成事项。

## 八、开发历史追加模板

后续 AI 每次完成更新后，将以下内容追加到 `DEVELOPMENT_HISTORY.md` 末尾：

```markdown
## YYYY-MM-DD — 版本 X.XX

### 用户目标

-

### 问题表现与复现条件

-

### 排查经过

-

### 根本原因

-

### 设计决定

-

### 修改文件

- `路径`：修改内容。

### 构建与测试

- 已完成：
- 待人工验证：

### 未完成事项与下一步

-
```

## 九、当前交接结论

QQDeBreath 项目仍然可以继续开发。当前不能彻底保证的是：在不购买 Developer ID、也不进行 Apple 公证的前提下，让所有正常开启安全保护的 Mac 都像安装商业插件一样无提示接受该插件。

这是一项 macOS公开分发限制，不代表 QQDeBreath 的呼吸识别、ARA处理或其他功能已经无法继续维护。后续开发应继续改进插件本身，同时如实标注 macOS免证书发布的兼容性边界。


## 2026-09-20 — 1.23 Stable / Windows VST3 transport correction

用户目标：插件内点击波形后，按播放从所选音频位置开始；在 DAW 重新定位时，清除试听偏移并恢复工程对齐。

- 1.21 将停止也作为复位条件，改变了原有试听行为；用户未认可。
- 1.22 保留原地停止/继续，但用户仍报告点击波形后按播放回到 DAW 位置；用户未认可。
- 1.23 将停止时的选择保留到首个实际渲染音频块，再用实际宿主时钟建立对应关系。普通 VST3 界面改为读取音频线程发布的位置快照。后续宿主定位变化仍恢复工程时间线。
- 回归测试覆盖 44.1/48/96 kHz、采样数/秒时钟、重复停止回调、启动时的时间差、零长度块、前后跳转、循环及逐采样实际输出；编辑器状态、监听、锁竞争回退和多实例隔离测试通过。
- Steinberg VST3 加载检查通过；用户随后明确反馈“这个版本就对了”，要求设为 Stable 并执行 Plan B。
- Stable 标记仅更新状态、文档和验收记录，保留用户测试通过的 1.23 二进制。
- 当前工作只涉及 QQDeBreath；QQEasyTool 的修复仍暂停。此次范围到 Plan B。
- JUCE 8.0.13 commit: 7c9d3783b127263d72bb65fe0a7e2dc8a02a7ac2。
- 原有 Qing Audio 非商业许可证及第三方许可保留。
