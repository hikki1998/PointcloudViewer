# Session Handoff

更新时间：2026-04-16

## 用途

这个文档不是替代 `context.md` 或 `README.md`，而是给新会话里的 agent 一个更短的“直接上手”入口。

建议用法：
- 先读根目录 `AGENTS.md`
- 再读 `docs/agent/context.md`
- 然后读本文件
- 最后按任务继续看 `docs/agent/README.md` 里的专题文档

## 项目一句话

这是一个基于 Qt 5.15、OpenSceneGraph 和 LASlib 的 Windows 桌面点云查看器，主要面向电力巡检 / 通道检查场景，重点能力包括点云浏览、量测、净空分析、杆塔与隐患管理、航线编辑以及工程文件持久化。

## 最常看的入口

- 程序入口：`src/main.cpp`
- 主窗口总入口：`src/gui/MainWindow.cpp`
- Ribbon：`src/gui/MainWindow.Ribbon.cpp`
- Backstage：`src/gui/MainWindow.Backstage.cpp`
- 动作定义：`src/gui/MainWindow.Actions.cpp`
- dock / 状态栏：`src/gui/MainWindow.Docks.cpp`
- viewer 交互：`src/gui/PointCloudViewer.cpp`
- 点云渲染：`src/osg/OsgPointCloudNode.cpp`
- 显示参数模型：`src/osg/PointCloudVisualization.h`
- 中文翻译：`translations/lasviewer_zh_CN.ts`
- 构建入口：`CMakeLists.txt`

## 当前代码组织重点

`MainWindow` 已按职责拆分，新增或修改功能时尽量落到对应文件，而不是继续把逻辑堆回 `MainWindow.cpp`：

- `src/gui/MainWindow.Core.cpp`
- `src/gui/MainWindow.Actions.cpp`
- `src/gui/MainWindow.Ribbon.cpp`
- `src/gui/MainWindow.Backstage.cpp`
- `src/gui/MainWindow.Docks.cpp`
- `src/gui/MainWindow.Connections.cpp`
- `src/gui/MainWindow.PointCloud.cpp`
- `src/gui/MainWindow.Route.cpp`
- `src/gui/MainWindow.TowerIssue.cpp`
- `src/gui/MainWindow.ProjectSerializer.cpp`
- `src/gui/MainWindow.SettingsStore.cpp`
- `src/gui/MainWindow.Helpers.cpp`
- `src/gui/MainWindowInternal.h`

## 近期值得知道的改动方向

最近一轮 UI 调整主要集中在 Backstage 和首页入口：

- 首页 `Dataset` 组新增了“工程管理”入口，直接跳转到 Backstage 的工程管理页。
- `Project Properties` 相关文案已统一收口为 `Project Management / 工程管理`。
- Backstage 左侧导航图标已改为统一的 Ribbon 风格图标。
- Backstage 左侧 `保存工程 / 工程另存为 / 退出` 的按钮样式已做额外归一化，避免 `addAction()` 与 `addPage()` 渲染尺寸不一致。

如果要继续看这块，优先读：

- `src/gui/MainWindow.Actions.cpp`
- `src/gui/MainWindow.Ribbon.cpp`
- `src/gui/MainWindow.Backstage.cpp`
- `src/gui/MainWindow.cpp`
- `src/gui/support/RibbonIconFactory.h`
- `src/gui/support/RibbonIconFactory.cpp`
- `translations/lasviewer_zh_CN.ts`

## 常见任务从哪里入手

- 改 UI / Ribbon / Backstage / dock：
  - `src/gui/MainWindow.Ribbon.cpp`
  - `src/gui/MainWindow.Backstage.cpp`
  - `src/gui/MainWindow.Docks.cpp`
  - `src/gui/MainWindow.cpp`
- 改 viewer 交互、拾取、量测、状态栏：
  - `src/gui/PointCloudViewer.cpp`
- 改点云显示参数或渲染效果：
  - `src/osg/PointCloudVisualization.h`
  - `src/osg/OsgPointCloudNode.cpp`
- 改净空分析、剖面图、导出：
  - `src/domain/ClearanceAnalysis.*`
  - `src/domain/ProfileMarkerProjection.*`
  - `src/gui/ProfilePlotWidget.*`
- 改工程文件保存加载：
  - `src/gui/MainWindow.ProjectSerializer.cpp`
  - `src/domain/InspectionData.*`
- 改构建脚本：
  - `CMakeLists.txt`
  - `cmake/*.cmake`
  - `src/*/CMakeLists.txt`
  - `examples/CMakeLists.txt`

## 约束和坑

- UI 可读性是强约束，默认保持浅色背景 + 深色高对比文字。
- 新增界面文本后必须同步更新 `translations/lasviewer_zh_CN.ts`。
- 新增显示参数时，不要只改 UI；要同时串到 `PointCloudVisualization` 和渲染层。
- 新增源码文件时，优先改所属目录的 `CMakeLists.txt`，不要回到顶层集中登记。
- 不要误提交 `out/`、本地大 LAS、或用户自己的无关工作区改动。

## 标准验证命令

```powershell
cmake -S . -B out/build -G "Visual Studio 17 2022" -A x64 -DQT_ROOT=E:/code/Qt5.15.2/5.15.2/msvc2019_64
cmake --build out/build --config Release --target LASPointCloudViewer LASViewerSmokeTest
```

如果只是验证当前代码逻辑，且希望跳过 post-build runtime deploy，可优先用：

```powershell
cmake --build out/build --config Release --target LASPointCloudViewer LASViewerSmokeTest -- /p:PostBuildEventUseInBuild=false
```

常用 smoke：

```powershell
.\out\build\bin\Release\LASViewerSmokeTest.exe --mode main-backstage
.\out\build\bin\Release\LASViewerSmokeTest.exe --mode viewer-render --las .\test_data\ezhou_powerline_sample.las
.\out\build\bin\Release\LASViewerSmokeTest.exe --mode route-roam --las .\test_data\ezhou_powerline_sample.las
```

## 给下个会话的最短提示

```text
先按顺序读 AGENTS.md、docs/agent/context.md、docs/agent/session-handoff.md、docs/agent/README.md。
这是一个 Qt 5.15 + OSG + LASlib 的电力巡检点云查看器。
如果当前任务和 UI / Backstage / Ribbon 有关，先看：
- src/gui/MainWindow.Actions.cpp
- src/gui/MainWindow.Ribbon.cpp
- src/gui/MainWindow.Backstage.cpp
- src/gui/MainWindow.cpp
- src/gui/support/RibbonIconFactory.*
- translations/lasviewer_zh_CN.ts

改完优先验证：
cmake --build out/build --config Release --target LASPointCloudViewer LASViewerSmokeTest -- /p:PostBuildEventUseInBuild=false
.\out\build\bin\Release\LASViewerSmokeTest.exe --mode main-backstage
```
