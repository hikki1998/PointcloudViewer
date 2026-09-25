# Architecture

## 顶层结构

- `src/gui/`
  - `MainWindow.*` 负责 Ribbon、dock、检查器、表格、动作和项目级状态组织。
  - `PointCloudViewer.*` 负责 OSG 嵌入、相机、拾取、覆盖层、漫游和场景交互。
- `src/osg/`
  - `OsgPointCloudNode.*` 负责 LAS/LAZ 点云几何与渲染状态。
  - `PointCloudVisualization.h` 是 LAS/LAZ 显示参数的单一模型入口。
- `src/gaussian/`
  - `GaussianModel`、`GaussianPlyReader`、`GaussianRenderer` 负责 3DGS PLY 数据、解析和原生 OpenGL GPU 渲染。
- `src/pointcloud/`
  - `LasReader.*` 负责 LAS/LAZ 读取。
  - `PointCloudData.*` 负责点云基础数据结构。
- `src/domain/`
  - 杆塔、隐患、净空分析、剖面投影、工程数据序列化。
- `src/route/`
  - 标准航线模型、JSON IO、桥接导出、航线规划与 QA。
- `examples/`
  - smoke test 与开发验证入口。

## 构建目标分层

- `LASViewerCoreObj`（OBJECT）
  - 承载 `src/*` 下共享编译单元和 `resources.qrc`。
  - 由 `las_viewer_add_shared_sources(...)` 注入源码。
- `LASPointCloudViewer`（EXE）
  - 只保留主程序入口与可执行目标配置。
  - 通过链接 `LASViewerCoreObj` 复用共享对象文件。
- `LASViewerSmokeTest`（EXE）
  - 只保留 smoke 入口与可执行目标配置。
  - 通过链接 `LASViewerCoreObj` 复用共享对象文件。

该分层的目的：避免主程序与 smoke 对同一批 `.cpp` 重复编译，同时继续保留双可执行入口与统一 smoke mode 机制。

## 核心运行链路

### LAS/LAZ 加载与渲染
1. `LasReader` 读取 LAS/LAZ，填充 `PointCloudData`
2. `PointCloudViewer` 持有当前点云与可视化状态
3. `OsgPointCloudNode` 把点云转成 OSG 几何
4. `MainWindow` 通过检查器和 Ribbon 修改显示参数，再下发给 viewer

### Gaussian PLY 加载与渲染
1. `GaussianPlyReader` 校验并解析受支持的 binary little-endian 3DGS PLY
2. 大型 PLY 在工作线程构建 `GaussianModel`
3. `OsgWidget` 复用 OSG 相机和输入，在同一 `QOpenGLWidget` 中调用 `GaussianRenderer`
4. `GaussianRenderer` 使用 OpenGL 4.3 SSBO、排序、实例化 quad 和 alpha blending 绘制
5. 当前视图一次只承载一种主数据类型：LAS/LAZ 或单个 Gaussian；不支持混载和同屏深度组合

### 空场景最近工作台
1. `MainWindow.Backstage.cpp` 从 `QSettings` 读取最近工程/数据并解析轻量工程摘要
2. `WelcomeWorkspaceWidget` 显示可交互列表、缺失状态和右键菜单
3. 场景稳定渲染后，`PointCloudViewer`/`OsgWidget` 提供 framebuffer，`WorkspaceThumbnailCache` 被动保存缩略图
4. 启动工作台只读工程 JSON 和缩略图缓存，不重新读取 LAS/LAZ/PLY

### 场景交互
1. `OsgWidget` 负责 Qt 鼠标/滚轮/键盘事件
2. `PointCloudViewer` 接管拾取、悬停、量测、杆塔/隐患/航点编辑
3. 交互结果通过信号槽同步到 `MainWindow` 的表格、dock、状态栏

### 航线模块
1. `PowerlineRouteTypes.h`
  - 定义标准航线模型 `PowerlineRouteDocument`
2. `PowerlineRouteJson.*`
  - 负责标准 JSON 导入、导出、校验
3. `PowerlineRouteBridge.*`
  - 把标准航线模型桥接为显示层和导出层需要的数据
4. `InspectionRoutePlanning.*`
  - 承载生成、QA、机型相关规划逻辑
5. `MainWindow` / `PointCloudViewer`
  - 实现航线表格、场景 overlay、编辑、预览、漫游

## MainWindow 拆分边界

- `src/gui/MainWindow.Core.cpp`
  - 主窗口构造、关闭、拖放、窗口事件与无边框窗口行为
- `src/gui/MainWindow.Actions.cpp`
  - QAction 创建与动作分组
- `src/gui/MainWindow.Ribbon.cpp`
  - Ribbon 页面、组、快速工具栏、窗口控制按钮
- `src/gui/MainWindow.Backstage.cpp`
  - Backstage 页面、最近工程/数据工作台、缩略图调度、应用设置入口
- `src/gui/MainWindow.Docks.cpp`
  - 左右/底部 dock、检查器区、量测区、日志区、状态栏
- `src/gui/MainWindow.Connections.cpp`
  - 连接创建总入口
- `src/gui/MainWindow.ControllerConnections.cpp`
  - controller、dock 和业务动作之间的信号槽连接
- `src/gui/MainWindow.ViewerConnections.cpp`
  - 主窗口动作、viewer 和全局 UI 状态之间的信号槽连接
- `src/gui/MainWindow.Capture.cpp`
  - 截图、录屏和捕获文件落盘
- `src/gui/MainWindow.Analysis.cpp`
  - 量测、净空与植被分析面板同步
- `src/gui/MainWindow.ProfileClassification.cpp`
  - 分类配色表、分类编辑状态与 LAS 保存
- `src/gui/MainWindow.ProjectExplorer.cpp`
  - 项目树构建、过滤、可见性和上下文菜单
- `src/gui/MainWindow.PointCloud.cpp`
  - 点云打开、追加、清空、配色和基础显示同步
- `src/gui/MainWindow.Route.cpp`
  - 航线导入导出、焦点、表格刷新、漫游状态同步
- `src/gui/MainWindow.RouteEditor.cpp`
  - 航点编辑对话框、实时预览、保存/取消恢复
- `src/gui/MainWindow.TowerIssue.cpp`
  - 杆塔/隐患面板、详情编辑器、导入导出与聚焦
- `src/gui/MainWindow.ProjectSerializer.cpp`
  - 工程文件 JSON 读写与工程内嵌状态恢复
- `src/gui/MainWindow.SettingsStore.cpp`
  - `QSettings` / `UiHistoryStore` 读写与窗口状态恢复
- `src/gui/MainWindow.Helpers.cpp`
  - 共享 helper、JSON 辅助转换、最近工程路径归一化等稳定内部实现
- `src/gui/MainWindowInternal.h`
  - `MainWindow` 拆分后共享的最小内部声明与常量

## PointCloudViewer 拆分边界

- `src/gui/OsgWidget.*`
  - Qt/OpenGL/OSG 嵌入和鼠标、键盘、滚轮事件桥接
  - 持有并调用原生 `GaussianRenderer`，复用 OSG 相机矩阵
- `src/gui/PointCloudViewer.cpp`
  - Viewer 通用交互、场景、拾取、Overlay 和基础显示状态
- `src/gui/PointCloudViewer.Loading.cpp`
  - LAS/LAZ/Gaussian 加载、追加与清空
  - Gaussian 单模型异步解析、GPU 上传和失败状态恢复
- `src/gui/PointCloudViewer.Clip.cpp`
  - 多边形/盒裁剪、裁剪预览与导出
- `src/gui/PointCloudViewer.Classification.cpp`
  - 分类配色可见性、框选/多边形分类任务与 Undo/Redo
- `src/gui/PointCloudViewer.Measurement.cpp`
  - 连续量测状态、计算、OSG overlay 与 Qt 标签
- `src/gui/PointCloudViewer.Markers.cpp`
  - 杆塔/隐患状态、拾取、OSG overlay 与 Qt 标签
- `src/gui/PointCloudViewerOverlays.*`
  - 裁剪和分类共用的多边形选择覆盖层
- `src/gui/PointCloudViewer.Route.cpp`
  - 航线显示数据、标签、颜色、可见性和编辑状态
- `src/gui/PointCloudViewer.RouteRoam.cpp`
  - 航线漫游状态机、相机位姿和模拟拍照

## Smoke 编译单元

`LASViewerSmokeTest.exe` 仍是唯一 smoke 可执行文件，场景实现按职责拆在 `examples/*Smoke.cpp`，`viewer_smoke_test.cpp` 只保留公共 helper、命令行入口和场景注册。

## 当前高热文件

### UI / 交互
- `src/gui/MainWindow.cpp`
- `src/gui/MainWindow.Docks.cpp`
- `src/gui/MainWindow.Route.cpp`
- `src/gui/MainWindow.ProjectSerializer.cpp`
- `src/gui/MainWindow.SettingsStore.cpp`
- `src/gui/MainWindow.h`
- `src/gui/PointCloudViewer.cpp`
- `src/gui/PointCloudViewer.h`

### 渲染
- `src/osg/OsgPointCloudNode.cpp`
- `src/osg/PointCloudVisualization.h`
- `src/gaussian/GaussianPlyReader.cpp`
- `src/gaussian/GaussianRenderer.cpp`
- `src/gui/OsgWidget.cpp`

### 空场景工作台 / 工程入口
- `src/gui/WelcomeWorkspaceWidget.*`
- `src/gui/WorkspaceThumbnailCache.*`
- `src/gui/MainWindow.Backstage.cpp`
- `src/gui/MainWindow.ViewerConnections.cpp`

### 巡检业务
- `src/domain/InspectionData.*`
- `src/domain/ClearanceAnalysis.*`
- `src/domain/ProfileMarkerProjection.*`
- `src/gui/ProfilePlotWidget.*`

### 航线
- `src/route/PowerlineRouteTypes.h`
- `src/route/PowerlineRouteJson.*`
- `src/route/PowerlineRouteBridge.*`
- `src/route/InspectionRoutePlanning.*`

## 约束边界

- LAS/LAZ 显示参数不要分散定义，统一收口到 `PointCloudVisualization.h`；Gaussian 专属参数留在 Gaussian 模块，不要硬塞进 LAS 数据模型。
- 业务模型不要直接耦合到 OSG 绘制结构，尽量经由 viewer/bridge 投影到显示层。
- 工程文件、外部 route 文件、导出格式是三个不同边界，不要混成一个模型层。
- 对已有大文件，优先沿现有结构最小侵入修改；只有在职责已经明显失控时才拆分。
- 所有职责拆分文件都是正式编译单元，不要用 `.cpp` 包含 `.cpp` 或 `.inc` 聚合来伪拆分。
