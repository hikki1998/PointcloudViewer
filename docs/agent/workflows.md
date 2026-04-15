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

## 文档更新规则

- 功能状态变化：
  - 更新 `product-state.md`
- 模块边界变化：
  - 更新 `architecture.md`
- 入口顺序或阅读路径变化：
  - 更新 `docs/agent/README.md`
- 强约束变化：
  - 更新根目录 `AGENTS.md`
