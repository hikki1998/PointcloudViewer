# Architecture

## 顶层结构

- `src/gui/`
  - `MainWindow.*` 负责 Ribbon、dock、检查器、表格、动作和项目级状态组织。
  - `PointCloudViewer.*` 负责 OSG 嵌入、相机、拾取、覆盖层、漫游和场景交互。
- `src/osg/`
  - `OsgPointCloudNode.*` 负责点云几何与渲染状态。
  - `PointCloudVisualization.h` 是显示参数的单一模型入口。
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

### 点云加载与渲染
1. `LasReader` 读取 LAS/LAZ，填充 `PointCloudData`
2. `PointCloudViewer` 持有当前点云与可视化状态
3. `OsgPointCloudNode` 把点云转成 OSG 几何
4. `MainWindow` 通过检查器和 Ribbon 修改显示参数，再下发给 viewer

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
  - Backstage 页面、最近工程、应用设置入口
- `src/gui/MainWindow.Docks.cpp`
  - 左右/底部 dock、检查器区、量测区、日志区、状态栏
- `src/gui/MainWindow.Connections.cpp`
  - viewer、dock、controller、动作之间的信号槽连接
- `src/gui/MainWindow.PointCloud.cpp`
  - 点云打开、追加、清空、配色和基础显示同步
- `src/gui/MainWindow.Route.cpp`
  - 航线导入导出、编辑、焦点、表格刷新、漫游状态同步
- `src/gui/MainWindow.TowerIssue.cpp`
  - 杆塔/隐患面板、详情编辑器、导入导出与聚焦
- `src/gui/MainWindow.ProjectSerializer.cpp`
  - 工程文件 JSON 读写与工程内嵌状态恢复
- `src/gui/MainWindow.SettingsStore.cpp`
  - `QSettings` / `UiHistoryStore` 读写与窗口状态恢复
- `src/gui/MainWindow.Helpers.cpp`
  - 共享 helper、JSON 辅助转换、最近工程记录等稳定内部实现
- `src/gui/MainWindowInternal.h`
  - `MainWindow` 拆分后共享的最小内部声明与常量

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

- 显示参数不要分散定义，统一收口到 `PointCloudVisualization.h`。
- 业务模型不要直接耦合到 OSG 绘制结构，尽量经由 viewer/bridge 投影到显示层。
- 工程文件、外部 route 文件、导出格式是三个不同边界，不要混成一个模型层。
- 对已有大文件，优先沿现有结构最小侵入修改；只有在职责已经明显失控时才拆分。
- `MainWindow.ProjectSerializer.cpp` 和 `MainWindow.SettingsStore.cpp` 是正式编译单元，不要再用 `.cpp` 包含 `.cpp` 的方式继续扩展。
