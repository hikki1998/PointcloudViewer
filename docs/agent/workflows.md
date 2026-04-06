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

### 运行
```powershell
.\out\build\bin\Release\LASPointCloudViewer.exe
```

### smoke test
```powershell
.\out\build\bin\Release\LASViewerSmokeTest.exe --mode viewer-render --las .\test_data\ezhou_powerline_sample.las
.\out\build\bin\Release\LASViewerSmokeTest.exe --mode route-roam --las .\test_data\ezhou_powerline_sample.las
```

## 常见改动路径

### 加一个显示参数
1. `src/osg/PointCloudVisualization.h`
2. `src/gui/MainWindow.*`
3. `src/gui/PointCloudViewer.*`
4. `src/osg/OsgPointCloudNode.cpp`

### 改 UI / Ribbon / dock / 表格 / 弹窗
1. `src/gui/MainWindow.h`
2. `src/gui/MainWindow.cpp`
3. 如果联动场景交互，再看 `src/gui/PointCloudViewer.*`

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

## 验证基线

### 默认最少验证
- 改 UI、交互、翻译、渲染、点选、构建脚本时：
  - 至少做一次 `Release` 构建

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
.\scripts\package_release.ps1 -Version v1.0.0 -Config Release -BuildBinDir out/build/bin/Release -OutputDir out/release
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
