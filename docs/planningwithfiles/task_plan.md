# 内嵌录屏替代 `ffmpeg.exe` 计划

## 目标

- 移除当前对外部 `ffmpeg.exe` 的依赖。
- 用软件内嵌实现替换现有录屏后端。
- 保留当前 Ribbon、Backstage、保存路径和自动保存设置体验。

## 当前背景

- 当前录屏入口已经接入：
  - `src/gui/MainWindow.Actions.cpp`
  - `src/gui/MainWindow.Core.cpp`
  - `src/gui/MainWindow.Backstage.cpp`
  - `src/gui/MainWindow.SettingsStore.cpp`
- 当前实现本质是：
  - Qt 负责 UI、设置、状态提示
  - `QProcess` 启动外部 `ffmpeg.exe` 执行录屏
- 当前仓库没有现成的：
  - `Windows.Graphics.Capture`
  - `D3D11`
  - `DXGI`
  - `Media Foundation`
  接入基础

## 当前推荐方案

- 平台范围：仅 Windows
- 采集：`Windows.Graphics.Capture`
- 帧承载：`D3D11`
- 编码与封装：`Media Foundation Sink Writer`
- MVP 输出：`MP4(H.264)`，仅视频，不录音频

## 非目标

- 跨平台录屏
- 音频采集
- 暂停/继续
- 任意区域裁剪
- 多编码器切换
- 一次性补齐所有高级录屏能力

## 阶段计划

### Phase R1 - 现状梳理与目标收口
状态：`complete`

动作：
- 确认当前录屏链路、设置入口、保存路径、状态提示和生命周期管理。
- 确认哪些代码可复用，哪些只是 `ffmpeg` 适配层。
- 明确 MVP 范围与非目标。

产出：
- 现状结构说明
- MVP 功能边界

### Phase R2 - 技术选型与模块边界设计
状态：`complete`

动作：
- 确定采集、编码、封装的推荐技术路线。
- 设计新的录屏模块边界，避免平台代码继续堆入 `MainWindow`。
- 明确 `MainWindow` 保留的职责与新模块承担的职责。

产出：
- 推荐方案：`Windows.Graphics.Capture + D3D11 + Media Foundation`
- 目标模块拆分草案

### Phase R3 - 构建与依赖接入计划
状态：`complete`

动作：
- 确认需要增加的系统头文件、WinRT/Win32 互操作头和系统库。
- 规划 `CMakeLists.txt` 与 `cmake/*.cmake` 的最小改动面。
- 定义“最小可编译”目标。

产出：
- 构建接入清单
- 依赖与风险清单

已完成（R3-1）：
- 顶层目标分层梳理并显式化源码注入路由：
  - 在 `cmake/LASViewerTargetConfig.cmake` 新增 `las_viewer_set_source_routes()`。
  - `las_viewer_add_app_sources()` / `las_viewer_add_shared_sources()` / `las_viewer_add_smoke_sources()` 改为走路由解析，不再硬编码目标名。
- 在 `CMakeLists.txt` 注册三类路由映射：
  - `APP -> LASPointCloudViewer`
  - `SHARED -> LASViewerCoreObj`
  - `SMOKE -> LASViewerSmokeTest`
- 修复源码清单漂移：
  - `src/CMakeLists.txt` 中 `app_icon.rc` 从 shared 清单迁移到 app 清单，避免 smoke 目标误注入应用图标资源。
- 完成关键回归验证：
  - `cmake --build out/build --config Release --target LASPointCloudViewer LASViewerSmokeTest -- /p:PostBuildEventUseInBuild=false`
  - `LASViewerSmokeTest --mode main-backstage --las .\\test_data\\ezhou_powerline_sample.las`
  - `LASViewerSmokeTest --mode main-settings-restore`

已完成（R3-2）：
- 在 `CMakeLists.txt` 新增开关：
  - `LAS_VIEWER_ENABLE_WINDOWS_CAPTURE`（默认 `OFF`）。
- 在 `cmake/LASViewerDependencies.cmake` 增加最小接入清单与门槛检查：
  - 头文件检查：`d3d11.h`、`dxgi1_2.h`、`mfapi.h`、`mfidl.h`、`mfreadwrite.h`、`windows.graphics.capture.interop.h`、`winrt/Windows.Graphics.Capture.h`。
  - 平台门槛：非 Windows 下启用该开关会直接报错。
  - 系统库清单：`d3d11`、`dxgi`、`windowsapp`、`mfplat`、`mfreadwrite`、`mfuuid`。
- 在 `cmake/LASViewerTargetConfig.cmake` 打通编译定义与链接注入：
  - 编译定义：`LAS_VIEWER_ENABLE_WINDOWS_CAPTURE=1`（仅开关启用时）。
  - 可执行目标自动链接上述系统库（仅开关启用时）。
- 完成回归验证：
  - `cmake --build out/build --config Release --target LASPointCloudViewer LASViewerSmokeTest -- /p:PostBuildEventUseInBuild=false`
  - `LASViewerSmokeTest --mode main-backstage --las .\\test_data\\ezhou_powerline_sample.las`
  - `LASViewerSmokeTest --mode main-settings-restore`

### Phase R4 - MVP 实现计划
状态：`complete`

动作：
- 规划 `ScreenRecorder` 抽象接口。
- 规划 Windows 实现类、窗口采集、帧接收、编码、文件落盘主链路。
- 规划与 `MainWindow` 的替换点。

产出：
- 代码落点清单
- 核心调用时序

已完成（R4-1）：
- 新增 `src/capture` 模块骨架并接入构建：
  - `src/capture/CMakeLists.txt`
  - `src/capture/ScreenRecordingTypes.h`
  - `src/capture/ScreenRecorder.h`
  - `src/capture/ScreenRecorderFactory.h/.cpp`
  - `src/capture/WindowsGraphicsCaptureRecorder.h/.cpp`
- 在 `src/CMakeLists.txt` 中接入 `add_subdirectory(capture)`。
- 当前实现策略：
  - 提供 `ScreenRecorder` 抽象接口与工厂。
  - Windows 实现先以占位类存在，不改变现有 ffmpeg 录屏主路径。
  - `LAS_VIEWER_ENABLE_WINDOWS_CAPTURE` 开关启用时，工厂返回 Windows 实现；否则返回不支持实现并带明确原因。
- 验证：
  - 构建通过：`LASPointCloudViewer`、`LASViewerSmokeTest`
  - smoke 通过：`main-backstage`、`main-settings-restore`

已完成（R4-2）：
- `MainWindow` 已接入录屏后端选择层：
  - 构造阶段初始化 `screenRecorder_ = capture::createScreenRecorder()`。
  - `toggleScreenRecording()` 先尝试 embedded backend（仅当 `screenRecorder_->isAvailable()`），否则回落到既有 ffmpeg 流程。
  - `stopScreenRecording()` 已统一处理 embedded backend 与 ffmpeg process 两条路径。
- `updateActionState()` 与重翻译动作文本已统一按“任一后端正在录制”判断 Start/Stop 状态。
- 行为保持：默认配置下仍走原 ffmpeg 路径，不影响现有功能与 smoke。

待完成（R4-3）：
- 在 `WindowsGraphicsCaptureRecorder` 中替换占位实现，落地 WGC + D3D11 + Media Foundation 的最小可用录制主链路。

当前进展（R4-3）:
- 已替换占位逻辑为可运行后端实现（当前采用 Media Foundation + GDI 窗口帧采集路径，保留 Windows capture 目标接口）。
- `ScreenRecordingStartOptions` 已补充 `nativeWindowHandle`，`MainWindow` 已向 recorder 传入 `winId()`。
- `WindowsGraphicsCaptureRecorder` 已支持：
  - 后台线程采集与写入
  - start/stop 生命周期
  - Media Foundation H.264 Sink Writer 编码落盘
  - 运行期错误回传

补充验证（已完成）：
- capture-on 配置已通过：`-DLAS_VIEWER_ENABLE_WINDOWS_CAPTURE=ON`。
- capture-on 构建已通过：`LASPointCloudViewer`、`LASViewerSmokeTest`。
- capture-on 关键 smoke 已通过：`main-backstage`、`main-settings-restore`。

后续转入 R5 的手工验证项：
- 手工录制 10 秒并验证 MP4 可播放。
- 重复 start/stop 稳定性与关窗收尾验证。

### Phase R5 - 验证与回归计划
状态：`complete`

动作：
- 定义编译、启动录制、停止录制、文件存在性、重复录制、关闭窗口收尾的验证顺序。
- 评估对 `main-backstage`、`main-settings-restore` 的影响。
- 规划是否新增录屏专项 smoke 或最小自动化断言。

产出：
- 验证矩阵
- 回归风险清单

已完成（R5-1）验证矩阵基线：
- 构建验证：
  - `LASPointCloudViewer`（Release）
  - `LASViewerSmokeTest`（Release）
- 主窗口回归 smoke：
  - `main-backstage`
  - `main-settings-restore`
- capture-on 构建与 smoke：
  - `cmake -S . -B out/build_capture_on ... -DLAS_VIEWER_ENABLE_WINDOWS_CAPTURE=ON`
  - `cmake --build out/build_capture_on --config Release --target LASPointCloudViewer LASViewerSmokeTest`
  - `LASViewerSmokeTest --mode main-backstage`
  - `LASViewerSmokeTest --mode main-settings-restore`
- 录屏链路验证项（待环境解锁后执行）：
  - 启动录制（embedded）
  - 停止录制（embedded）
  - 录制文件存在性与可播放性（MP4/H.264）
  - 重复 start/stop 稳定性
  - 关闭窗口时录制收尾

已完成（R5-2）录屏专项 smoke（capture-on）：
- 新增 `screen-recording` 模式到 `LASViewerSmokeTest`。
- 验证内容：
  - 启动录制 -> 停止录制。
  - 输出文件存在且非空。
  - 第二次启动录制后直接关闭主窗口，验证录制收尾与无崩溃。
- 双构建验证：
  - 默认构建：该模式按设计跳过（`LAS_VIEWER_ENABLE_WINDOWS_CAPTURE=OFF`）。
  - capture-on 构建：该模式通过。

补充说明（非阻塞）：
- 当前自动化已覆盖“可生成且非空”的 MP4 落盘，但“外部播放器可播放性”仍建议保留一次人工抽检。

补充说明（用户侧可用性）：
- 已处理“仍提示 ffmpeg 不存在”问题：
  - Windows 下默认构建改为启用 `LAS_VIEWER_ENABLE_WINDOWS_CAPTURE`。
  - 对 capture-off 或 embedded 不可用场景增加明确原因提示，避免误判为代码未生效。

风险清单（R5）：
- 录制线程退出与 UI 关闭时序冲突。
- Media Foundation Finalize 失败导致空文件或损坏文件。
- 窗口最小化/遮挡时采集帧为空或抖动。
- 缺失 SDK 组件导致 capture-on 构建不可用。

### Phase R6 - 二期扩展预留
状态：`complete`

动作：
- 记录音频、暂停/继续、区域选择、码率/帧率配置、硬件编码偏好等后续能力。
- 明确哪些接口现在就要预留，哪些可以后置。

产出：
- 二期 backlog
- 接口预留建议

已完成（R6）二期 backlog：
- 音频录制（系统音 + 麦克风）与音视频同步。
- 暂停/继续录制。
- 区域录制与多窗口选择。
- 可配置参数：码率、帧率、关键帧间隔、编码质量。
- 编码策略：硬件优先（D3D11 VA / MFT）与软件回退。
- 文件分段与长录制稳定性策略。

已完成（R6）接口预留建议：
- 在 `ScreenRecordingStartOptions` 持续扩展而非破坏签名，确保向后兼容。
- 在 `ScreenRecordingResult` 统一传递后端错误域与可读诊断。
- `ScreenRecorder` 接口未来追加 `pause()` / `resume()` 时保持默认实现可选。

### Phase R7 - 录屏交互与体验修复（用户追加需求）
状态：`complete`

动作：
- 把录屏保存路径选择从“开始录制前”调整为“停止录制后”。
- 修复 MP4 画面上下颠倒。
- 增加醒目的录屏状态提示。
- 完成构建与关键 smoke 验证后交付。

产出：
- 录屏起停流程改造（临时文件录制 + 停止后落盘）。
- 编码写入方向修复。
- 状态栏红色 `● REC` 徽标提示。

已完成（R7）实现与验证：
- `MainWindow` 录屏逻辑改为：
  - 启动时仅创建临时录制文件并开始录制。
  - 停止时弹保存路径（或按“自动保存”配置直接落盘）。
  - 关窗停止时静默落盘，不弹保存框。
- `WindowsGraphicsCaptureRecorder` 写样本时执行逐行垂直翻转，修复输出上下颠倒。
- UI 增加状态栏红色 `● REC` 徽标，录制中常亮。
- 验证通过：
  - `cmake --build out/build --config Release --target LASPointCloudViewer LASViewerSmokeTest`
  - `LASViewerSmokeTest --mode screen-recording`
  - `LASViewerSmokeTest --mode main-backstage`
  - `LASViewerSmokeTest --mode main-settings-restore`

## 模块拆分建议

- `src/capture/ScreenRecordingTypes.*`
- `src/capture/ScreenRecorder.h`
- `src/capture/WindowsGraphicsCaptureRecorder.*`
- `src/capture/MediaFoundationWriter.*`
- `src/capture/D3D11Helpers.*`

`MainWindow` 仅保留：
- 动作入口
- 保存路径 / 自动保存设置
- 状态提示
- start / stop 生命周期控制

## 风险记录

| 风险 | 说明 | 应对 |
|------|------|------|
| 平台接入成本被低估 | 当前仓库没有 D3D11/MF/WGC 基础 | 先做 `R3` 最小接入 |
| 线程与生命周期复杂 | 停止录制、关窗、文件收尾容易踩时序坑 | 保持 `MainWindow` 只持有抽象接口 |
| 验证不足 | 当前自动化未覆盖真实录制链路 | 在 `R5` 中单独规划录屏验证 |

## 下一步

进入收尾阶段：
- 维护 `screen-recording` smoke 作为录屏回归基线。
- 在后续迭代按 `R6` backlog 推进（音频、暂停/继续、参数化编码、硬件优先策略）。
- 执行一次外部播放器可播放性人工抽检，完成录屏 MVP 最后一项体验确认。
