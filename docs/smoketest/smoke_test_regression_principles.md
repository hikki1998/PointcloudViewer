# LASViewer Smoke Test 各模式回归原理（代码结合版）

本文说明本项目如何在**无人工干预**情况下执行 GUI smoke test，并按 mode 解释回归覆盖意图与代码路径。

## 1. Smoke Test 总体架构

- 单一冒烟可执行：`LASViewerSmokeTest` 在顶层 CMake 声明，源码来自一个统一入口文件。
  - 可执行声明：[CMakeLists.txt#L52](../../CMakeLists.txt#L52)
  - `examples` 目录挂载 smoke 源：[examples/CMakeLists.txt#L1](../../examples/CMakeLists.txt#L1)
  - `las_viewer_add_smoke_sources()` 定义：[cmake/LASViewerTargetConfig.cmake#L7](../../cmake/LASViewerTargetConfig.cmake#L7)
- 统一入口文件：
  - [examples/viewer_smoke_test.cpp#L4738](../../examples/viewer_smoke_test.cpp#L4738)
- 每个 smoke case 用同一数据结构注册：`mode/category/displayName/requiresLas/run`。
  - 结构定义：[examples/viewer_smoke_test.cpp#L87](../../examples/viewer_smoke_test.cpp#L87)

结论：仓库通过“**单 exe + mode 分发**”实现回归扩展，避免新增多个分散的 smoke 程序。

## 2. 从命令行到模式分发

### 2.1 参数解析

- `--mode/-m`、`--category/-c`、`--las/-l` 在 main 中注册：
  - [examples/viewer_smoke_test.cpp#L4761](../../examples/viewer_smoke_test.cpp#L4761)
  - [examples/viewer_smoke_test.cpp#L4765](../../examples/viewer_smoke_test.cpp#L4765)
  - [examples/viewer_smoke_test.cpp#L4769](../../examples/viewer_smoke_test.cpp#L4769)
- 多值支持（逗号分隔）由 `parseCsvValues()` 统一处理：
  - [examples/viewer_smoke_test.cpp#L4569](../../examples/viewer_smoke_test.cpp#L4569)

### 2.2 输入数据解析

- `resolveLasInputs()` 合并 `--las` 与位置参数。
- 若未提供 LAS，默认使用 `./test_data/ezhou_powerline_sample.las`。
  - [examples/viewer_smoke_test.cpp#L4604](../../examples/viewer_smoke_test.cpp#L4604)

### 2.3 选择校验与调度

- `validateSelections()` 校验 mode/category 合法性，并禁止 `all` 与其他值混用：
  - [examples/viewer_smoke_test.cpp#L4625](../../examples/viewer_smoke_test.cpp#L4625)
- `runSelectedSmokes()` 负责：
  - 按 mode/category 过滤测试集合
  - 依据 `requiresLas` 先检查文件是否存在
  - 顺序执行 `smokeCase.run(lasFiles)`
  - 输出统一 summary
  - [examples/viewer_smoke_test.cpp#L4679](../../examples/viewer_smoke_test.cpp#L4679)
  - [examples/viewer_smoke_test.cpp#L4711](../../examples/viewer_smoke_test.cpp#L4711)
  - [examples/viewer_smoke_test.cpp#L4720](../../examples/viewer_smoke_test.cpp#L4720)
  - [examples/viewer_smoke_test.cpp#L4731](../../examples/viewer_smoke_test.cpp#L4731)

结论：`--mode all` 本质是选择 `smokeCases` 列表中的全部 case，执行顺序即列表声明顺序。

## 3. 无人工干预复现机制

### 3.1 事件泵与时序推进

- `pumpEvents()` 通过 `QCoreApplication::processEvents + QThread::msleep` 推进 UI/信号处理：
  - [examples/viewer_smoke_test.cpp#L97](../../examples/viewer_smoke_test.cpp#L97)

### 3.2 直接触发交互链路

- 直接触发 `QAction`：`trigger()`
- 直接触发控件：`setChecked/setValue/setCurrentIndex/click/editingFinished`
- 直接发送鼠标事件：`QApplication::sendEvent`（例如 viewer 轨道拖拽）
  - 典型实现：[examples/viewer_smoke_test.cpp#L580](../../examples/viewer_smoke_test.cpp#L580)

### 3.3 绕开真实鼠标的人机输入替代

- 右键菜单复现：`QMetaObject::invokeMethod(...customContextMenuRequested...)`
  - 表格：[examples/viewer_smoke_test.cpp#L126](../../examples/viewer_smoke_test.cpp#L126)
  - 树控件：[examples/viewer_smoke_test.cpp#L162](../../examples/viewer_smoke_test.cpp#L162)
- 双击复现：`cellDoubleClicked/itemDoubleClicked`
  - [examples/viewer_smoke_test.cpp#L198](../../examples/viewer_smoke_test.cpp#L198)
  - [examples/viewer_smoke_test.cpp#L220](../../examples/viewer_smoke_test.cpp#L220)
- 颜色对话框自动接受：`QTimer::singleShot` + `QColorDialog::accept`
  - [examples/viewer_smoke_test.cpp#L246](../../examples/viewer_smoke_test.cpp#L246)

结论：测试不是“截图脚本”，而是 Qt 事件系统上的程序化驱动，回归重点在信号槽、状态机和数据同步。

## 4. 数据加载与状态重置原理

### 4.1 Smoke 对真实加载逻辑的调用路径

- smoke 模式里通常直接调用 `PointCloudViewer::loadPointCloud()` 或 `loadPointCloudFiles()`，不是 mock。
  - `runViewerRenderSmoke`：
    - [examples/viewer_smoke_test.cpp#L580](../../examples/viewer_smoke_test.cpp#L580)
  - `runProjectExplorerMainWindowSmoke`：
    - [examples/viewer_smoke_test.cpp#L2535](../../examples/viewer_smoke_test.cpp#L2535)
  - `runMeasurementAnalysisControllerSmoke`：
    - [examples/viewer_smoke_test.cpp#L3041](../../examples/viewer_smoke_test.cpp#L3041)
  - `runRouteRoamStateSmoke`：
    - [examples/viewer_smoke_test.cpp#L3826](../../examples/viewer_smoke_test.cpp#L3826)

### 4.2 Viewer 内部加载链路

- 单文件入口转多文件入口：
  - [src/gui/PointCloudViewer.cpp#L1710](../../src/gui/PointCloudViewer.cpp#L1710)
- `loadPointCloudFiles()` 关键步骤：
  - 归一化/去重路径：[src/gui/PointCloudViewer.cpp#L1736](../../src/gui/PointCloudViewer.cpp#L1736)
  - 启动 loading 状态与进度信号：[src/gui/PointCloudViewer.cpp#L1764](../../src/gui/PointCloudViewer.cpp#L1764)
  - `LasReader::read(...)` 真正读取 LAS/LAZ：[src/gui/PointCloudViewer.cpp#L1802](../../src/gui/PointCloudViewer.cpp#L1802)
  - 读取期间 `processEvents` 保持 UI 活性：[src/gui/PointCloudViewer.cpp#L1800](../../src/gui/PointCloudViewer.cpp#L1800)
  - 同步 DataManager 数据集：[src/gui/PointCloudViewer.cpp#L1847](../../src/gui/PointCloudViewer.cpp#L1847)
  - 重建合并点云与场景：[src/gui/PointCloudViewer.cpp#L1889](../../src/gui/PointCloudViewer.cpp#L1889)
  - 视角 preset 与 loaded 信号：[src/gui/PointCloudViewer.cpp#L1891](../../src/gui/PointCloudViewer.cpp#L1891), [src/gui/PointCloudViewer.cpp#L1906](../../src/gui/PointCloudViewer.cpp#L1906)
- `appendPointCloudFiles()` 采用相同 reader+progress+merge 流程：
  - [src/gui/PointCloudViewer.cpp#L1918](../../src/gui/PointCloudViewer.cpp#L1918)
- `clearPointCloud()` 清理点云/杆塔/隐患/航线/分类编辑态，并停止漫游：
  - [src/gui/PointCloudViewer.cpp#L2073](../../src/gui/PointCloudViewer.cpp#L2073)
  - [src/gui/PointCloudViewer.cpp#L2075](../../src/gui/PointCloudViewer.cpp#L2075)

### 4.3 MainWindow 层业务入口（真实产品链路）

- 点云打开/追加/清空均走 MainWindow 封装，再转给 viewer：
  - [src/gui/MainWindow.PointCloud.cpp#L35](../../src/gui/MainWindow.PointCloud.cpp#L35)
  - [src/gui/MainWindow.PointCloud.cpp#L68](../../src/gui/MainWindow.PointCloud.cpp#L68)
  - [src/gui/MainWindow.PointCloud.cpp#L99](../../src/gui/MainWindow.PointCloud.cpp#L99)
- 工程文件加载（`.lpproj`）会读取 JSON 并调用 `viewer_->loadPointCloudFiles(...)` 再恢复可视化/业务状态：
  - [src/gui/MainWindow.ProjectSerializer.cpp#L22](../../src/gui/MainWindow.ProjectSerializer.cpp#L22)
  - [src/gui/MainWindow.ProjectSerializer.cpp#L59](../../src/gui/MainWindow.ProjectSerializer.cpp#L59)

## 5. 各模式回归原理（mode -> 回归目标）

模式清单来源：
- usage 文案：[examples/viewer_smoke_test.cpp#L4584](../../examples/viewer_smoke_test.cpp#L4584)
- 注册列表：[examples/viewer_smoke_test.cpp#L4787](../../examples/viewer_smoke_test.cpp#L4787)

### 5.1 render 类

- `viewer-render`（requiresLas=true）
  - 函数：[examples/viewer_smoke_test.cpp#L580](../../examples/viewer_smoke_test.cpp#L580)
  - 回归点：加载成功、`grabFramebuffer()` 非空、可见像素判定、点击后仍可渲染、orbit 拖拽与“反转拖拽”映射正确。

### 5.2 ui 类

- `main-backstage`（false）
  - 函数：[examples/viewer_smoke_test.cpp#L748](../../examples/viewer_smoke_test.cpp#L748)
  - 回归点：主窗口大集成（Dock/Ribbon/导航设置/量测/净空/航线/杆塔/隐患/Backstage），并验证大量 UI->viewer 反向同步。
- `main-settings-restore`（false）
  - 函数：[examples/viewer_smoke_test.cpp#L1903](../../examples/viewer_smoke_test.cpp#L1903)
  - 回归点：使用 `QTemporaryDir + QSettings::setPath` 做隔离，验证窗口与交互设置的持久化和重启恢复。
- `log-panel`（false）
  - 函数：[examples/viewer_smoke_test.cpp#L2359](../../examples/viewer_smoke_test.cpp#L2359)
  - 回归点：日志总量、过滤、关键字搜索、logger 清空后 UI 联动。
- `project-explorer-dock`（false）
  - 函数：[examples/viewer_smoke_test.cpp#L2410](../../examples/viewer_smoke_test.cpp#L2410)
  - 回归点：Dock 基础构件可用性（toolbar/tree/search）。
- `project-explorer-controller`（false）
  - 函数：[examples/viewer_smoke_test.cpp#L2442](../../examples/viewer_smoke_test.cpp#L2442)
  - 回归点：controller 信号转发、过滤、展开折叠与 action wiring。
- `project-explorer-mainwindow`（true）
  - 函数：[examples/viewer_smoke_test.cpp#L2535](../../examples/viewer_smoke_test.cpp#L2535)
  - 回归点：真实加载后项目树项（点云/图像/轨迹）构建、过滤、勾选显隐、上下文菜单、双击定位联动。
- `visualization-panel-controller`（false）
  - 函数：[examples/viewer_smoke_test.cpp#L2897](../../examples/viewer_smoke_test.cpp#L2897)
  - 回归点：可视化参数控件与 viewer 参数对象同步（点大小/透明度/背景/着色模式）。
- `measurement-analysis-controller`（true）
  - 函数：[examples/viewer_smoke_test.cpp#L3041](../../examples/viewer_smoke_test.cpp#L3041)
  - 回归点：量测开关不破坏航线可见性，净空与植被风险相关 action/callback/表格选择链路正确。
- `profile-classification-widget`（false）
  - 函数：[examples/viewer_smoke_test.cpp#L2782](../../examples/viewer_smoke_test.cpp#L2782)
  - 回归点：剖面分类 widget 的基础 UI 契约（标题、模式、源/目标列表）。
- `profile-classification-controller`（true）
  - 函数：[examples/viewer_smoke_test.cpp#L2809](../../examples/viewer_smoke_test.cpp#L2809)
  - 回归点：LAS 加载后分类项初始化、全选/清空/目标类别/选择模式到 viewer 的同步。
- `route-controller`（false）
  - 函数：[examples/viewer_smoke_test.cpp#L3272](../../examples/viewer_smoke_test.cpp#L3272)
  - 回归点：航线 controller 对 action/button/spinbox/combobox 的回调桥接正确。
- `tower-controller`（false）
  - 函数：[examples/viewer_smoke_test.cpp#L3484](../../examples/viewer_smoke_test.cpp#L3484)
  - 回归点：杆塔编辑相关 action、表格列显隐、选择与详情提交回调链。
- `issue-controller`（false）
  - 函数：[examples/viewer_smoke_test.cpp#L3682](../../examples/viewer_smoke_test.cpp#L3682)
  - 回归点：隐患编辑/导出 action、表格选择与详情提交回调链。

### 5.3 route 类

- `route-json`（false）
  - 函数：[examples/viewer_smoke_test.cpp#L4095](../../examples/viewer_smoke_test.cpp#L4095)
  - 回归点：模板航线导入、roundtrip 导出再导入形状一致性、辅助航点字段保真。
- `route-interop`（false）
  - 函数：[examples/viewer_smoke_test.cpp#L4183](../../examples/viewer_smoke_test.cpp#L4183)
  - 回归点：CRS 解析、风险点生成航线、桥接结构转换、KML/KMZ 导出导入完整性。
- `route-roam`（true）
  - 函数：[examples/viewer_smoke_test.cpp#L3826](../../examples/viewer_smoke_test.cpp#L3826)
  - 回归点：漫游状态机（start/pause/resume/stop）、速度上下限、隐藏航线/清空航点/清空点云后的自动停机。

### 5.4 tower 类

- `tower-file`（false）
  - 函数：[examples/viewer_smoke_test.cpp#L4364](../../examples/viewer_smoke_test.cpp#L4364)
  - 回归点：LiTower 文件导入导出格式、字段与浮点精度保持。
- `tower-project-link`（false）
  - 函数：[examples/viewer_smoke_test.cpp#L4447](../../examples/viewer_smoke_test.cpp#L4447)
  - 回归点：工程 JSON 中 tower linked file 的路径解析、索引归一化与文件回写一致性。

## 6. `all` 模式执行顺序（即注册顺序）

当 `--mode all` 或无 mode/category（默认全选）时，执行顺序固定为：

1. viewer-render
2. main-backstage
3. main-settings-restore
4. log-panel
5. project-explorer-dock
6. project-explorer-controller
7. project-explorer-mainwindow
8. visualization-panel-controller
9. measurement-analysis-controller
10. profile-classification-widget
11. profile-classification-controller
12. route-controller
13. tower-controller
14. issue-controller
15. route-json
16. route-interop
17. route-roam
18. tower-file
19. tower-project-link

对应注册代码：
- [examples/viewer_smoke_test.cpp#L4787](../../examples/viewer_smoke_test.cpp#L4787)

## 7. 当前回归盲区与风险（基于现有实现）

- 强依赖默认测试数据路径（若未显式 `--las`）：
  - [examples/viewer_smoke_test.cpp#L4604](../../examples/viewer_smoke_test.cpp#L4604)
- `pumpEvents(固定毫秒)` 依赖机器性能，慢机可能触发偶发失败：
  - [examples/viewer_smoke_test.cpp#L97](../../examples/viewer_smoke_test.cpp#L97)
- render 检查主要依靠 framebuffer 可见像素，属于有效但较轻量的图像启发式。
- 目前 smoke 重点在 GUI wiring/状态机/导入导出链路，不是算法精度回归基准（例如复杂净空算法精度、超大数据压力）。

## 8. 新增 smoke mode 的落地规范

新增 mode 时应同步修改：

1. 新增 `runXxxSmoke(const QStringList&)` 实现。
2. 在 `smokeCases` 中注册 mode/category/requiresLas/run。
   - [examples/viewer_smoke_test.cpp#L4787](../../examples/viewer_smoke_test.cpp#L4787)
3. 在 `validateSelections()` 的 `validModes` 中加入新 mode。
   - [examples/viewer_smoke_test.cpp#L4625](../../examples/viewer_smoke_test.cpp#L4625)
4. 更新 `printUsageSummary()` 的 mode 列表与示例。
   - [examples/viewer_smoke_test.cpp#L4584](../../examples/viewer_smoke_test.cpp#L4584)

这样才能保证 CLI 可见、参数合法、all 模式可执行。
