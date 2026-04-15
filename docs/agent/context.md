# Agent Context

## What This Repo Is
这是一个基于 Qt 5.15、OpenSceneGraph 和 LASlib 的 Windows 桌面点云查看器。当前重点不是通用 GIS 能力，而是电力巡检/通道检查场景下的点云浏览、业务标注、量测分析和较稳定的本地构建体验。

## Read This Next
- `docs/agent/README.md`
  - 渐进式披露入口，按任务继续下钻
- 根目录 `AGENTS.md`
  - Codex 仓库级强约束
- 根目录 `CLAUDE.md`
  - Claude Code 兼容入口

## Fast Orientation
- 入口：`src/main.cpp`
- 主窗口与 Ribbon：`src/gui/MainWindow.cpp`、`src/gui/MainWindow.Ribbon.cpp`、`src/gui/MainWindow.Docks.cpp`
- 视图交互、量测、悬停拾取、状态栏：`src/gui/PointCloudViewer.cpp`
- 巡检业务模型：`src/domain/InspectionData.*`
- 净空分析与导出：`src/domain/ClearanceAnalysis.*`、`src/domain/ClearanceReportExporter.*`
- 剖面投影与绘制：`src/domain/ProfileMarkerProjection.*`、`src/gui/ProfilePlotWidget.*`
- 点云显示参数模型：`src/osg/PointCloudVisualization.h`
- 点云渲染与 shader：`src/osg/OsgPointCloudNode.cpp`
- LAS/LAZ 读取：`src/pointcloud/LasReader.cpp`
- 中文翻译：`translations/lasviewer_zh_CN.ts`
- 构建与部署逻辑：`CMakeLists.txt`

## Current User-Facing Features
- 加载一个或多个 `.las/.laz`
- 左侧 `Project Explorer` 目录树，支持搜索、展开折叠、定位文件夹、复制路径
- 工程打开/保存/另存为，持久化多数据集、显示参数、语言、杆塔和隐患台账
- RGB、高程渐变、单色显示
- 点大小、透明度、深度雾化、EDL 风格增强、圆形 splat
- 顶视、前视、右视、适配视图
- 右上角坐标轴指示器，显示 `X+ / Y+ / Z+`
- 鼠标悬停点坐标显示
- 多点连续量测，右键回退点，视图覆盖层显示量测路径
- 净空分析阈值、分段明细表、净空 CSV 导出
- 底部档距剖面 dock，支持预警分段高亮，并叠加附近杆塔/隐患点
- 杆塔编辑：连续添加、前插、移动、表格改名、业务属性维护
- 隐患台账：连续点选标注、列表管理、详情编辑、CSV/HTML 导出
- 航线导入/导出、场景显示、编辑、Route QA 和漫游预览
- 中英文界面，其中中文翻译已接入构建和部署

## File Responsibilities
### `src/gui/MainWindow.*`
- Ribbon 动作
- 左右/底部 dock 组织
- 检查器面板和渲染控制
- 项目树、杆塔表、隐患表、量测面板、剖面面板接线
- 设置持久化
- 语言切换
- 与 `PointCloudViewer` 的信号槽连接

当前已按职责拆分为：
- `src/gui/MainWindow.Core.cpp`
  - 生命周期、拖放、窗口事件、无边框窗口行为
- `src/gui/MainWindow.Actions.cpp`
  - QAction 创建与分组
- `src/gui/MainWindow.Ribbon.cpp`
  - Ribbon 页面、组、快速工具栏、窗口控制
- `src/gui/MainWindow.Backstage.cpp`
  - Backstage 页面、最近工程、应用设置入口
- `src/gui/MainWindow.Docks.cpp`
  - dock、检查器、日志、状态栏
- `src/gui/MainWindow.Connections.cpp`
  - viewer、dock、controller、动作之间的信号槽连接
- `src/gui/MainWindow.PointCloud.cpp`
  - 点云打开、追加、清空、配色和基础显示同步
- `src/gui/MainWindow.Route.cpp`
  - 航线导入导出、编辑、焦点、表格刷新、漫游状态同步
- `src/gui/MainWindow.TowerIssue.cpp`
  - 杆塔/隐患面板、详情编辑器、导入导出与聚焦
- `src/gui/MainWindow.ProjectSerializer.cpp`
  - 工程文件 JSON 读写
- `src/gui/MainWindow.SettingsStore.cpp`
  - `QSettings` / `UiHistoryStore` 持久化
- `src/gui/MainWindow.Helpers.cpp`
  - 共享 helper 与内部辅助转换
- `src/gui/MainWindowInternal.h`
  - 拆分后的共享内部声明与常量

### `src/gui/PointCloudViewer.*`
- OSG 嵌入小部件
- 相机操纵器
- 点点击/悬停拾取
- 状态栏信息
- 多点量测逻辑
- 杆塔/隐患拾取与覆盖层
- 右上角坐标轴覆盖层

### `src/domain/InspectionData.*`
- 杆塔业务属性和隐患台账模型
- 工程文件序列化所需的 JSON 转换

### `src/domain/ClearanceAnalysis.*`
- 量测路径转净空分段结果
- 水平距离、三维距离、里程、阈值预警统计

### `src/domain/ProfileMarkerProjection.*`
- 将杆塔/隐患投影到当前量测剖面
- 供剖面图叠加业务标记使用

### `src/gui/ProfilePlotWidget.*`
- 量测剖面绘制
- 预警分段高亮
- 杆塔/隐患投影标记绘制

### `src/osg/OsgPointCloudNode.cpp`
- 点云几何构建
- 渲染状态
- EDL-style / depth cue / opacity / round splat 等 shader uniform

### `src/osg/PointCloudVisualization.h`
- 所有显示参数的单一数据结构
- 如果新增显示选项，通常先从这里加字段，再串到 GUI 和渲染层

### `CMakeLists.txt`
- 依赖探测
- Qt 翻译 `.qm` 生成
- Windows 运行时 DLL 部署
- Visual Studio / MSVC 并行编译配置

## Standard Validation
```powershell
cmake -S . -B out/build -G "Visual Studio 17 2022" -A x64 -DQT_ROOT=E:/code/Qt5.15.2/5.15.2/msvc2019_64
cmake --build out/build --config Release --target LASPointCloudViewer LASViewerSmokeTest
.\out\build\bin\Release\LASViewerSmokeTest.exe --mode viewer-render --las .\test_data\ezhou_powerline_sample.las
.\out\build\bin\Release\LASViewerSmokeTest.exe --mode route-roam --las .\test_data\ezhou_powerline_sample.las
```

如在 `mainwindow-refactor` 这类重构 worktree 中验证，可先使用：
```powershell
cmake --build out/build --config Release --target LASPointCloudViewer LASViewerSmokeTest -- /p:PostBuildEventUseInBuild=false
.\out\build\bin\Release\LASViewerSmokeTest.exe --mode main-backstage
```

如需快速检查 GUI 是否能正常启动：
```powershell
.\out\build\bin\Release\LASPointCloudViewer.exe
```

## Translation Workflow
新增界面文字后：
```powershell
E:\code\Qt5.15.2\5.15.2\msvc2019_64\bin\lupdate.exe src -ts translations\lasviewer_zh_CN.ts
cmake --build out/build --config Release --target LASPointCloudViewer
```

## Data And Smoke Test
- 推荐 smoke test 数据：`test_data/ezhou_powerline_sample.las`
- 如果需要新的测试数据，优先加小样本和生成脚本，不要直接提交大型原始 LAS

## Common Pitfalls
- 新增显示参数时，只改 UI 不改渲染层会导致控件无效。
- 新增 UI 文本但不更新 `.ts/.qm` 会出现漏翻译。
- 改相机或拾取逻辑后，最好同时验证缩放、悬停坐标、量测点选、杆塔/隐患选择。
- 改工程文件结构后，要同时检查旧工程兼容和新字段保存加载。
- 改量测逻辑后，要同时检查量测表格、剖面 dock 和导出结果是否一致。
- CMake 依赖 Windows 和本地 Qt 路径，排查构建问题时优先看 `CMakeLists.txt` 和 `out/build/CMakeCache.txt`。

## What To Read For Typical Tasks
- “加一个显示选项”：
  - `src/osg/PointCloudVisualization.h`
  - `src/gui/MainWindow.cpp`
  - `src/gui/MainWindow.Docks.cpp`
  - `src/gui/PointCloudViewer.cpp`
  - `src/osg/OsgPointCloudNode.cpp`
- “修 UI 或交互”：
  - `src/gui/MainWindow.cpp`
  - `src/gui/MainWindow.Docks.cpp`
  - `src/gui/MainWindow.Ribbon.cpp`
  - `src/gui/MainWindow.Backstage.cpp`
  - `src/gui/PointCloudViewer.cpp`
- “修电力巡检业务功能”：
  - `src/domain/InspectionData.*`
  - `src/gui/MainWindow.cpp`
  - `src/gui/MainWindow.Route.cpp`
  - `src/gui/MainWindow.TowerIssue.cpp`
  - `src/gui/PointCloudViewer.cpp`
- “修净空分析或剖面图”：
  - `src/domain/ClearanceAnalysis.*`
  - `src/domain/ClearanceReportExporter.*`
  - `src/domain/ProfileMarkerProjection.*`
  - `src/gui/ProfilePlotWidget.*`
  - `src/gui/MainWindow.cpp`
  - `src/gui/MainWindow.Docks.cpp`
- “修翻译”：
  - `translations/lasviewer_zh_CN.ts`
  - `src/gui/MainWindow.cpp`
  - `src/gui/MainWindow.Ribbon.cpp`
  - `src/gui/MainWindow.Backstage.cpp`
  - `src/gui/PointCloudViewer.cpp`
- “修构建或部署”：
  - `CMakeLists.txt`

## Goal For New Sessions
新对话的 agent 读完本文件后，应当已经知道：
- 这个项目是什么
- 入口和热区文件在哪里
- 当前能力大致到什么程度
- 改完后该怎么构建和验证
- 接下来应该去读 `docs/agent/README.md`
