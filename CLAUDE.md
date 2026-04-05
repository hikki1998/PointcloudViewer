# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What This Is

Windows 桌面点云查看器，面向电力巡检/通道检查场景。基于 Qt 5.15.2 + OpenSceneGraph 3.6.5 + LASlib，支持 `.las/.laz` 加载、量测、净空分析、杆塔/隐患管理、巡检航线规划。语言 C++17，编译器 MSVC (VS2022)，构建系统 CMake。

## Build & Run Commands

```powershell
# 配置（只需一次）
cmake -S . -B out/build -G "Visual Studio 17 2022" -A x64 -DQT_ROOT=E:/code/Qt5.15.2/5.15.2/msvc2019_64

# 构建主程序 + smoke test
cmake --build out/build --config Release --target LASPointCloudViewer LASViewerSmokeTest

# 运行
.\out\build\bin\Release\LASPointCloudViewer.exe
.\out\build\bin\Release\LASViewerSmokeTest.exe .\test_data\ezhou_powerline_sample.las

# 更新中文翻译（新增 UI 文字后）
E:\code\Qt5.15.2\5.15.2\msvc2019_64\bin\lupdate.exe src -ts translations\lasviewer_zh_CN.ts
# 然后重新构建以生成 .qm
```

涉及 UI、翻译、渲染、点选、量测、构建脚本的改动，默认至少做一次构建验证。

## Architecture

### 渲染参数流转

所有点云显示参数（点大小、透明度、雾化、EDL、splat 等）统一定义在 `src/osg/PointCloudVisualization.h`，由 `MainWindow` 的 Ribbon 控件修改，通过 `PointCloudViewer` 传递给 `OsgPointCloudNode` 的 shader uniform。新增显示参数必须同时改这三层，否则控件无效。

### GUI 与 OSG 嵌入

`PointCloudViewer` 是 OSG 嵌入 Qt 的核心小部件，负责相机操纵器、点拾取/悬停、量测逻辑、覆盖层绘制。`MainWindow` 管理 Ribbon（QtitanRibbon）、左右底 dock、检查器面板，通过信号槽与 `PointCloudViewer` 通信。

### 业务模型层

- `src/domain/InspectionData.*` — 杆塔业务属性 + 隐患台账模型 + 工程 JSON 序列化
- `src/domain/ClearanceAnalysis.*` — 量测路径转净空分段，阈值预警
- `src/domain/ProfileMarkerProjection.*` — 杆塔/隐患投影到量测剖面
- `src/gui/ProfilePlotWidget.*` — 剖面绘制 + 预警高亮 + 业务标记

### 航线模块

`src/route/` 独立收口标准航线 JSON 模型。`PowerlineRouteTypes.h` 定义主模型，`PowerlineRouteJson` 负责 IO，`PowerlineRouteBridge` 桥接到显示层和导出格式（KML/KMZ）。工程文件通过外部文件关联保存航线。

### 坐标参考系

`src/crs/` 提供 CRS 选择对话框和 PROJ 转换服务。PROJ 为可选依赖，编译宏 `LAS_VIEWER_HAS_PROJ` 控制。

### 三方依赖

仓库内 `3rd/` 包含 `osg/`、`qtitan/`、`laslib/`、`lastools/` 精简版 release，每个目录有 `.version` 标记。Qt 不随仓库分发，需通过 `QT_ROOT` 指定。CMake 的 `las_viewer_resolve_package_root` 函数统一处理依赖探测。

## Coding Conventions

- C++17，4 空格缩进，函数大括号换行，控制语句大括号同行
- 类名 `PascalCase`，函数/局部变量 `lowerCamelCase`，常量 `kPrefix`，成员变量尾随下划线
- 头文件分组：Qt -> 第三方 -> 项目，组间空行
- 默认中文沟通，commit message 中文
- 用户说"提交"默认表示 `commit + push`
- 提交时只加入本次相关文件，不误带 `out/` 或大 LAS

## UI Readability Constraint

UI 可读性为强约束：所有新增或改造 UI 默认浅色背景 + 深色高对比文字。必须覆盖 dock、Ribbon、Message Box、表格、ComboBox（含下拉列表）和覆盖层，禁止深色背景压住深色文字。对 Message Box 和自定义弹窗，禁止深色底叠深色文字；使用样式表时需显式指定浅色背景与可读文本颜色。

## Common Pitfalls

- 只改 UI 不改渲染层 → 控件无效
- 新增 UI 文本不更新 `.ts/.qm` → 漏翻译
- 改相机或拾取逻辑后要同时验证缩放、悬停坐标、量测点选、杆塔/隐患选择
- 改工程文件结构后要检查旧工程兼容和新字段保存加载
- 改量测逻辑后要检查量测表格、剖面 dock 和导出结果是否一致
