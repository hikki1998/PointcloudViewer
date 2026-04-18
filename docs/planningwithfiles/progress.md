# 进度记录

## 2026-04-18

### 会话目标
- 为“去掉外部 `ffmpeg.exe`，改为软件内嵌录屏实现”制定并清理出一份单独规划。

### 已完成
- 恢复并读取现有规划文件。
- 核对当前录屏相关代码：
  - `src/gui/MainWindow.Actions.cpp`
  - `src/gui/MainWindow.Core.cpp`
  - `src/gui/MainWindow.Backstage.cpp`
  - `src/gui/MainWindow.SettingsStore.cpp`
  - `CMakeLists.txt`
- 确认当前实现本质是：
  - UI/设置已接好
  - 录屏后端是 `QProcess + ffmpeg.exe`
- 确认当前仓库暂无：
  - `Windows.Graphics.Capture`
  - `D3D11`
  - `DXGI`
  - `Media Foundation`
  的现成接入基础
- 形成规划结论：
  - 推荐路线：`Windows.Graphics.Capture + D3D11 + Media Foundation`
  - 先做 Windows-only MVP
  - 保留现有 Backstage 保存路径与自动保存逻辑

### 本轮整理
- 已按要求清理三个规划文件，只保留当前“内嵌录屏替代 `ffmpeg.exe`”计划。
- 之前的重构排查和竞品规划内容已从这三个文件中移除，避免上下文污染。

### 当前状态
- `Phase R1-R2` 已完成。
- 下一步建议直接进入 `Phase R3`，开始整理构建与依赖接入清单。

### 验证状态
- 本轮仅整理规划文件，未运行构建或测试。

### 本轮继续新增进展
- 已执行“按计划落地”的构建层改造，完成以下任务：
  - 重构顶层目标分层（源码注入路由显式化）
  - 迁移共享源码注入路由（app/shared/smoke 三路）
  - 修复清单漂移文件（`app_icon.rc` 迁移到 app 专属）
- 代码改动：
  - `cmake/LASViewerTargetConfig.cmake`
  - `CMakeLists.txt`
  - `src/CMakeLists.txt`
- 新增能力：
  - `las_viewer_set_source_routes()` 可配置源码注入目标
  - `las_viewer_add_*_sources()` 通过路由解析目标，避免硬编码

### 本轮验证
- 已完成构建：
  - `cmake --build out/build --config Release --target LASPointCloudViewer LASViewerSmokeTest -- /p:PostBuildEventUseInBuild=false`
- 已完成关键 smoke：
  - `LASViewerSmokeTest --mode main-backstage --las .\\test_data\\ezhou_powerline_sample.las`
  - `LASViewerSmokeTest --mode main-settings-restore`
- 结果：全部通过。

### 当前状态
- 待办已完成到“构建并运行关键 smoke”。
- 下一步进入“同步文档说明”收尾与 `Phase R3-2`（系统库接入清单）准备。

### 本轮继续新增进展 2
- 已完成 `Phase R3-2` 的构建接入落地：
  - `CMakeLists.txt` 新增 `LAS_VIEWER_ENABLE_WINDOWS_CAPTURE`（默认 `OFF`）。
  - `cmake/LASViewerDependencies.cmake` 新增 Windows Capture 最小头文件门槛检查与系统库清单。
  - `cmake/LASViewerTargetConfig.cmake` 新增开关编译宏与可执行目标链接注入。
- 设计结果：
  - 默认构建路径不受影响。
  - 仅在显式开启开关时才要求 WinRT/Media Foundation 头文件与系统库。

### 本轮验证 2
- 构建通过：
  - `cmake --build out/build --config Release --target LASPointCloudViewer LASViewerSmokeTest -- /p:PostBuildEventUseInBuild=false`
- smoke 通过：
  - `LASViewerSmokeTest --mode main-backstage --las .\\test_data\\ezhou_powerline_sample.las`
  - `LASViewerSmokeTest --mode main-settings-restore`
  - `main-backstage` 退出码：`0`

### 当前状态 2
- `Phase R3` 已可收口。
- 下一步可进入 `Phase R4`，实现 `ScreenRecorder` 抽象接口与 Windows 实现骨架。

### 本轮继续新增进展 3
- 已进入 `Phase R4` 并完成首轮代码骨架接入：
  - 新增 `src/capture` 子模块与 CMake 接入。
  - 新增录屏抽象接口 `ScreenRecorder` 和结果/参数类型 `ScreenRecordingTypes`。
  - 新增工厂 `ScreenRecorderFactory`，支持按构建开关选择后端。
  - 新增 `WindowsGraphicsCaptureRecorder` 占位实现，当前仅承载接口语义，不改变现有 ffmpeg 行为。

### 本轮验证 3
- 构建通过：
  - `cmake --build out/build --config Release --target LASPointCloudViewer LASViewerSmokeTest -- /p:PostBuildEventUseInBuild=false`
- smoke 通过：
  - `LASViewerSmokeTest --mode main-backstage --las .\\test_data\\ezhou_powerline_sample.las`
  - `LASViewerSmokeTest --mode main-settings-restore`

### 当前状态 3
- `R4-1`（抽象层骨架）已完成。
- 下一步执行 `R4-2`：把 `MainWindow` 的录屏启动/停止链路切到 `ScreenRecorder` 接口并保留 ffmpeg fallback。

### 本轮继续新增进展 4
- 已完成 `R4-2`：`MainWindow` 接入 `ScreenRecorder` 抽象链路。
  - 构造阶段初始化 recorder 工厂实例。
  - `toggleScreenRecording()` 支持 embedded 优先、ffmpeg fallback。
  - `stopScreenRecording()` 支持双后端统一收口。
  - `updateActionState()` 和动作文案统一按双后端录制状态更新。

### 本轮验证 4
- 构建通过：
  - `cmake --build out/build --config Release --target LASPointCloudViewer LASViewerSmokeTest -- /p:PostBuildEventUseInBuild=false`
- smoke 通过：
  - `LASViewerSmokeTest --mode main-backstage --las .\\test_data\\ezhou_powerline_sample.las`
  - `LASViewerSmokeTest --mode main-settings-restore`

### 当前状态 4
- `R4-1`、`R4-2` 已完成且回归通过。
- 下一步进入 `R4-3`：实现 `WindowsGraphicsCaptureRecorder` 的真实采集编码链路。

### 本轮继续新增进展 5
- 已完成 `R4-3` 的核心实现落地（代码层）：
  - `WindowsGraphicsCaptureRecorder` 从占位实现升级为 Media Foundation 编码 + 窗口帧采集实现。
  - `ScreenRecordingStartOptions` 新增原生窗口句柄字段。
  - `MainWindow` 录制启动时向 recorder 传入 `winId()`。

### 本轮验证 5
- 默认构建通过：
  - `cmake --build out/build --config Release --target LASPointCloudViewer LASViewerSmokeTest -- /p:PostBuildEventUseInBuild=false`
- 默认 smoke 通过：
  - `LASViewerSmokeTest --mode main-backstage --las .\\test_data\\ezhou_powerline_sample.las`（退出码 0，`selected=1 passed=1 failed=0`）
  - `LASViewerSmokeTest --mode main-settings-restore`（`selected=1 passed=1 failed=0`）
- capture-on 环境验证：
  - `cmake -S . -B out/build_capture_on ... -DLAS_VIEWER_ENABLE_WINDOWS_CAPTURE=ON` 失败
  - 错误：缺少 `mfreadwrite.h`

### 当前状态 5
- 代码实现已推进到可运行后端阶段。
- 当前剩余阻塞是本机 Windows SDK / Media Foundation 开发头缺失，导致 capture-on 无法完成编译验证。

### 本轮继续新增进展 6
- 在环境阻塞情况下继续推进未完成任务：
  - `R5` 已补齐验证矩阵与回归风险条目（状态改为 `in_progress`）。
  - `R6` 已补齐二期 backlog 与接口预留建议（状态改为 `complete`）。
- 这样后续只需补 SDK，即可直接进入 capture-on 编译和录制实测，不再额外补计划文档。

### 本轮继续新增进展 7
- 已完成 capture-on 阻塞复核与修复：
  - 先确认本机 SDK 头文件实际存在。
  - 修复 `mfreadwrite.h` 的 CMake 误判探测逻辑（改为带前置头的编译探测）。
- capture-on 验证结果：
  - 配置通过：`cmake -S . -B out/build_capture_on ... -DLAS_VIEWER_ENABLE_WINDOWS_CAPTURE=ON`
  - 构建通过：`LASPointCloudViewer`、`LASViewerSmokeTest`
  - smoke 通过：`main-backstage`、`main-settings-restore`
- 现阶段剩余工作：
  - 执行手工录屏端到端验证（10 秒录制、可播放性、重复 start/stop、关窗收尾）。

### 本轮继续新增进展 8
- 已新增录屏专项自动化：
  - `LASViewerSmokeTest` 新增 `screen-recording` mode。
  - 覆盖 embedded 录屏起停、文件落盘、录制中关窗收尾。
- 过程修复：
  - 初版直接调用 `MainWindow` 私有录屏方法引发 `LNK2001`（`private/public` 符号访问级别不匹配）。
  - 已改为触发 `toggleScreenRecordingAction_`，链接问题消除。
- 验证结果：
  - 默认构建：`screen-recording` 按设计 skip（capture-off）。
  - capture-on 构建：`screen-recording` 通过。
  - capture-on 关键回归：`main-backstage`、`main-settings-restore` 均通过。

### 本轮继续新增进展 9
- 针对“用户运行时仍提示 ffmpeg 不存在”已完成定位与修复：
  - 根因是当前运行目录此前为 capture-off 构建，录屏逻辑回落到 ffmpeg 分支。
  - 已将 Windows 默认构建改为 `LAS_VIEWER_ENABLE_WINDOWS_CAPTURE=ON`。
  - 已增强 fallback 提示文案，显示 embedded 不可用原因与构建开关提示。
- 已对 `out/build` 重新执行 capture-on 配置并完成验证：
  - 构建通过：`LASPointCloudViewer`、`LASViewerSmokeTest`
  - smoke 通过：`screen-recording`、`main-backstage`、`main-settings-restore`

### 本轮继续新增进展 10
- 按用户追加需求完成录屏交互与体验修复：
  - 保存路径改为停止录制时处理（启动仅写临时文件）。
  - 修复 MP4 上下颠倒（写样本前垂直翻转）。
  - 状态栏新增醒目红色 `● REC` 录屏徽标。
- 测试结果：
  - capture-on 重新配置与构建通过。
  - `screen-recording` smoke 通过。
  - `main-backstage` 与 `main-settings-restore` 回归通过。
