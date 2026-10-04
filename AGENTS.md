# Repository Guidelines

## Communication
- 默认使用中文，除非用户明确要求英文。
- 回答尽量简洁，优先说明结果、验证状态和下一步。
- 较大改动前先发一句短进度说明。

## First Read
- 新对话先读 `docs/agent/context.md`。
- 需要确认当前能力或限制时读 `docs/agent/product-state.md`。
- 涉及构建、测试、翻译或发布时读 `docs/agent/workflows.md`。
- 不要从旧发布说明或历史提交推断当前状态。

## Safety
- 禁止格式化磁盘或清理当前工程目录以外的路径。
- 不要删除、改名或提交用户自己的未跟踪 LAS/LAZ/PLY 大文件。
- 强推、历史重写、reset、批量删除等不可逆操作必须先说明影响。

## Core Structure
- `src/gui/`：MainWindow、viewer、Ribbon、dock、交互和覆盖层。
- `src/pointcloud/`：LAS/LAZ 读取和点云数据模型。
- `src/osg/`：LAS/LAZ OSG 几何与渲染状态。
- `src/gaussian/`：3D Gaussian PLY 解析和原生 OpenGL 渲染。
- `src/crs/`：工程 CRS、目录、选择对话框和 PROJ 转换。
- `src/domain/`：杆塔、隐患、净空、剖面和报告业务。
- `src/route/`：航线模型、JSON/KML/KMZ、规划和 QA。
- `src/capture/`：Windows 内嵌录屏。
- `examples/`：唯一 smoke 可执行文件的各场景实现。

## Change Placement
- UI/Ribbon/dock：修改对应职责的 `src/gui/MainWindow.*.cpp`，不要把逻辑重新堆回 `MainWindow.cpp`。
- Viewer 功能：修改对应的 `PointCloudViewer.*.cpp`；通用场景和拾取才放 `PointCloudViewer.cpp`。
- 点云显示参数统一定义在 `src/osg/PointCloudVisualization.h`。
- 点云渲染修改优先看 `src/osg/OsgPointCloudNode.cpp`。
- 航线修改优先看 `src/route/*`、`MainWindow.Route*.cpp`、`PointCloudViewer.Route*.cpp`。
- 工程 CRS 修改优先看 `src/crs/*` 和 `MainWindow.ProjectSerializer.cpp`。
- 新共享源码登记到所属目录 `CMakeLists.txt`，通过 `LASViewerCoreObj` 同时供主程序和 smoke 使用。

## Coding Conventions
- 使用 C++17。
- 4 空格缩进；函数大括号换行；控制语句大括号同行。
- 类名 `PascalCase`，函数和局部变量 `lowerCamelCase`，常量 `kPrefix`，成员尾随 `_`。
- 头文件按 Qt、第三方、项目头分组。
- 不新增投机性抽象、单实现接口或无实际使用的配置。
- UI 默认浅色背景和深色高对比文字，覆盖 Ribbon、dock、弹窗、菜单、表格、ComboBox 和覆盖层。

## Build And Validation
用户已授权常规拉取、配置、构建和 smoke，可直接执行。

```powershell
cmake -S . -B out/build -G "Visual Studio 17 2022" -A x64 -DQT_ROOT=E:/code/Qt5.15.2/5.15.2/msvc2019_64
cmake --build out/build --config Release --target LASPointCloudViewer LASViewerSmokeTest
.\out\build\bin\Release\LASViewerSmokeTest.exe --mode all --las .\test_data\ezhou_powerline_sample.las
```

只验证编译、跳过部署：

```powershell
cmake --build out/build --config Release --target LASPointCloudViewer LASViewerSmokeTest -- /p:PostBuildEventUseInBuild=false
```

- 仓库只保留 `LASViewerSmokeTest.exe` 一个 smoke 可执行文件；新增场景必须增加 mode/category。
- UI、渲染、交互、翻译、构建脚本修改至少做 Release 构建；显示结果变化优先跑相关 smoke 或全量 smoke。
- UI 样式修改额外检查深色背景压住深色文字、下拉列表和选中/悬停态可读性。

## Translation
新增或修改 UI 文本后：

```powershell
E:\code\Qt5.15.2\5.15.2\msvc2019_64\bin\lupdate.exe src -ts translations\lasviewer_zh_CN.ts
```

补全中文后重新构建；若跳过部署，需要手动同步 `.qm` 到运行目录。

## Planning And Git
- 规划状态只能放在 `.planning/<plan-id>/`，不要在项目根目录或 `docs/` 新建 `task_plan.md`、`findings.md`、`progress.md`。
- 用户说“提交”默认表示 `commit + push`。
- 提交信息默认中文。
- 只加入本次相关文件，不带入 `out/`、本地测试数据或无关改动。
- 推送失败时说明原因和最新本地提交号。
