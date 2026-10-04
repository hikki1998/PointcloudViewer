# Agent Context

## Product

LAS Point Cloud Viewer 是基于 Qt 5.15、OpenSceneGraph 和 LASlib/LASzip 的桌面点云工具，主要面向 Windows，并维护 Ubuntu 22.04 构建路径。产品聚焦电力巡检和通道检查，不是通用 GIS 平台。

当前主要能力：

- 后台加载一个或多个 LAS/LAZ，支持进度、取消、渐进 preview 和交互 LOD。
- 多数据集独立 OSG 节点；日常浏览不保留完整合并副本，分析时才按需生成兼容缓存。
- RGB、高程、单色、分类着色；分类显隐、框选/多边形编辑和 Undo/Redo。
- 点大小、透明度、Depth Cue、EDL 风格和圆形 splat。
- 多边形/盒裁剪、裁剪预览和 LAS 导出。
- 连续量测、净空分析、剖面、杆塔、隐患和报告导出。
- 工程级点云/地理 CRS、PROJ 转换和 KML/KMZ 互操作。
- 航线导入导出、多目标航点编辑、Route QA、相机预览和漫游。
- 单个 3D Gaussian Splatting PLY 的 OpenGL 4.3 GPU 渲染。
- 工程保存/恢复、最近工作台、缩略图缓存和中英文界面。
- Windows 内嵌 MP4(H.264) 录屏；Linux 当前不包含该录屏后端。

明确边界见 `product-state.md`。

## Architecture

### Build Targets

- `LASViewerCoreObj`：`src/*` 共享实现和资源。
- `LASPointCloudViewer`：主程序入口。
- `LASViewerSmokeTest`：唯一 smoke 可执行文件，场景位于 `examples/*Smoke.cpp`。

源码通过各目录的 `CMakeLists.txt` 就近登记；顶层 `CMakeLists.txt` 只负责目标和模块接线。

### LAS/LAZ Pipeline

1. 用户打开、追加或拖放时，`PointCloudViewer.Loading.cpp` 在工作线程读取 LAS/LAZ。
2. 工作线程同时建立轻量 XY 拾取索引、交互 preview 和相对原点的 float OSG CPU 几何。
3. UI 线程原子提交已准备节点；大文件单文件先显示最多约 16 万 preview 点。
4. 超过阈值的数据集额外保留最多约 18 万点交互节点；相机运动时通过 `NodeMask` 显示 preview，释放并静止后恢复完整节点。
5. 每个数据集保留独立 `PointCloudData` 和场景节点；需要连续全量数组的分析 API 才生成合并缓存。
6. `OsgPointCloudNode` 使用相对原点 `Vec3`、`Vec4ub` 颜色和 VBO；点大小、透明度、背景、Depth Cue、EDL 和圆形 splat 直接更新 State/Uniform。

工程恢复仍保留同步兼容入口；GPU VBO 首次上传仍需要有效 OpenGL context。

### Gaussian Pipeline

1. `GaussianPlyReader` 读取 binary little-endian、float32 3DGS PLY。
2. 工作线程构建 `GaussianModel`。
3. `OsgWidget` 复用 OSG 相机，在同一 `QOpenGLWidget` 中调用原生 `GaussianRenderer`。
4. renderer 使用 OpenGL 4.3 SSBO、排序和实例化 quad。

Gaussian 不进入 `PointCloudData`，当前不与 LAS/LAZ 或业务 overlay 同屏混合。

### Project And Business Data

- `MainWindow.ProjectSerializer.cpp` 保存多数据集、显示参数、工程 CRS、杆塔、隐患和航线状态。
- `src/crs/` 提供 CRS 模型、常用目录、authority 查询、选择对话框和 PROJ 转换。
- `src/domain/` 承载杆塔、隐患、净空、剖面投影和报告。
- `src/route/` 承载标准航线模型、JSON、KML/KMZ、规划和 QA。
- `MainWindow` 组织 UI 和项目状态；业务模型不要直接依赖 OSG。

## File Map

### Main Window

| 文件 | 职责 |
|---|---|
| `MainWindow.Core.cpp` | 生命周期、拖放、窗口行为和录屏生命周期 |
| `MainWindow.Actions.cpp` / `MainWindow.Ribbon.cpp` | 动作和 Ribbon |
| `MainWindow.Backstage.cpp` | Backstage、最近工程/数据和缩略图调度 |
| `MainWindow.Docks.cpp` | dock、检查器、状态栏和日志 |
| `MainWindow.Connections.cpp` | 连接总入口 |
| `MainWindow.ControllerConnections.cpp` | controller 和业务动作接线 |
| `MainWindow.ViewerConnections.cpp` | viewer 和全局 UI 状态接线 |
| `MainWindow.PointCloud.cpp` | 点云打开、追加、清空和基础显示 |
| `MainWindow.ProjectExplorer.cpp` | 项目树 |
| `MainWindow.Analysis.cpp` | 量测、净空和植被分析 |
| `MainWindow.ProfileClassification.cpp` | 分类编辑和 LAS 保存 |
| `MainWindow.Route.cpp` / `MainWindow.RouteEditor.cpp` | 航线状态和航点编辑 |
| `MainWindow.TowerIssue.cpp` | 杆塔与隐患 |
| `MainWindow.ProjectSerializer.cpp` | 工程 JSON |
| `MainWindow.SettingsStore.cpp` | QSettings 和 UI 历史 |
| `MainWindow.Capture.cpp` | 截图和录屏落盘 |
| `MainWindow.Helpers.cpp` | 共享内部 helper |

### Viewer

| 文件 | 职责 |
|---|---|
| `OsgWidget.*` | Qt/OpenGL/OSG 嵌入和原始输入 |
| `PointCloudViewer.cpp` | 通用场景、相机、拾取和基础显示 |
| `PointCloudViewer.Loading.cpp` | LAS/LAZ/Gaussian 加载和清空 |
| `PointCloudViewer.Clip.cpp` | 裁剪 |
| `PointCloudViewer.Classification.cpp` | 分类编辑 |
| `PointCloudViewer.Measurement.cpp` | 量测 |
| `PointCloudViewer.Markers.cpp` | 杆塔和隐患 overlay |
| `PointCloudViewer.Route.cpp` / `RouteRoam.cpp` | 航线显示、编辑和漫游 |
| `PointCloudViewerOverlays.*` | 共用多边形选择覆盖层 |

### Other Hot Paths

- LAS/LAZ：`src/pointcloud/LasReader.*`、`PointCloudData.*`
- OSG：`src/osg/OsgPointCloudNode.*`、`PointCloudVisualization.h`
- Gaussian：`src/gaussian/*`
- CRS：`src/crs/*`
- 航线：`src/route/*`
- 业务：`src/domain/*`
- 最近工作台：`WelcomeWorkspaceWidget.*`、`WorkspaceThumbnailCache.*`
- Smoke：`examples/*Smoke.cpp`、`viewer_smoke_test.cpp`

## Common Task Routing

- 显示参数：`PointCloudVisualization.h` → `MainWindow.Docks.cpp`/`PointCloudViewer.*` → `OsgPointCloudNode.cpp`
- 点云加载：`LasReader.*`、`PointCloudViewer.Loading.cpp`、`MainWindow.PointCloud.cpp`
- 拾取/相机：`OsgWidget.*`、`PointCloudViewer.cpp`
- 量测/覆盖层：对应的 `PointCloudViewer.Measurement.cpp` 或 `Markers.cpp`
- 航线：`src/route/*`、`MainWindow.Route*.cpp`、`PointCloudViewer.Route*.cpp`
- CRS：`src/crs/*`、`MainWindow.ProjectSerializer.cpp`、`RouteInterop.cpp`
- 工程：`MainWindow.ProjectSerializer.cpp`、`SettingsStore.cpp`
- 构建：`CMakeLists.txt`、`cmake/*.cmake`、各目录 `CMakeLists.txt`
- 翻译：`translations/lasviewer_zh_CN.ts`

构建、smoke、翻译和发布命令见 `workflows.md`。
