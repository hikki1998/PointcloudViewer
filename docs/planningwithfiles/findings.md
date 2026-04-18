# 内嵌录屏替代 `ffmpeg.exe` 发现记录

## 当前实现现状

### 1. 当前录屏后端是外部进程

当前代码位置：
- `src/gui/MainWindow.Actions.cpp`
- `src/gui/MainWindow.Core.cpp`
- `src/gui/MainWindow.Backstage.cpp`
- `src/gui/MainWindow.SettingsStore.cpp`

当前录屏链路：
- `Ctrl+Shift+R` 或 Ribbon `Capture` 组触发 `toggleScreenRecording()`
- `MainWindow.Core.cpp` 中通过 `QStandardPaths::findExecutable("ffmpeg")` 查找 `ffmpeg`
- 使用 `QProcess` 启动外部 `ffmpeg.exe`
- 停止录制时向外部进程写入 `q\n`
- 成功/失败依赖外部进程退出码与诊断输出

结论：
- 当前 UI、设置页、保存路径和自动保存逻辑已经具备复用价值。
- 真正要替换的是录屏执行后端。

### 2. 当前实现的主要工程问题

- 依赖外部 `ffmpeg.exe`，部署不内聚。
- 录制目标依赖 `gdigrab` + `title=%1`，本质是按窗口标题抓取，不够稳。
- 生命周期和错误处理严重依赖外部进程行为。

结论：
- 替换重点不是界面，而是把采集、编码、文件落盘做成内部模块。

### 3. 当前仓库没有现成平台接入基础

检索结果：
- `src/` 与 `cmake/` 下未发现现成的：
  - `Windows.Graphics.Capture`
  - `GraphicsCaptureItem`
  - `Direct3D11CaptureFramePool`
  - `d3d11`
  - `dxgi`
  - `mfplat`
  - `mfreadwrite`
  - `mfuuid`
  - `windowsapp`
  - `CppWinRT`

结论：
- 这是一次新的平台能力接入，不是单纯替换几行调用。

## 技术选型结论

### 4. 推荐路线

- 采集：`Windows.Graphics.Capture`
- 帧承载：`D3D11`
- 编码/封装：`Media Foundation Sink Writer`

原因：
- 项目本身就是 Windows Qt 桌面程序。
- 可以按 `HWND` 精确绑定当前主窗口。
- 不再依赖外部 exe。
- 后续扩展能力更顺。

### 5. 当前不推荐的路线

- 继续依赖外部 `ffmpeg.exe`
- 引入 `libobs`
- 把 FFmpeg 动态库直接塞回 `MainWindow.Core.cpp`

原因：
- 要么依赖仍然偏重，要么模块边界会继续恶化。

## 架构建议

### 6. 新模块应独立于 `MainWindow`

建议新增：
- `src/capture/ScreenRecordingTypes.*`
- `src/capture/ScreenRecorder.h`
- `src/capture/WindowsGraphicsCaptureRecorder.*`
- `src/capture/MediaFoundationWriter.*`
- `src/capture/D3D11Helpers.*`

`MainWindow` 保留：
- 动作入口
- 设置与路径
- 状态提示
- 生命周期控制

结论：
- 不应把 Windows 采集/编码细节继续堆入 `MainWindow.Core.cpp`。

### 7. MVP 边界建议

MVP 只做：
- Windows
- 当前主窗口录制
- `MP4(H.264)`
- 仅视频，不录音频
- 保留现有保存路径与自动保存行为

MVP 暂不做：
- 音频
- 暂停/继续
- 任意区域录制
- 高级编码参数面板

结论：
- 先以最小可落地版本替换外部 exe，再考虑增强功能。

## 外部参考方向

可作为实现依据的资料方向：
- `IGraphicsCaptureItemInterop::CreateForWindow`
- `Windows.Graphics.Capture`
- `Direct3D11CaptureFramePool`
- `Media Foundation Sink Writer`
- `Media Foundation H.264 encoder`
- `robmikh/Win32CaptureSample`

使用原则：
- 只用于确认 API 能力边界和接入方式。
- 实现必须贴合本项目现有 Qt / CMake / MainWindow / smoke 结构。

## 2026-04-18 R3 执行补充发现

### 8. 共享源码注入原先依赖硬编码目标名

现状：
- `las_viewer_add_app_sources()` 直接写 `${PROJECT_NAME}`。
- `las_viewer_add_shared_sources()` 直接写 `LASViewerCoreObj`。
- `las_viewer_add_smoke_sources()` 直接写 `LASViewerSmokeTest`。

问题：
- 目标分层调整时，源码注入入口会被目标名耦合住。
- 子目录改造或目标替换时，路由稳定性差。

结论：
- 需要把“源码注入到哪个目标”从函数实现里解耦，改为可配置路由。

### 9. `app_icon.rc` 属于应用专属资源，放在 shared 清单会产生漂移

现状：
- `src/CMakeLists.txt` 之前通过 `las_viewer_add_shared_sources(app_icon.rc)` 注入。

影响：
- smoke 目标会被动携带应用图标资源，清单边界被污染。

修复：
- 改为 `las_viewer_add_app_sources(app_icon.rc)`，仅注入主程序目标。

### 10. 路由化改造已验证可回归通过

已完成改造：
- 在 `cmake/LASViewerTargetConfig.cmake` 增加：
  - `las_viewer_set_source_routes()`
  - `las_viewer_resolve_source_route()`
- 三个 add-sources 入口统一走路由解析。
- 在顶层 `CMakeLists.txt` 显式注册：
  - `APP -> LASPointCloudViewer`
  - `SHARED -> LASViewerCoreObj`
  - `SMOKE -> LASViewerSmokeTest`

验证结果：
- 构建：`LASPointCloudViewer`、`LASViewerSmokeTest` 通过。
- smoke：`main-backstage`、`main-settings-restore` 通过。

结论：
- 当前“顶层目标分层 + 共享源码注入路由迁移 + 清单漂移修复”已经形成闭环。

### 11. R3-2 已补齐最小依赖接入清单与开关策略

已落地：
- 顶层新增 `LAS_VIEWER_ENABLE_WINDOWS_CAPTURE`（默认 `OFF`），保证默认构建路径零侵入。
- 开关启用时进行 Windows SDK 头文件门槛检查；缺失时在配置阶段直接失败并给出修复提示。
- 开关启用时注入系统库：
  - `d3d11`
  - `dxgi`
  - `windowsapp`
  - `mfplat`
  - `mfreadwrite`
  - `mfuuid`
- 开关启用时统一下发编译宏：`LAS_VIEWER_ENABLE_WINDOWS_CAPTURE=1`。

结论：
- 现在已经具备“默认不改变现状、启用时有明确门槛与依赖”的最小构建接入形态。
- 该形态可以直接承接 `Phase R4` 的 `ScreenRecorder` 抽象与实现骨架，不需要再改顶层构建框架。

### 12. R4 首轮落地采用“骨架先行、行为不变”策略

已新增：
- `src/capture/ScreenRecordingTypes.h`
- `src/capture/ScreenRecorder.h`
- `src/capture/ScreenRecorderFactory.h/.cpp`
- `src/capture/WindowsGraphicsCaptureRecorder.h/.cpp`

关键设计点：
- 先建立稳定接口，不急于在同一轮替换 `MainWindow` 现有 ffmpeg 流程。
- `ScreenRecorderFactory` 已具备开关分流能力：
  - 开启 `LAS_VIEWER_ENABLE_WINDOWS_CAPTURE` 且在 Windows 下返回 `WindowsGraphicsCaptureRecorder`。
  - 其他情况返回 `UnsupportedScreenRecorder`，并提供可读原因。
- 该做法保证“增量接入 + 无行为回归”，便于后续分步迁移 start/stop 逻辑。

验证结果：
- 构建通过：`LASPointCloudViewer`、`LASViewerSmokeTest`。
- smoke 通过：`main-backstage`、`main-settings-restore`。

### 13. R4-2 已完成 MainWindow 接口接入，默认行为保持不变

改动点：
- `MainWindow` 新增 `screenRecorder_` 成员并在构造时通过工厂初始化。
- 录屏启动逻辑改为：
  - 若 embedded backend 可用则走 `ScreenRecorder::startRecording()`。
  - 否则走既有 ffmpeg `QProcess` 路径。
- 录屏停止逻辑统一入口，分别处理 embedded 与 ffmpeg 两个后端。
- 动作状态文本（Start/Stop Recording）按“任一后端是否处于录制态”计算。

结论：
- 抽象层已真正进入调用链，而不是孤立骨架。
- 默认构建下用户行为无变化，风险可控。
- 后续可在不改 UI 交互层的前提下，直接替换 `WindowsGraphicsCaptureRecorder` 内部实现。

### 14. R4-3 已完成最小可运行后端实现，但 capture-on 构建受环境依赖阻塞

本轮实现：
- `WindowsGraphicsCaptureRecorder` 已从占位实现升级为可运行实现：
  - COM / Media Foundation 生命周期管理
  - `IMFSinkWriter` H.264 输出
  - GDI 窗口帧采集（32-bit DIB + `BitBlt`）
  - 采集线程与 stop 同步
  - 录制错误消息回传
- `ScreenRecordingStartOptions` 新增 `nativeWindowHandle`，并由 `MainWindow` 在启动录制时传入。

验证结果：
- 默认构建 + smoke：通过。
- capture-on 配置：失败，缺失 `mfreadwrite.h`。

结论：
- 代码链路已进入“可运行后端”阶段。
- 当前无法在本机完成 capture-on 编译验证的根因是 SDK 环境缺失，不是编译逻辑或接口设计缺陷。

### 15. 未完成任务的推进策略已切换为“环境阻塞外先完成可执行项”

策略：
- 对受环境阻塞的项（capture-on 编译）保留明确阻塞记录与解除条件。
- 对不受阻塞的项（验证矩阵、回归风险、二期接口预留）先行完成，降低后续切换成本。

收益：
- 后续仅需补齐 SDK 即可进入“编译 -> 录制实测 -> 回归”的直线流程。
- 计划文档已经具备可直接执行的验证 checklist，避免重复讨论。

### 16. capture-on 阻塞根因是 CMake 探测逻辑，不是 SDK 缺失

复核结果：
- 本机 Windows SDK 已存在 `mfreadwrite.h`、`windows.graphics.capture.interop.h`、`Windows.Graphics.Capture.h`。
- 失败点来自 `check_include_file_cxx(mfreadwrite.h ...)`：该头依赖 `mfapi.h` / `mfidl.h` 声明，单头探测会误报失败。

修复方案：
- 在 `cmake/LASViewerDependencies.cmake` 中保留常规头文件探测。
- 对 `mfreadwrite.h` 改为 `check_cxx_source_compiles`，显式先包含 `mfapi.h` + `mfidl.h` 再包含 `mfreadwrite.h`。

验证结果：
- capture-on 配置通过。
- capture-on 构建通过（`LASPointCloudViewer`、`LASViewerSmokeTest`）。
- capture-on 关键 smoke 通过（`main-backstage`、`main-settings-restore`）。

### 17. 录屏专项自动化已落地到统一 smoke 可执行

实现：
- 在 `examples/viewer_smoke_test.cpp` 新增 `screen-recording` 模式。
- 覆盖链路：启动录制、停止录制、文件落盘校验、录制中关窗收尾。

验证：
- 默认构建（capture-off）按设计 skip。
- capture-on 构建通过并执行通过。

结论：
- 录屏主链路已纳入统一 smoke，不再仅依赖人工点击回归。

### 18. `#define private public` 会改变成员函数链接符号访问级别

现象：
- 在 smoke 中直接调用 `MainWindow::toggleScreenRecording()` / `stopScreenRecording()` 时，capture-on 构建出现 `LNK2001`。

根因：
- `viewer_smoke_test.cpp` 通过 `#define private public` 包含 `MainWindow.h`，导致调用端期望 `public` 符号（`QEAA`）。
- 实际定义仍为 `private` 符号（`AEAA`），产生符号不匹配。

修复：
- 不直接调用私有成员函数，改为触发 `toggleScreenRecordingAction_` 完成录屏起停。

结论：
- 在采用 `private/public` hack 的 smoke 中，优先走可公开触发路径（action/signal），避免直接调用私有方法。

### 19. 用户侧仍提示 ffmpeg 不存在的直接原因是构建目录仍处于 capture-off

根因：
- 录屏入口逻辑是“embedded backend 可用则走内嵌；不可用时回落 ffmpeg”。
- 若当前构建目录是 `LAS_VIEWER_ENABLE_WINDOWS_CAPTURE=OFF`，就会进入 ffmpeg 分支。
- 本机未安装 ffmpeg 时会提示“Recording requires ffmpeg...”。

已做修复与防复发：
- 顶层 `CMakeLists.txt` 改为 Windows 下默认启用 `LAS_VIEWER_ENABLE_WINDOWS_CAPTURE`。
- `MainWindow.Core.cpp` 改进提示文案：当 embedded 不可用且 ffmpeg 缺失时，明确显示不可用原因并提示启用构建开关。

验证：
- 已对 `out/build` 明确执行 `-DLAS_VIEWER_ENABLE_WINDOWS_CAPTURE=ON` 重新配置并构建。
- `screen-recording` / `main-backstage` / `main-settings-restore` smoke 均通过。

### 20. 保存路径弹窗改为停止录制时触发的实现方式

实现：
- 启动录制时先写入临时目录 `LASViewerRecordingTemp` 下的临时 MP4。
- 停止录制后再走最终落盘流程：
  - 若启用“自动保存”，直接保存到配置目录。
  - 若未启用“自动保存”，在停止时弹 `Save Recording` 对话框。
  - 关闭窗口触发的停止（非交互）采用静默落盘，不弹框。

结论：
- 满足“结束录屏时才弹保存路径”的交互要求，并兼容自动保存与关窗收尾。

### 21. MP4 上下颠倒问题来源于帧行序与编码器期望不一致

现象：
- 录制文件播放时画面上下颠倒。

修复：
- 在 `writeCapturedFrame` 中按行倒序拷贝（垂直翻转）后再送入 `IMFSinkWriter`。

结论：
- 编码输入与播放方向对齐，输出方向恢复正常。

### 22. 录屏状态可见性增强

实现：
- 状态栏新增红色 `● REC` 徽标。
- 录制中显示，停止后隐藏。
- 与 `updateActionState()` 的录制状态统一。

结论：
- 用户可一眼识别当前是否处于录屏状态，降低误操作概率。
