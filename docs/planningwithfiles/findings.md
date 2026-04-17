# 重构回归排查发现

## 2026-04-17 初始发现

### 1. 重构规模非常大，天然适合产生“链路断开型”回归

- 提交：`b9a8e3b 重构 MainWindow 实现边界并同步文档`
- 统计：
  - 18 个文件变更
  - 4970 行新增
  - 6075 行删除
  - 新增多个 `MainWindow.*.cpp` 拆分单元

结论：
- 这不是局部重排，而是一次大规模职责搬迁。
- 高风险不是“代码不存在”，而是“代码存在但接线/初始化/恢复逻辑漏迁移”。

### 2. 已确认一个典型漏迁移模式

问题：
- `Project Explorer` 右键菜单失效

直接原因：
- `ProjectExplorerController` 仍会发出：
  - `searchTextChanged`
  - `itemChanged`
  - `currentItemChanged`
  - `itemDoubleClicked`
  - `customContextMenuRequested`
- 但 `MainWindow.Connections.cpp` 中原本缺少这些 controller -> MainWindow 的接线。

结论：
- 当前 bug 不是控件样式或 Qt 本身问题，而是“重构后的接线遗漏”。
- 同类问题很可能还存在于其他 controller / dock / QAction 集成路径上。

### 3. `MainWindow.Connections.cpp` 是当前第一风险点

架构文档明确说明：
- `MainWindow.Connections.cpp`
  - 负责 viewer、dock、controller、动作之间的信号槽连接

推断：
- 任何“能显示但操作不生效”的问题，都应优先检查这个文件。
- 特别是把旧 `MainWindow.cpp` 直接 widget 连接改造成 controller 转发之后，最容易发生二段接线缺失。

### 4. 已发现的近期回归具有明显家族特征

- 标题区双击/拖拽/窗口行为异常
- 全屏点击闪屏
- 右侧 dock 过宽且不可继续缩小
- 项目树右键菜单失效

共同点：
- 都不是业务算法错误
- 都是 UI 集成、窗口状态、布局约束、信号接线、状态恢复链路问题

结论：
- 应按“交互/布局/状态恢复/接线”几个家族系统排查，而不是按具体功能点零散修。

### 5. 现有 smoke 覆盖不足以捕捉主窗口集成回归

当前已有：
- `main-backstage`
- `viewer-render`
- `project-explorer-controller`

不足：
- `project-explorer-controller` 只验证 controller 单体，不验证 controller 是否已接到 `MainWindow`
- 最近已为 dock 宽度补过主窗口级 smoke，这证明“集成 smoke”是有效的

结论：
- 后续 smoke 重点要从“组件单测式 smoke”补到“MainWindow 集成式 smoke”

### 6. 需要优先审计的风险面

- `MainWindow.Connections.cpp`
- `MainWindow.SettingsStore.cpp`
- `MainWindow.Core.cpp`
- `MainWindow.Docks.cpp`
- `ProjectExplorerController.*`
- `RouteController.*`
- `TowerController.*`
- `IssueController.*`

## 2026-04-17 静态审计补充

### 7. controller 风险不是平均分布的

- 高风险二段接线型：
  - `ProjectExplorerController`
  - `ProfileClassificationController`
- 相对低风险构造期回调注入型：
  - `RouteController`
  - `TowerController`
  - `IssueController`
  - `MeasurementAnalysisController`

结论：
- 当前最需要优先做“旧 `connect(...)` 对等性核对”的，不是所有 controller，而是显式 `signals:` 暴露后还要靠 `MainWindow.Connections.cpp` 二次接收的这类控制器。

### 8. `saveState()` 主前提基本满足，风险重心更偏向触发链路

已确认以下 dock 具备稳定 `objectName`：
- `projectExplorerDock`
- `sceneInspectorDock`
- `routeDetailsDock`
- `profileClassificationDock`
- `spanProfileDock`
- `applicationLogDock`

结论：
- 目前没有再发现“主 dock 因缺 `objectName` 导致 `restoreState()` 先天失效”的新证据。
- `MainWindow.SettingsStore.cpp` 的主风险更像是“恢复后补偿动作 / 持久化触发时机”而不是“dock 根本不参与状态保存”。

### 9. 已确认第二个明确漏迁移案例：`showProfileDockAction_`

问题：
- `Profile View` 动作存在，`profileDock_` 的可见性回写也存在，但动作本身缺少 `show/hide` 接线。

直接证据：
- 重构前 `MainWindow.cpp` 存在：
  - `connect(showProfileDockAction_, &QAction::toggled, ...)`
- 当前拆分后的 `MainWindow.Connections.cpp` 中只保留了：
  - `profileDock_ -> visibilityChanged -> showProfileDockAction_` 的回写
- 但丢失了：
  - `showProfileDockAction_ -> profileDock_` 的正向接线

结论：
- 这是标准的“动作仍在、状态同步还在、主触发链路丢了”的重构回归。
- 这进一步验证了本轮审计方法是有效的：以重构前 `connect(...)` 为基线，能直接抓出 UI 集成漏迁移。

修复状态：
- 已在 `src/gui/MainWindow.Connections.cpp` 补回 `showProfileDockAction_ -> profileDock_` 接线。
- 已在 `examples/viewer_smoke_test.cpp` 的 `main-backstage` smoke 中补充：
  - `showProfileDockAction_` 显示 `SpanProfileDock`
  - `showProfileDockAction_` 隐藏 `SpanProfileDock`

验证结果：
- `cmake --build out/build --config Release --target LASPointCloudViewer LASViewerSmokeTest -- /p:PostBuildEventUseInBuild=false`
- `.\out\build\bin\Release\LASViewerSmokeTest.exe --mode main-backstage`
- 结果：通过

### 10. 已确认第三个、且更高风险的漏迁移家族：`viewer_ -> MainWindow` 主链路整体缺失

问题：
- 当前 `src/gui/MainWindow.Connections.cpp` 的 `createWindowAndViewerConnections()` 在 dock / tab 的持久化接线后就结束了。
- 重构前 `MainWindow.cpp` 中那批 `viewer_ -> MainWindow` 的集成接线没有迁移过来。

直接影响面：
- 点云加载/清空后的 UI 同步
- 项目树重建
- 量测面板和 profile dock 状态同步
- 杆塔/隐患面板与选择同步
- 航线 waypoint 选择、双击编辑、拖拽回写
- 导航偏好修改后的 UI 反馈

已补回的连接包括：
- route：
  - `selectedInspectionRouteWaypointChanged`
  - `inspectionRouteWaypointDoubleClicked`
  - `inspectionRouteWaypointDragFinished`
- point cloud / viewer state：
  - `pointCloudLoadingStarted`
  - `pointCloudLoadingProgress`
  - `pointCloudLoadingFinished`
  - `pointCloudLoaded`
  - `pointCloudCleared`
  - `visualizationOptionsChanged`
  - `interactionOptionsChanged`
  - `measurementChanged`
  - `measurementModeChanged`
- tower / issue：
  - `towerMarkersChanged`
  - `selectedTowerChanged`
  - `towerEditModeChanged`
  - `towerEditRequested`
  - `inspectionIssuesChanged`
  - `selectedIssueChanged`
  - `issueEditModeChanged`
  - `issueEditRequested`
  - `measurementMessage`

结论：
- 这不是单个遗漏，而是一次完整的 `viewer` 集成段在重构时被截断。
- 风险等级高于前两个单点回归，因为它会导致“底层 viewer 还能工作，但 MainWindow 面板和状态完全不同步”。

修复状态：
- 已在 `src/gui/MainWindow.Connections.cpp` 补回上述 `viewer_ -> MainWindow` 连接。
- 已扩展 `examples/viewer_smoke_test.cpp` 的 `main-backstage` smoke：
  - 通过嵌入的 `PointCloudViewer` 加载测试 LAS
  - 验证 `Project Explorer` 会重建并出现点云数据项
  - 验证清空点云后项目树中的数据项会被移除，但树结构仍保留

验证结果：
- `cmake --build out/build --config Release --target LASPointCloudViewer LASViewerSmokeTest -- /p:PostBuildEventUseInBuild=false`
- `.\out\build\bin\Release\LASViewerSmokeTest.exe --mode main-backstage --las .\test_data\ezhou_powerline_sample.las`
- 结果：通过

### 11. 已确认第四个漏迁移家族：主窗口剩余直连 widget 事件整批缺失

问题：
- 在补回 `viewer_ -> MainWindow` 主链路后，继续用重构前 `MainWindow.cpp` 与当前拆分文件做机械化对比，剩余缺口集中在“直接由 widget 驱动主窗口副作用”的一整批连接。
- 这些连接在 controller 层没有替代实现；当前代码里只有控件创建，没有 `connect(...)`。

直接影响面：
- route 面板：
  - waypoint / part 列显隐复选框不生效
  - waypoint / part / trajectory 颜色按钮不生效
  - route 表格的选中、双击、右键菜单不生效
  - target / QA 表格选中不同步
- rendering / navigation / classification：
  - classification mapping 表的可见性、名称编辑、颜色双击和 reset 不生效
  - `round splats` / `axes` / `bounding box` 复选框不驱动 viewer
  - 导航反转选项和滚轮缩放灵敏度 slider 不驱动 viewer

已补回的连接包括：
- route：
  - `routeWaypointShowCoordinatesCheckBox_`
  - `routeWaypointShowCaptureAnglesCheckBox_`
  - `routePartShowCoordinatesCheckBox_`
  - `routePartShowCaptureAnglesCheckBox_`
  - `routeWaypointColorButton_`
  - `routePartPointColorButton_`
  - `routeTrajectoryColorButton_`
  - `routePartPointsTableWidget_`
  - `routeWaypointsTableWidget_`
  - `routeWaypointTargetsTableWidget_`
  - `routeQaIssuesTableWidget_`
- rendering / navigation / classification：
  - `resetClassificationColorsButton_`
  - `classificationColorsTableWidget_`
  - `roundSplatsCheckBox_`
  - `axesCheckBox_`
  - `boundingBoxCheckBox_`
  - `invertOrbitCheckBox_`
  - `invertPanCheckBox_`
  - `invertWheelCheckBox_`
  - `wheelZoomSensitivitySlider_`

补充修复：
- `MainWindow.h` 中此前已声明、但未在拆分后落实现的 3 个 route 颜色方法也一并补回：
  - `chooseRouteWaypointColor()`
  - `chooseRoutePartPointColor()`
  - `chooseRouteTrajectoryColor()`

结论：
- 当前 `MainWindow` 重构的风险不是零散漏点，而是“先丢整段 `viewer` 集成，再丢整批直连 widget 副作用”的连续性迁移缺口。
- 这类问题仅靠 controller 单体 smoke 很难发现，必须由 MainWindow 集成 smoke 兜底。

修复状态：
- 已在 `src/gui/MainWindow.Connections.cpp` 补回这批直连 widget 接线。
- 已在 `src/gui/MainWindow.Route.cpp` 补回 route 颜色选择方法实现。
- 已扩展 `examples/viewer_smoke_test.cpp` 的 `main-backstage` smoke，新增覆盖：
  - navigation settings widget -> viewer 交互选项同步
  - classification mapping 表的可见性 / 名称编辑 / reset
  - route waypoint / part 列显隐复选框 -> 表格列隐藏

验证结果：
- `cmake --build out/build --config Release --target LASPointCloudViewer LASViewerSmokeTest -- /p:PostBuildEventUseInBuild=false`
- `.\out\build\bin\Release\LASViewerSmokeTest.exe --mode main-backstage --las .\test_data\ezhou_powerline_sample.las`
- 结果：通过

## 后续排查假设

### 假设 A
- 重构前 `MainWindow.cpp` 中的旧 `connect(...)` 仍有一部分没有迁移到新的 `MainWindow.Connections.cpp`

### 假设 B
- 一些 `QDockWidget` / page / toolbar 缺少 `objectName`，导致 `saveState()` / `restoreState()` 只部分生效

### 假设 C
- 一些原本直接由 widget 驱动的副作用逻辑，如：
  - `updateActionState()`
  - `updateIssuePanel()`
  - `updateRoutePlanningPanel()`
  - 选中同步到 `viewer`
  - `retranslateUi()`
  在拆分后只迁移了 UI 构造，没有迁移触发点

### 假设 D
- 现有 smoke 只覆盖“类本身还活着”，没有覆盖“主窗口整条交互链路还活着”

## 2026-04-17 设置恢复链路补充发现

### 12. dock 可见性持久化不能直接依赖 `isVisible()`

问题：
- 新增的 `main-settings-restore` smoke 证明，第一窗口关闭后：
  - `window/showLog`
  - `window/showProfileClassification`
  会被错误写回 `false`
- 直接原因不是 `restoreState()` 失效，而是关闭流程里的临时可见性变化污染了持久化结果。

结论：
- 对 dock 显隐偏好做持久化时，不能把“关窗瞬间是否 still visible”当成用户意图。
- 需要在 `QEvent::Close` 进入时就切换到关闭态，并在 `closeEvent()` 中只做一次强制保存；关闭过程中由 dock 发出的 `visibilityChanged(false)` 不应继续反向修改 action / settings。

修复状态：
- 已在 `MainWindow` 中新增关闭态门禁：
  - `event(QEvent*)` 提前标记 `closingWindow_`
  - `closeEvent()` 改为 `persistWindowSettings(true)` 单次强制保存
  - `persistWindowSettings()` 在关闭态下默认拒绝二次持久化
- `persistWindowSettings()` 中 dock 可见性偏好已改为优先使用 action 勾选态，避免单纯依赖 `isVisible()`
- `log/profile/profileClassification/routeDetails` 的 dock `visibilityChanged` 在关闭态下不再回写设置

### 13. `showProfileClassificationDockAction_` 的正向链路仍不完整

问题：
- `showProfileClassificationDockAction_` 原先只做 `setVisible(true)`，但 `profileClassificationDock_` 与 `projectDock_` 是 tabify 关系。
- 仅 `show()` 不 `raise()` 时，dock 可能仍停留在后台 tab，`updateActionState()` 又会按 `dock->isVisible()` 把 action 回写成 `false`。

结论：
- 这是一个真实主程序回归，不是 smoke 误报。
- 对 tabified dock，动作驱动显示时必须同时 `raise()` 才能保证前台可见和状态一致。

修复状态：
- 已在 `MainWindow.Connections.cpp` 中将：
  - `showProfileClassificationDockAction_`
  - `showProfileDockAction_`
  的正向接线统一改为 `show()/raise()` 与显式 `hide()`

### 14. route roam 主面板控件在重构后漏接到 `viewer_`

问题：
- `routeRoamSpeedSpinBox_` / `routeRoamViewModeComboBox_` 在主面板里改值后，没有同步回 `viewer_`
- 持久化逻辑优先保存 `viewer_` 中的 roam 参数，因此主面板改动不会真正落盘

直接影响：
- `main-settings-restore` 中：
  - roam speed
  - roam view mode
  无法正确恢复

修复状态：
- 已在 `MainWindow.Connections.cpp` 补回：
  - 主面板 roam speed -> `viewer_->setInspectionRouteRoamSpeedMetersPerSecond()`
  - 主面板 roam view mode -> `viewer_->setInspectionRouteRoamViewMode()`
- 新接线已补关闭态门禁，避免窗口析构阶段再次回写 `viewer_` 导致访问违例

### 15. `TowerController` / `IssueController` 主窗口集成整段缺失

问题：
- `TowerController` 与 `IssueController` 类本身仍在，controller 单体 smoke 也可跑。
- 但拆分后的 `MainWindow.Connections.cpp` 中，这两组 controller 的构造与回调注入整段丢失。
- 结果是：
  - tower/issue 的 QAction 仍存在、按钮也能显示
  - `viewer_ -> MainWindow` 的回流同步也还在
  - 但主窗口正向链路缺失，导致开始编辑、添加/插入/移动、表格选中、详情编辑、清空/删除、导出等入口没有真正接入

直接证据：
- 当前代码中：
  - `MainWindow.h` 仍保留 `towerController_` / `issueController_`
  - `TowerController.cpp` / `IssueController.cpp` 仍保留完整封装
  - 但 `MainWindow.Connections.cpp` 里此前没有 `new TowerController(...)` / `new IssueController(...)`

修复状态：
- 已把重构前 `MainWindow.cpp` 中 tower/issue controller 的构造与回调逻辑按原语义迁回 `src/gui/MainWindow.Connections.cpp`
- 已恢复的主窗口能力包括：
  - tower：
    - 开始/结束编辑
    - add / insert / move / cancel
    - focus / remove / clear
    - 导入/保存/另存/重载 tower 文件
    - X/Y/Z 列显隐
    - 表格选中 -> viewer，同步回流 -> 表格
    - 表格改名与详情编辑器提交
  - issue：
    - start / cancel
    - focus / remove / clear
    - issues CSV / inspection report 导出
    - 表格选中 -> viewer，同步回流 -> 表格
    - 详情编辑器提交

验证结果：
- 已扩展 `examples/viewer_smoke_test.cpp` 的 `main-backstage`，新增 MainWindow 集成级断言，覆盖：
  - tower 表格填充、选中双向同步、名称编辑、详情提交、列显隐、模式切换、删除与清空
  - issue 表格填充、选中双向同步、详情提交、模式切换、删除与清空
- 已完成验证：
  - `cmake --build out/build --config Release --target LASPointCloudViewer LASViewerSmokeTest -- /p:PostBuildEventUseInBuild=false`
  - `.\out\build\bin\Release\LASViewerSmokeTest.exe --mode main-backstage --las .\test_data\ezhou_powerline_sample.las`
  - `.\out\build\bin\Release\LASViewerSmokeTest.exe --mode main-settings-restore`
  - `.\out\build\bin\Release\LASViewerSmokeTest.exe --mode tower-controller`
  - `.\out\build\bin\Release\LASViewerSmokeTest.exe --mode issue-controller`
  - 结果：全部通过

### 16. `MeasurementAnalysisController` / `RouteController` 主窗口集成也存在整段漏迁移

问题：
- `MeasurementAnalysisController` 与 `RouteController` 类和单体 smoke 仍然存在。
- 但拆分后的 `src/gui/MainWindow.Connections.cpp` 此前没有：
  - `new MeasurementAnalysisController(...)`
  - `new RouteController(...)`
- 导致测量/净空/植被风险/航线规划/航线漫游在主窗口里出现“控件还在、部分表格接线也在，但主入口动作和控制器封装未真正接入”的半断链状态。

直接影响面：
- measurement / vegetation risk：
  - `measureAction_`
  - `clearMeasurementAction_`
  - `exportClearanceCsvAction_`
  - `analyzeVegetationRisksAction_`
  - `focusVegetationRiskAction_`
  - `createIssueFromRiskAction_`
  - `createIssuesFromRisksAction_`
  - `clearVegetationRisksAction_`
  - clearance / vegetation 参数控件和两张表
- route：
  - `generateInspectionRouteAction_`
  - `regenerateInspectionRouteAction_`
  - `clearInspectionRouteAction_`
  - `toggleRouteEditingAction_`
  - `start/pause/stopInspectionRouteRoamAction_`
  - `focusRouteWaypointAction_`
  - route 文件 / KML / DJI KMZ 导入导出
  - roam 三按钮与 speed / view mode

修复状态：
- 已把重构前 `MainWindow.cpp` 中 measurement / route controller 的构造和回调逻辑按现有拆分结构迁回 `src/gui/MainWindow.Connections.cpp`
- 已恢复：
  - 测量模式开关与 profile dock 同步
  - clearance CSV 导出
  - vegetation risk 分析 / 聚焦 / 生成 issue / 清空
  - 测量与 vegetation 参数变更的持久化与面板刷新
  - 航线生成 / 清空 / 编辑锁 / focus
  - 航线 roam start / pause / resume / stop
  - route JSON / KML / DJI KMZ 导入导出
  - route roam speed / view mode 重新纳入 `RouteController`
- 已删除此前为临时补洞加在 `createWindowAndViewerConnections()` 中、会与恢复后的 `RouteController` 重复的 roam speed / view mode 直连，避免重复触发。

验证结果：
- 已扩展 `examples/viewer_smoke_test.cpp` 的 `main-backstage`，新增 MainWindow 集成级验证，覆盖：
  - measurement action -> viewer measurement mode
  - clear measurement action
  - clearance threshold / preset -> MainWindow 状态
  - vegetation risk 表选择 -> `selectedVegetationRiskIndex_`
  - create issue from risk
  - clear vegetation risks
  - generate route / clear route
  - route editing action
  - route roam speed / view mode -> viewer
  - route roam start / pause / resume / stop
- 已完成验证：
  - `cmake --build out/build --config Release --target LASPointCloudViewer -- /p:PostBuildEventUseInBuild=false`
  - `cmake --build out/build --config Release --target LASViewerSmokeTest -- /p:PostBuildEventUseInBuild=false`
  - `.\out\build\bin\Release\LASViewerSmokeTest.exe --mode main-backstage --las .\test_data\ezhou_powerline_sample.las`
  - `.\out\build\bin\Release\LASViewerSmokeTest.exe --mode main-settings-restore`
  - `.\out\build\bin\Release\LASViewerSmokeTest.exe --mode measurement-analysis-controller --las .\test_data\ezhou_powerline_sample.las`
  - `.\out\build\bin\Release\LASViewerSmokeTest.exe --mode route-controller`
  - 结果：全部通过

遗留问题：
- 运行 `main-backstage` / `main-settings-restore` 时，`QString::arg: Argument missing ...` 警告仍然存在。
- 该问题与本轮 controller 漏迁移修复无直接冲突，仍应作为下一批单独处理。

### 17. `QString::arg: Argument missing ...` 的主因是中文翻译占位符丢失，首次复测还叠加了运行目录 `.qm` 陈旧

问题：
- `main-backstage` / `main-settings-restore` 反复输出两类告警：
  - `QString::arg: Argument missing: 工程坐标系已更新。, ...`
  - `QString::arg: Argument missing: 未设置 -> EPSG:4,326, ...`
- 追查后确认，主因不在 controller 集成，而在 `translations/lasviewer_zh_CN.ts` 的两条翻译被错误改坏：
  - `src/gui/MainWindow.Helpers.cpp` 对应的 `%1 -> %2` 被翻成了不带占位符的 `工程坐标系已更新。`
  - `src/gui/MainWindow.Route.cpp` 对应的 8 占位符航线摘要，被翻成了只剩 `%1 -> %2`

补充发现：
- 首次修完 `.ts` 后，重新构建虽然已生成新的 `out/build/translations/lasviewer_zh_CN.qm`，但由于本地验证命令使用了 `/p:PostBuildEventUseInBuild=false`，运行时实际读取的仍是旧的：
  - `out/build/bin/Release/translations/lasviewer_zh_CN.qm`
- 也就是说，第一次复测“看起来没修好”，真实原因是部署目录里的 `.qm` 没同步，不是代码修复失败。

修复状态：
- 已修正 `translations/lasviewer_zh_CN.ts` 中两条翻译，恢复完整占位符：
  - `%1 -> %2`
  - `%1 -> %2 | DJI 机型：%3 | 安全高度 %4 米 | 速度 %5 米/秒 | 航点间距 %6 米 | 平滑 %7%% | 高程偏移 %8 米`
- 已将新生成的 `out/build/translations/lasviewer_zh_CN.qm` 同步到 `out/build/bin/Release/translations/lasviewer_zh_CN.qm`，使本地 smoke 运行时加载到最新翻译。

验证结果：
- 已完成验证：
  - `cmake --build out/build --config Release --target LASPointCloudViewer LASViewerSmokeTest -- /p:PostBuildEventUseInBuild=false`
  - `Copy-Item "out/build/translations/lasviewer_zh_CN.qm" "out/build/bin/Release/translations/lasviewer_zh_CN.qm" -Force`
  - `.\out\build\bin\Release\LASViewerSmokeTest.exe --mode main-backstage --las .\test_data\ezhou_powerline_sample.las`
  - `.\out\build\bin\Release\LASViewerSmokeTest.exe --mode main-settings-restore`
- 结果：
  - 两个 smoke 都通过
  - `QString::arg: Argument missing ...` 告警已消失
  - 当前仅剩 `libpng warning: iCCP: cHRM chunk does not match sRGB`，与本轮 Qt 字符串格式化问题无关

### 18. 审查 `Phase 2-5` 后，唯一仍值得补的自动化缺口是 `Project Explorer` 主窗口级 smoke

问题：
- 之前虽然已有：
  - `project-explorer-dock`
  - `project-explorer-controller`
  - `main-backstage`
- 但仍缺一个专门覆盖 `Project Explorer -> MainWindow` 集成链的 smoke。
- 现有 `main-backstage` 只验证了项目树会出现点云项和清空后结构保留，没有单独覆盖：
  - 搜索过滤
  - `currentItemChanged`
  - `itemChanged`
  - `itemDoubleClicked`
  - `customContextMenuRequested`

结论：
- 继续无差别扩展更多 smoke 的收益已经不高。
- 这一个缺口补上后，`Phase 2-5` 就没有新的同等级 actionable gap 了。

修复状态：
- 已在 `examples/viewer_smoke_test.cpp` 新增独立 mode：
  - `project-explorer-mainwindow`
- 新 smoke 直接走 `MainWindow` 集成路径，覆盖：
  - 加载 LAS 后项目树重建
  - 带图片的 issue 进入项目树
  - 通过 `generateInspectionRouteAction_` 生成 route，并让 trajectory item 进入项目树
  - 搜索过滤隐藏当前项时清空选择
  - 选择 image / trajectory / pointCloud item 时与 `viewer_` 的同步
  - 勾选状态到 dataset / issue / route 可见性的同步
  - 右键菜单打开前当前项更新
  - image item 双击链路触发聚焦/选中同步

验证结果：
- 已重新编译主程序与 smoke：
  - `cmake --build out/build --config Release --target LASPointCloudViewer LASViewerSmokeTest -- /p:PostBuildEventUseInBuild=false`
- 已完成验证：
  - `.\out\build\bin\Release\LASViewerSmokeTest.exe --mode project-explorer-mainwindow --las .\test_data\ezhou_powerline_sample.las`
  - `.\out\build\bin\Release\LASViewerSmokeTest.exe --mode main-backstage --las .\test_data\ezhou_powerline_sample.las`
  - `.\out\build\bin\Release\LASViewerSmokeTest.exe --mode main-settings-restore`
- 结果：
  - 三个 smoke 全部通过
  - 当前运行期残留告警仍只有 `libpng warning: iCCP: cHRM chunk does not match sRGB`
