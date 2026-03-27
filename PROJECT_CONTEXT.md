# Project Context

## What This Repo Is
这是一个基于 Qt 5.15、OpenSceneGraph 和 LASlib 的 Windows 桌面点云查看器。当前重点不是通用 GIS 能力，而是点云浏览、测绘风格显示、量测和较稳定的本地构建体验。

## Fast Orientation
- 入口：`src/main.cpp`
- 主窗口与 Ribbon：`src/gui/MainWindow.cpp`
- 视图交互、量测、悬停拾取、状态栏：`src/gui/PointCloudViewer.cpp`
- 点云显示参数模型：`src/osg/PointCloudVisualization.h`
- 点云渲染与 shader：`src/osg/OsgPointCloudNode.cpp`
- LAS/LAZ 读取：`src/pointcloud/LasReader.cpp`
- 中文翻译：`translations/lasviewer_zh_CN.ts`
- 构建与部署逻辑：`CMakeLists.txt`

## Current User-Facing Features
- 加载 `.las/.laz`
- RGB、高程渐变、单色显示
- 点大小、透明度、深度雾化、EDL 风格增强、圆形 splat
- 顶视、前视、右视、适配视图
- 右上角坐标轴指示器，显示 `X+ / Y+ / Z+`
- 鼠标悬停点坐标显示
- 两点量测和覆盖层标注
- 中英文界面，其中中文翻译已接入构建和部署

## File Responsibilities
### `src/gui/MainWindow.*`
- Ribbon 动作
- 检查器面板和渲染控制
- 设置持久化
- 语言切换
- 与 `PointCloudViewer` 的信号槽连接

### `src/gui/PointCloudViewer.*`
- OSG 嵌入小部件
- 相机操纵器
- 点点击/悬停拾取
- 状态栏信息
- 量测逻辑
- 右上角坐标轴覆盖层

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
.\out\build\bin\smoke\Release\LASViewerSmokeTest.exe .\test_data\ezhou_powerline_sample.las
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
- 改相机或拾取逻辑后，最好同时验证缩放、悬停坐标、量测点选。
- CMake 依赖 Windows 和本地 Qt 路径，排查构建问题时优先看 `CMakeLists.txt` 和 `out/build/CMakeCache.txt`。

## What To Read For Typical Tasks
- “加一个显示选项”：
  - `src/osg/PointCloudVisualization.h`
  - `src/gui/MainWindow.cpp`
  - `src/gui/PointCloudViewer.cpp`
  - `src/osg/OsgPointCloudNode.cpp`
- “修 UI 或交互”：
  - `src/gui/MainWindow.cpp`
  - `src/gui/PointCloudViewer.cpp`
- “修翻译”：
  - `translations/lasviewer_zh_CN.ts`
  - `src/gui/MainWindow.cpp`
  - `src/gui/PointCloudViewer.cpp`
- “修构建或部署”：
  - `CMakeLists.txt`

## Goal For New Sessions
新对话的 Codex 应该在读完本文件后，直接知道：
- 这个项目是什么
- 入口和热区文件在哪里
- 当前已经有哪些显示与交互能力
- 改完后该怎么构建和验证
- 哪些本地数据不该误提交
