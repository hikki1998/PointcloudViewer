# Repository Guidelines

## Communication
- 默认使用中文，除非用户明确要求英文。
- 回答尽量简洁，优先说明结果、验证状态、以及下一步最有价值的动作。
- 在较大改动前先发一句短进度说明，说明准备查看的文件和改动方向。

## First Read
- 新对话进入仓库后，先读 [PROJECT_CONTEXT.md](/E:/code/VibeCodingProject/las_pointcloud_viewer/PROJECT_CONTEXT.md)。
- 如果任务涉及构建、依赖或运行问题，再读 [README.md](/E:/code/VibeCodingProject/las_pointcloud_viewer/README.md) 和 [CMakeLists.txt](/E:/code/VibeCodingProject/las_pointcloud_viewer/CMakeLists.txt)。
- 如果任务涉及 UI/交互，优先看 [src/gui/MainWindow.cpp](/E:/code/VibeCodingProject/las_pointcloud_viewer/src/gui/MainWindow.cpp)、[src/gui/PointCloudViewer.cpp](/E:/code/VibeCodingProject/las_pointcloud_viewer/src/gui/PointCloudViewer.cpp)。
- 如果任务涉及点云着色/显示，优先看 [src/osg/OsgPointCloudNode.cpp](/E:/code/VibeCodingProject/las_pointcloud_viewer/src/osg/OsgPointCloudNode.cpp) 和 [src/osg/PointCloudVisualization.h](/E:/code/VibeCodingProject/las_pointcloud_viewer/src/osg/PointCloudVisualization.h)。

## Project Structure
- `src/gui/`：主窗口、Ribbon、检查器面板、OSG 嵌入视图、量测和状态栏。
- `src/domain/`：杆塔/隐患业务模型、净空分析、导出和剖面投影。
- `src/pointcloud/`：LAS/LAZ 读取、点云数据结构、包围盒等基础模型。
- `src/osg/`：OSG 场景节点、点云渲染、着色参数接入。
- `examples/`：开发者用示例和 smoke test，可执行验证逻辑集中在这里。
- `test_data/`：提交到仓库的小型测试数据和生成脚本。
- `translations/`：Qt `.ts` 翻译源文件，构建时生成 `.qm`。
- `3rd/`：仓库内分发的 release 三方依赖。
- `out/build/`：推荐本地构建目录。

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
- 主程序：
```powershell
.\out\build\bin\Release\LASPointCloudViewer.exe
```
- smoke test：
```powershell
.\out\build\bin\Release\LASViewerSmokeTest.exe .\test_data\ezhou_powerline_sample.las
```
- 涉及 UI、翻译、渲染、点选、量测、构建脚本时，默认至少做一次构建验证；如果改动影响显示结果，优先再跑 smoke test。
- 涉及 UI 样式或主题改动时，额外检查 dock、Ribbon、Message Box、表格和覆盖层，确认没有“黑色背景覆盖深色文字/文字不可读”的回归。

## Current Product State
- 当前应用支持加载多个 `.las/.laz`、基础浏览、视角预设、RGB/高程/单色配色。
- 渲染控制已包含 `Point Size`、`Point Opacity`、`Depth Cue`、`EDL-style Shading`、`Round splats`。
- 左侧有独立 `Project Explorer`，支持多数据集项目树管理。
- 视图右上角有 `X+ / Y+ / Z+` 坐标轴指示器。
- 底部信息栏会显示鼠标当前指向点的坐标。
- 支持多点连续量测、右键回退，并在视图中显示量测覆盖层。
- 支持净空分析阈值、分段明细表、净空 CSV 导出和底部档距剖面视图。
- 支持杆塔编辑、杆塔业务属性维护、隐患台账和巡检报告导出。
- 工程文件会保存多数据集、显示参数、杆塔和隐患业务数据。
- 中文翻译需要保持完整，新增 UI 文本后要同步更新 [translations/lasviewer_zh_CN.ts](/E:/code/VibeCodingProject/las_pointcloud_viewer/translations/lasviewer_zh_CN.ts)。

## Coding Conventions
- 使用 C++17。
- 延续现有 Qt/OSG 代码风格：4 空格缩进，函数大括号换行，控制语句大括号同行。
- 类名用 `PascalCase`，函数和局部变量用 `lowerCamelCase`，文件内常量用 `kPrefix`，成员变量用尾随下划线。
- 头文件引用分组：Qt、第三方、项目头文件，组间空一行。
- 渲染参数统一收口在 [src/osg/PointCloudVisualization.h](/E:/code/VibeCodingProject/las_pointcloud_viewer/src/osg/PointCloudVisualization.h)，不要把同一参数散落在多个地方各自定义。
- UI 可读性为强约束：除非用户明确要求深色风格，所有新增或改造的 UI 默认使用浅色背景 + 深色高对比文字。
- 该约束必须覆盖 dock、Ribbon、Message Box、面板、表格、工具栏、菜单、提示气泡和覆盖层，避免出现黑色或深色遮罩压住文字导致内容看不清。
- 对 Message Box、自定义弹窗和临时浮层，禁止使用深色底叠加深色文字；如使用样式表或调色板，需显式指定浅色背景与可读文本颜色。

## Common Change Paths
- 加 UI 控件或交互：
  - [src/gui/MainWindow.h](/E:/code/VibeCodingProject/las_pointcloud_viewer/src/gui/MainWindow.h)
  - [src/gui/MainWindow.cpp](/E:/code/VibeCodingProject/las_pointcloud_viewer/src/gui/MainWindow.cpp)
- 加 viewer 状态、拾取、悬停、量测、状态栏显示：
  - [src/gui/PointCloudViewer.h](/E:/code/VibeCodingProject/las_pointcloud_viewer/src/gui/PointCloudViewer.h)
  - [src/gui/PointCloudViewer.cpp](/E:/code/VibeCodingProject/las_pointcloud_viewer/src/gui/PointCloudViewer.cpp)
- 加巡检业务模型、净空分析、导出：
  - `src/domain/InspectionData.*`
  - `src/domain/ClearanceAnalysis.*`
  - `src/domain/ClearanceReportExporter.*`
  - `src/domain/ProfileMarkerProjection.*`
- 改剖面图和量测明细：
  - `src/gui/ProfilePlotWidget.*`
  - [src/gui/MainWindow.cpp](/E:/code/VibeCodingProject/las_pointcloud_viewer/src/gui/MainWindow.cpp)
- 改点云显示、shader uniform、渲染表现：
  - [src/osg/OsgPointCloudNode.cpp](/E:/code/VibeCodingProject/las_pointcloud_viewer/src/osg/OsgPointCloudNode.cpp)
  - [src/osg/PointCloudVisualization.h](/E:/code/VibeCodingProject/las_pointcloud_viewer/src/osg/PointCloudVisualization.h)
- 改构建、依赖、翻译生成或运行时部署：
  - [CMakeLists.txt](/E:/code/VibeCodingProject/las_pointcloud_viewer/CMakeLists.txt)

## Translation Workflow
- 新增或修改 UI 文本后，更新 `.ts`：
```powershell
E:\code\Qt5.15.2\5.15.2\msvc2019_64\bin\lupdate.exe src -ts translations\lasviewer_zh_CN.ts
```
- 然后补全中文翻译，再重新构建以生成 `.qm`。

## Test Data Rules
- 优先使用仓库内小型测试数据，例如 [test_data/ezhou_powerline_sample.las](/E:/code/VibeCodingProject/las_pointcloud_viewer/test_data/ezhou_powerline_sample.las)。
- 不要把大型原始 LAS 直接提交到仓库；如需加入测试数据，先裁剪成小样本并附生成脚本。
- 工作区可能存在用户自己的未跟踪大文件，不要擅自删除、提交或改名。

## Git Workflow
- 用户说“提交”默认表示 `commit + push`。
- 提交信息必须使用中文，除非用户明确要求英文。
- 推送失败时，说明失败原因和最新本地提交号。
- 在 `main` 上做强推、重写历史、reset 等不可逆操作前，先说明影响再执行。
- 提交时只加入本次相关文件，尤其不要误带 `out/` 或本地大 LAS。

## Practical Goal
- 目标不是写一份泛化说明，而是让新对话能在几分钟内定位到：入口文件、常改模块、标准验证命令、当前已具备的点云显示功能、以及常见改动该落在哪些文件。

