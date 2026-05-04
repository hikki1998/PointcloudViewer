# Repository Guidelines

## Communication
- 默认使用中文，除非用户明确要求英文。
- 回答尽量简洁，优先说明结果、验证状态、以及下一步最有价值的动作。
- 在较大改动前先发一句短进度说明，说明准备查看的文件和改动方向。

## 禁止操作
- 禁止格式化磁盘或清理非当前工程目录以外的任何路径，如有需要，向我确认。

## First Read
- 新对话进入仓库后，先读 `docs/agent/context.md`。
- 读完后再看 `docs/agent/README.md`，按任务继续下钻。
- 如果任务涉及构建、依赖或运行问题，再读 `README.md` 和 `CMakeLists.txt`。
- 如果任务涉及 UI/交互，优先看 `src/gui/MainWindow.cpp`、`src/gui/PointCloudViewer.cpp`。
- 如果任务涉及点云着色/显示，优先看 `src/osg/OsgPointCloudNode.cpp` 和 `src/osg/PointCloudVisualization.h`。

## Project Structure
- `src/gui/`：主窗口、Ribbon、检查器面板、OSG 嵌入视图、量测和状态栏。
- `src/domain/`：杆塔/隐患业务模型、净空分析、导出和剖面投影。
- `src/pointcloud/`：LAS/LAZ 读取、点云数据结构、包围盒等基础模型。
- `src/osg/`：OSG 场景节点、点云渲染、着色参数接入。
- `src/route/`：标准航线模型、JSON IO、桥接、巡检规划与 QA。
- `src/capture/`：Windows 屏幕录制（Windows.Graphics.Capture + D3D11 + Media Foundation）。
- `examples/`：开发者用示例和 smoke test，可执行验证逻辑集中在这里。
- `test_data/`：提交到仓库的小型测试数据和生成脚本。
- `translations/`：Qt `.ts` 翻译源文件，构建时生成 `.qm`。
- `3rd/`：仓库内分发的 release 三方依赖。
- `out/build/`：推荐本地构建目录。

## MainWindow 拆分边界

当前 `src/gui/MainWindow.*` 已按职责拆分为以下编译单元：

| 文件 | 职责 |
|------|------|
| `MainWindow.Core.cpp` | 生命周期、拖放、窗口事件、无边框窗口行为、录屏生命周期 |
| `MainWindow.Actions.cpp` | QAction 创建与动作分组 |
| `MainWindow.Ribbon.cpp` | Ribbon 页面、组、快速工具栏、窗口控制按钮 |
| `MainWindow.Backstage.cpp` | Backstage 页面、最近工程、应用设置入口 |
| `MainWindow.Docks.cpp` | 左右/底部 dock、检查器区、量测区、日志区、状态栏 |
| `MainWindow.Connections.cpp` | viewer、dock、controller、动作之间的信号槽连接 |
| `MainWindow.PointCloud.cpp` | 点云打开、追加、清空、配色和基础显示同步 |
| `MainWindow.Route.cpp` | 航线导入导出、编辑、焦点、表格刷新、漫游状态同步 |
| `MainWindow.TowerIssue.cpp` | 杆塔/隐患面板、详情编辑器、导入导出与聚焦 |
| `MainWindow.ProjectSerializer.cpp` | 工程文件 JSON 读写与工程内嵌状态恢复 |
| `MainWindow.SettingsStore.cpp` | `QSettings` / `UiHistoryStore` 读写与窗口状态恢复 |
| `MainWindow.Helpers.cpp` | 共享 helper、JSON 辅助转换、最近工程记录 |
| `MainWindowInternal.h` | 拆分后共享的最小内部声明与常量 |

## Build And Validation
- 用户已明确授权：常规 `git pull`、`cmake` 配置、`cmake --build`、smoke test 这类开发拉取与构建验证命令可直接执行，无需逐次征询。
- 上述默认直执行授权不覆盖高风险命令；涉及强推、历史重写、删除、reset 或其他不可逆操作时，仍需先说明影响。
- 标准配置命令：
```powershell
cmake -S . -B out/build -G "Visual Studio 17 2022" -A x64 -DQT_ROOT=E:/code/Qt5.15.2/5.15.2/msvc2019_64
```
- 标准构建命令：
```powershell
cmake --build out/build --config Release --target LASPointCloudViewer LASViewerSmokeTest
```
- worktree 下跳过部署的构建（仅验证编译）：
```powershell
cmake --build out/build --config Release --target LASPointCloudViewer LASViewerSmokeTest -- /p:PostBuildEventUseInBuild=false
```
- 主程序：
```powershell
.\out\build\bin\Release\LASPointCloudViewer.exe
```
- smoke test：
```powershell
.\out\build\bin\Release\LASViewerSmokeTest.exe --mode viewer-render --las .\test_data\ezhou_powerline_sample.las
.\out\build\bin\Release\LASViewerSmokeTest.exe --mode route-roam --las .\test_data\ezhou_powerline_sample.las
.\out\build\bin\Release\LASViewerSmokeTest.exe --mode main-backstage
```
- 仓库只保留一个主冒烟可执行文件 `LASViewerSmokeTest.exe`；新增 smoke 场景时，必须并入该 exe 的新 `mode` 或 `category`，不要再新增独立 smoke exe。
- 涉及 UI、翻译、渲染、点选、量测、构建脚本时，默认至少做一次构建验证；如果改动影响显示结果，优先再跑 smoke test。
- 涉及 UI 样式或主题改动时，额外检查 dock、Ribbon、Message Box、表格、ComboBox（含下拉列表）和覆盖层，确认没有"黑色背景覆盖深色文字/文字不可读"的回归。

## Current Product State
- 当前应用支持加载多个 `.las/.laz`、基础浏览、视角预设、RGB/高程/单色/分类配色。
- 渲染控制已包含 `Point Size`、`Point Opacity`、`Depth Cue`、`EDL-style Shading`、`Round splats`。
- 左侧有独立 `Project Explorer`，支持多数据集项目树管理。
- 视图右上角有 `X+ / Y+ / Z+` 坐标轴指示器。
- 底部信息栏会显示鼠标当前指向点的坐标。
- 支持多点连续量测、右键回退，并在视图中显示量测覆盖层。
- 支持净空分析阈值、分段明细表、净空 CSV 导出和底部档距剖面视图。
- 支持杆塔编辑、杆塔业务属性维护、隐患台账和巡检报告导出。
- 支持航线导入/导出、场景显示、编辑、Route QA 和漫游预览。
- 支持内嵌屏幕录制（Windows.Graphics.Capture），保存为 MP4(H.264)，按钮位于 Ribbon 和 Backstage。
- 工程文件会保存多数据集、显示参数、杆塔和隐患业务数据。
- 中文翻译需要保持完整，新增 UI 文本后要同步更新 `translations/lasviewer_zh_CN.ts`。

## Coding Conventions
- 使用 C++17。
- 延续现有 Qt/OSG 代码风格：4 空格缩进，函数大括号换行，控制语句大括号同行。
- 类名用 `PascalCase`，函数和局部变量用 `lowerCamelCase`，文件内常量用 `kPrefix`，成员变量用尾随下划线。
- 头文件引用分组：Qt、第三方、项目头文件，组间空一行。
- 渲染参数统一收口在 `src/osg/PointCloudVisualization.h`，不要把同一参数散落在多个地方各自定义。
- UI 可读性为强约束：除非用户明确要求深色风格，所有新增或改造的 UI 默认使用浅色背景 + 深色高对比文字。
- 该约束必须覆盖 dock、Ribbon、Message Box、面板、表格、工具栏、菜单、提示气泡、覆盖层和输入控件（尤其 ComboBox 本体与下拉列表），避免出现黑色或深色背景压住深色文字，或选中/悬停态文字不可读。
- 对 Message Box、自定义弹窗和临时浮层，禁止使用深色底叠加深色文字；如使用样式表或调色板，需显式指定浅色背景与可读文本颜色。

## Common Change Paths
- 加 UI 控件或交互：
  - `src/gui/MainWindow.h`
  - `src/gui/MainWindow.cpp`（及对应的拆分文件）
- 加 viewer 状态、拾取、悬停、量测、状态栏显示：
  - `src/gui/PointCloudViewer.h`
  - `src/gui/PointCloudViewer.cpp`
- 加巡检业务模型、净空分析、导出：
  - `src/domain/InspectionData.*`
  - `src/domain/ClearanceAnalysis.*`
  - `src/domain/ClearanceReportExporter.*`
  - `src/domain/ProfileMarkerProjection.*`
- 改剖面图和量测明细：
  - `src/gui/ProfilePlotWidget.*`
  - `src/gui/MainWindow.cpp`
- 改点云显示、shader uniform、渲染表现：
  - `src/osg/OsgPointCloudNode.cpp`
  - `src/osg/PointCloudVisualization.h`
- 改航线：
  - `src/route/PowerlineRouteTypes.h`
  - `src/route/PowerlineRouteJson.*`
  - `src/route/PowerlineRouteBridge.*`
  - `src/route/InspectionRoutePlanning.*`
- 改屏幕录制：
  - `src/capture/ScreenRecorder.h`
  - `src/capture/WindowsGraphicsCaptureRecorder.cpp`
- 改构建、依赖、翻译生成或运行时部署：
  - `CMakeLists.txt`
  - `cmake/*.cmake`
  - `src/*/CMakeLists.txt`
  - `examples/CMakeLists.txt`

## Translation Workflow
- 新增或修改 UI 文本后，更新 `.ts`：
```powershell
E:\code\Qt5.15.2\5.15.2\msvc2019_64\bin\lupdate.exe src -ts translations\lasviewer_zh_CN.ts
```
- 然后补全中文翻译，再重新构建以生成 `.qm`。
- 如果使用跳过部署的构建，需手动同步 `.qm` 到运行目录。

## Test Data Rules
- 优先使用仓库内小型测试数据，例如 `test_data/ezhou_powerline_sample.las`。
- 不要把大型原始 LAS 直接提交到仓库；如需加入测试数据，先裁剪成小样本并附生成脚本。
- 工作区可能存在用户自己的未跟踪大文件，不要擅自删除、提交或改名。

## Git Workflow
- 用户说"提交"默认表示 `commit + push`。
- 提交信息必须使用中文，除非用户明确要求英文。
- 推送失败时，说明失败原因和最新本地提交号。
- 在 `main` 上做强推、重写历史、reset 等不可逆操作前，先说明影响再执行。
- 提交时只加入本次相关文件，尤其不要误带 `out/` 或本地大 LAS。

## Practical Goal
- 目标不是写一份泛化说明，而是让新对话能在几分钟内定位到：入口文件、常改模块、标准验证命令、当前已具备的功能、以及常见改动该落在哪些文件。
