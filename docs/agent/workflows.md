# Workflows

## 标准构建与运行

### 配置
```powershell
cmake -S . -B out/build -G "Visual Studio 17 2022" -A x64 -DQT_ROOT=E:/code/Qt5.15.2/5.15.2/msvc2019_64
```

### 构建
```powershell
cmake --build out/build --config Release --target LASPointCloudViewer LASViewerSmokeTest
```

### worktree 下的重构验证
```powershell
cmake --build out/build --config Release --target LASPointCloudViewer LASViewerSmokeTest -- /p:PostBuildEventUseInBuild=false
```

### 运行
```powershell
.\out\build\bin\Release\LASPointCloudViewer.exe
```

### smoke test
```powershell
.\out\build\bin\Release\LASViewerSmokeTest.exe --mode viewer-render --las .\test_data\ezhou_powerline_sample.las
.\out\build\bin\Release\LASViewerSmokeTest.exe --mode main-backstage
.\out\build\bin\Release\LASViewerSmokeTest.exe --mode route-roam --las .\test_data\ezhou_powerline_sample.las
```

## 常见改动路径

### 改构建 / 依赖 / 部署
1. `CMakeLists.txt`
2. `cmake/LASViewerDependencies.cmake`
3. `cmake/LASViewerTranslations.cmake`
4. `cmake/LASViewerTargetConfig.cmake`
5. `cmake/LASViewerRuntimeDeploy.cmake`
6. `src/*/CMakeLists.txt`
7. `examples/CMakeLists.txt`

### 共享源码路由规则（当前）
- `las_viewer_add_shared_sources(...)`
  - 注入 `LASViewerCoreObj`（OBJECT 共享编译层）。
- `las_viewer_add_app_sources(...)`
  - 注入 `LASPointCloudViewer` 专属入口源码（如 `src/main.cpp`）。
- `las_viewer_add_smoke_sources(...)`
  - 注入 `LASViewerSmokeTest` 专属入口源码（如 `examples/viewer_smoke_test.cpp`）。

新增源码时，先判断它是“共享实现”还是“入口专属”。避免把共享业务 `.cpp` 误挂到 app/smoke 专属入口，导致重复编译回归。

### 加一个显示参数
1. `src/osg/PointCloudVisualization.h`
2. `src/gui/MainWindow.*`
3. `src/gui/PointCloudViewer.*`
4. `src/osg/OsgPointCloudNode.cpp`

### 改 UI / Ribbon / dock / 表格 / 弹窗
1. `src/gui/MainWindow.h`
2. `src/gui/MainWindow.cpp`
3. 如果是已拆分区域，优先定位对应实现文件：
   - `MainWindow.Docks.cpp`
   - `MainWindow.Backstage.cpp`
   - `MainWindow.Ribbon.cpp`
   - `MainWindow.Actions.cpp`
   - `MainWindow.SettingsStore.cpp`
   - `MainWindow.ProjectSerializer.cpp`
4. 如果联动场景交互，再看 `src/gui/PointCloudViewer.*`

### 改点拾取 / 相机 / overlay / 漫游 / 状态栏
1. `src/gui/PointCloudViewer.h`
2. `src/gui/PointCloudViewer.cpp`

### 改航线
1. `src/route/PowerlineRouteTypes.h`
2. `src/route/PowerlineRouteJson.*`
3. `src/route/PowerlineRouteBridge.*`
4. `src/route/InspectionRoutePlanning.*`
5. `src/gui/MainWindow.*`
6. `src/gui/PointCloudViewer.*`

### 改净空分析 / 剖面
1. `src/domain/ClearanceAnalysis.*`
2. `src/domain/ProfileMarkerProjection.*`
3. `src/gui/ProfilePlotWidget.*`
4. `src/gui/MainWindow.cpp`
5. 如涉及参数持久化，再看 `src/gui/MainWindow.SettingsStore.cpp`

## 验证基线

### 默认最少验证
- 改 UI、交互、翻译、渲染、点选、构建脚本时：
  - 至少做一次 `Release` 构建
- 在 `mainwindow-refactor` 这类重构 worktree 中：
  - 优先使用 `-- /p:PostBuildEventUseInBuild=false`，先验证编译与 smoke，再单独处理 runtime 收集问题
- 仓库只保留一个主冒烟可执行文件：`LASViewerSmokeTest.exe`
- 新增 smoke 场景时：
  - 必须并入 `LASViewerSmokeTest` 的新 `mode` 或新 `category`
  - 不要新增零散的独立 smoke exe

### 需要补 smoke 的场景
- 改点云渲染
- 改拾取或相机
- 改航线显示、漫游、预览
- 改翻译生成或部署逻辑

## MainWindow 重构后对等性检查

适用场景：
- 拆分 `MainWindow.*`
- 把逻辑从 `MainWindow.cpp` 挪到 `*Controller.cpp`
- 新增 dock、表格、QAction、Backstage 页面、viewer 联动

### 连接类检查

- 每个新增或改动的 `QAction`，都确认 `triggered` 最终能到业务处理逻辑，不要只创建不接线。
- 每个 controller 的 `signals:`，都检查是否在 `src/gui/MainWindow.Connections.cpp` 有接收方。
- 每个关键 widget 信号都确认主窗口集成路径仍在：
  - `currentItemChanged`
  - `itemChanged`
  - `itemDoubleClicked`
  - `customContextMenuRequested`
  - `valueChanged`
  - `currentIndexChanged`
  - `toggled`
- 每个 viewer 发回 MainWindow 的信号都确认还会刷新：
  - 面板
  - 表格
  - 状态栏
  - `updateActionState()`

### 状态类检查

- 每个 `QDockWidget` 都有稳定 `objectName`，否则 `saveState()` / `restoreState()` 不可信。
- `loadWindowSettings()` 后确认窗口状态会重新归一：
  - dock 可见性
  - tabify 顺序
  - 宽度钳制
  - 最大化 / 全屏 / 普通窗口切换
- 关闭窗口时，确认不会被 `visibilityChanged(false)` 之类的瞬时状态污染持久化结果。

### 交互类检查

- 列表 / 表格选中后，确认场景高亮或焦点同步仍在。
- viewer 内选择变化后，确认表格当前行、详情面板和动作可用态一起刷新。
- 右键菜单、双击聚焦、勾选显隐、搜索过滤不能只在 controller 单测里通过，必须走主窗口集成再确认一遍。
- 标题区拖拽、双击最大化、全屏切换、边缘缩放要一起回归，避免只修一个交互破坏另一个。

### smoke 检查

- 不要只看 controller 单体 smoke；至少补一条走 `MainWindow` 集成路径的 smoke。
- 如果改动影响 Backstage / Ribbon / dock / 恢复链，优先回归：
  - `.\out\build\bin\Release\LASViewerSmokeTest.exe --mode main-backstage`
  - `.\out\build\bin\Release\LASViewerSmokeTest.exe --mode main-settings-restore`

### 翻译与部署检查

- 新增 UI 文案后必须更新 `translations/lasviewer_zh_CN.ts`。
- 如果使用 `-- /p:PostBuildEventUseInBuild=false` 构建，只生成 `.qm` 还不够；验证中文运行时效果前，要确认运行目录也拿到了最新翻译：

```powershell
Copy-Item "out/build/translations/lasviewer_zh_CN.qm" "out/build/bin/Release/translations/lasviewer_zh_CN.qm" -Force
```

### 提交前最小自检

- 改了什么接线
- 改了哪些 `QAction`
- 改了哪些 dock / settings / restore 链
- 有哪些 MainWindow 集成 smoke 覆盖到了
- 有没有翻译和 runtime 资源漏部署

### 额外人工检查
- 涉及 UI 样式时，检查：
  - dock
  - Ribbon
  - Message Box
  - 表格
  - ComboBox 本体和下拉列表
  - 叠加层
- 目标是避免深色背景压深色文字、选中态文字不可读。

## 翻译流程

```powershell
E:\code\Qt5.15.2\5.15.2\msvc2019_64\bin\lupdate.exe src -ts translations\lasviewer_zh_CN.ts
cmake --build out/build --config Release --target LASPointCloudViewer
```

## 发布与打包

### 打包脚本
```powershell
.\scripts\package_release.ps1 -Version v1.1.0 -Config Release -BuildBinDir out/build/bin/Release -OutputDir out/release
```

### 发布说明
- `docs/releases/`

## 工作区注意事项

- 工作区可能存在用户自己的未跟踪大 LAS，不要删除、改名或误提交。
- 提交时只加入本次相关文件，不要误带 `out/`、`.qm` 或本地测试数据。
- 用户说“提交”默认表示 `commit + push`。
- 提交信息默认中文。

## 本地规划文档工作流

- `docs/planningwithfiles/*.md`、`docs/brainstorming/`、根目录 `findings.md` / `progress.md` / `task_plan.md` 这类文件适合作为“实时工作草稿”，但不适合长期挂在日常 `git status` 里。
- 推荐分层：
  - 本地实时草稿：保留在工作区，用于当前会话整理、规划和中间结论。
  - 正式沉淀文档：确认有长期价值后，再整理提交到 `docs/agent/`、`docs/releases/` 或专题文档。
- 对未跟踪的本地草稿，优先使用 `.git/info/exclude` 做本地忽略，不修改仓库级 `.gitignore`：

```powershell
Add-Content .git\info\exclude @"
/.claude/
/docs/brainstorming/
/findings.md
/progress.md
/task_plan.md
"@
```

- 对已跟踪但只想本地维护的 planning 文件，可使用 `skip-worktree` 隐藏日常改动：

```powershell
git update-index --skip-worktree docs/planningwithfiles/findings.md docs/planningwithfiles/progress.md docs/planningwithfiles/task_plan.md
```

- 需要重新纳入 git 跟踪时，恢复为：

```powershell
git update-index --no-skip-worktree docs/planningwithfiles/findings.md docs/planningwithfiles/progress.md docs/planningwithfiles/task_plan.md
```

- 如果某次确实要提交被本地忽略的草稿文件，可显式强制加入：

```powershell
git add -f docs/brainstorming/...
```

- 使用 `skip-worktree` 后，要注意远端如果也修改了同名文件，本地不一定会第一时间显式提示；这类文件更适合作为个人工作副本，而不是多人同时编辑的正式文档。

## 文档更新规则

- 功能状态变化：
  - 更新 `product-state.md`
- 模块边界变化：
  - 更新 `architecture.md`
- 完成一轮较大的重构回归排查后：
  - 更新 `docs/agent/refactor-regression-report.md`
- 入口顺序或阅读路径变化：
  - 更新 `docs/agent/README.md`
- 强约束变化：
  - 更新根目录 `AGENTS.md`
