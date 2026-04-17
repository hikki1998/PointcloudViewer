# 进度记录

## 2026-04-17

### 会话目标
- 针对重构后可能存在的大量类似回归，制定一份可持续执行的深度排查与修复计划。

### 已完成
- 读取并恢复上下文：
  - `docs/agent/context.md`
  - `docs/agent/session-handoff.md`
  - `docs/agent/architecture.md`
  - `docs/agent/workflows.md`
- 检查当前工作区差异：
  - `.gitignore`
  - `AGENTS.md`
  - `src/gui/MainWindow.Connections.cpp`
- 检查重构提交范围：
  - `b9a8e3b 重构 MainWindow 实现边界并同步文档`
- 确认了一个可复用的回归模式：
  - `ProjectExplorerController` 信号存在，但 `MainWindow.Connections.cpp` 漏接导致主窗口功能失效
- 创建规划文件：
  - `task_plan.md`
  - `findings.md`
  - `progress.md`
- 开始 Phase 2 静态对等性审计：
  - 对比重构前 `MainWindow.cpp` 与当前 `MainWindow.Connections.cpp`
  - 检查 `MainWindow.SettingsStore.cpp` 的 `saveState()` / `restoreState()` 链路
  - 检查各主 dock 的 `objectName`
- 已形成一轮新的审计结论：
  - controller 风险集中在 `ProjectExplorerController`、`ProfileClassificationController`
  - 主 dock 的 `objectName` 前提基本满足
  - 又发现一个明确漏迁移项：`showProfileDockAction_ -> profileDock_` 接线缺失
- 已完成本轮首个“审计后立刻修复”闭环：
  - 修复 `src/gui/MainWindow.Connections.cpp` 中缺失的 `showProfileDockAction_` 接线
  - 在 `src/gui/MainWindow.Actions.cpp` 为该动作补稳定 `objectName`
  - 扩展 `examples/viewer_smoke_test.cpp` 的 `main-backstage`，覆盖 profile dock 显隐链路
- 已完成验证：
  - 主程序与 smoke 均重新编译通过
  - `LASViewerSmokeTest --mode main-backstage` 通过

### 当前结论
- 本项目当前最值得系统审计的不是单个功能模块，而是 `MainWindow` 重构后的接线、状态恢复、窗口交互和 smoke 覆盖完整性。
- 静态审计已经证明：当前回归不只是 controller 二段接线，还包含 `QAction -> dock` 这类主窗口动作链路漏迁移。

### 下一步建议
- 继续做 `MainWindow.Connections.cpp` 的旧新对等清单
- 重点核查 route 表格 / inspector 直连 widget 事件是否还有漏迁移
- 然后继续补主窗口级 smoke，而不是只补 controller 单体 smoke

### 本轮新增进展
- 用“重构前 `MainWindow.cpp` connect 列表 vs 当前拆分文件 connect 列表”做了机械化对比。
- 确认当前不是只有单个动作链路漏迁移，而是整段 `viewer_ -> MainWindow` 集成接线在 `createWindowAndViewerConnections()` 中丢失。
- 已在 `src/gui/MainWindow.Connections.cpp` 补回 route / point cloud / measurement / tower / issue 相关 `viewer` 接线。
- 已扩展 `examples/viewer_smoke_test.cpp` 的 `main-backstage`：
  - 通过 `PointCloudViewer` 加载测试 LAS
  - 验证 `Project Explorer` 会随 `pointCloudLoaded` 重建
  - 验证 `clearPointCloud()` 后项目树数据项被清空
- 已完成验证：
  - `cmake --build out/build --config Release --target LASPointCloudViewer LASViewerSmokeTest -- /p:PostBuildEventUseInBuild=false`
  - `.\out\build\bin\Release\LASViewerSmokeTest.exe --mode main-backstage --las .\test_data\ezhou_powerline_sample.las`
  - 结果：通过

### 本轮继续新增进展
- 已补回 `MainWindow` 剩余一整批直连 widget 接线，覆盖：
  - route 列显隐复选框
  - route 颜色按钮
  - route 各表格的选中 / 双击 / 右键菜单
  - classification mapping 表的 `itemChanged` / `cellDoubleClicked` / reset
  - rendering 三个复选框
  - navigation 三个反转复选框和滚轮灵敏度 slider
- 已补回 `src/gui/MainWindow.Route.cpp` 中缺失的 route 颜色方法实现：
  - `chooseRouteWaypointColor()`
  - `chooseRoutePartPointColor()`
  - `chooseRouteTrajectoryColor()`
- 已扩展 `examples/viewer_smoke_test.cpp` 的 `main-backstage`，新增 MainWindow 集成级断言：
  - `NavigationSettingsWidget` -> viewer `interactionOptions`
  - classification mapping 表的可见性、名称编辑和 reset
  - route waypoint / part 列显隐复选框 -> 表格列隐藏
- 已再次完成验证：
  - `cmake --build out/build --config Release --target LASPointCloudViewer LASViewerSmokeTest -- /p:PostBuildEventUseInBuild=false`
  - `.\out\build\bin\Release\LASViewerSmokeTest.exe --mode main-backstage --las .\test_data\ezhou_powerline_sample.las`
  - 结果：通过

### 本轮继续新增进展 2
- 已继续扩展 `examples/viewer_smoke_test.cpp` 的 `main-backstage`，补上 route 主窗口集成级回归守护，覆盖：
  - synthetic route JSON 导出 / 导入后，MainWindow route 数据装载
  - waypoint / part / target / QA 四张表的填充
  - `currentCellChanged` 驱动的 route 选中同步
  - waypoint / part 右键菜单打开前的当前行更新
  - waypoint / part / QA 双击链路可达
  - waypoint / part point / trajectory 三个颜色按钮 -> `QColorDialog` -> viewer 颜色同步
- 为保证 smoke 稳定性，测试侧新增了最小 route display data 组装与非阻塞对话框/菜单辅助逻辑；未修改生产代码可见性，也未引入新的业务分支。
- 已完成 `MainWindow.SettingsStore.cpp` / `MainWindow.Core.cpp` 二次审计：
  - `saveState()` / `restoreState()` 依赖的主 dock `objectName` 已在各 dock 类中稳定设置，当前未发现新增缺口
  - `WM_NCHITTEST`、标题区双击最大化/还原、工作区最大化约束已被现有 `main-backstage` smoke 覆盖，本轮未发现新增窗口交互回归
  - `loadWindowSettings()` 中 `kWindowShowProfile` 未在 `!restoredState` 分支显式恢复仍是旧行为，不作为本轮新增问题
  - 目前仍存在一个覆盖空白：尚无单独的 QSettings 持久化/恢复端到端 smoke
- 已完成验证：
  - `cmake --build out/build --config Release --target LASViewerSmokeTest -- /p:PostBuildEventUseInBuild=false`
  - `.\out\build\bin\Release\LASViewerSmokeTest.exe --mode main-backstage --las .\test_data\ezhou_powerline_sample.las`
  - 结果：通过

### 本轮继续新增进展 3
- 已为 `examples/viewer_smoke_test.cpp` 新增 `main-settings-restore` 主窗口集成 smoke，覆盖：
  - `QSettings` 的 geometry / state 恢复
  - `logDock_` / `profileClassificationDock_` / `routeDetailsDock_` 显隐恢复
  - inspector / routeDetails tab 恢复
  - log filter / keyword / auto-scroll 恢复
  - route label mode / 列显隐偏好恢复
  - route roam speed / roam view mode 恢复，并验证同步回 `viewer_`
- 已修复 `src/gui/MainWindow.SettingsStore.cpp` 的关闭态持久化回归：
  - 关闭时不再让 dock `visibilityChanged(false)` 覆写 `window/showLog`、`window/showProfileClassification`、`window/showRouteDetails`
  - `persistWindowSettings()` 新增 `force` 入口，`closeEvent()` 改为单次强制保存
  - `persistWindowSettings()` 中 dock 显隐偏好改为优先读取 action 勾选态 / 非显式隐藏态
- 已修复 `src/gui/MainWindow.Connections.cpp` 的两类主程序问题：
  - `showProfileClassificationDockAction_` / `showProfileDockAction_` 显示时补 `raise()`
  - 主面板 `routeRoamSpeedSpinBox_` / `routeRoamViewModeComboBox_` 补回到 `viewer_` 的同步接线，并加关闭态门禁
- 已修复 `main-settings-restore` 自身的清理问题：
  - 第二个 `MainWindow` 显式 `close()`，避免依赖析构路径触发 Qt/OSG 清理时序导致访问违例
- 已完成验证：
  - `cmake --build out/build --config Release --target LASPointCloudViewer LASViewerSmokeTest -- /p:PostBuildEventUseInBuild=false`
  - `.\out\build\bin\Release\LASViewerSmokeTest.exe --mode main-settings-restore`
  - `.\out\build\bin\Release\LASViewerSmokeTest.exe --mode main-backstage --las .\test_data\ezhou_powerline_sample.las`
  - 结果：全部通过

### 本轮继续新增进展 4
- 已补回 `src/gui/MainWindow.Connections.cpp` 中整段缺失的 tower / issue 主窗口集成：
  - 重新实例化 `towerController_` / `issueController_`
  - 恢复重构前已存在的 tower / issue 回调语义
  - 保持 controller 封装，不在 `MainWindow` 中重复散落手写接线
- 已扩展 `examples/viewer_smoke_test.cpp` 的 `main-backstage`，新增 MainWindow 集成级验证：
  - tower：
    - controller 创建
    - 表格填充
    - 表格 <-> viewer 选中双向同步
    - 名称编辑与详情编辑提交
    - X/Y/Z 列显隐
    - add / insert / move / cancel 模式切换
    - remove / clear
  - issue：
    - controller 创建
    - 表格填充
    - 表格 <-> viewer 选中双向同步
    - 详情编辑提交
    - start / cancel 模式切换
    - remove / clear
- 已重新编译主程序与 smoke，并完成验证：
  - `cmake --build out/build --config Release --target LASPointCloudViewer LASViewerSmokeTest -- /p:PostBuildEventUseInBuild=false`
  - `.\out\build\bin\Release\LASViewerSmokeTest.exe --mode main-backstage --las .\test_data\ezhou_powerline_sample.las`
  - `.\out\build\bin\Release\LASViewerSmokeTest.exe --mode main-settings-restore`
  - `.\out\build\bin\Release\LASViewerSmokeTest.exe --mode tower-controller`
  - `.\out\build\bin\Release\LASViewerSmokeTest.exe --mode issue-controller`
  - 结果：全部通过

### 本轮继续新增进展 5
- 已补回 `src/gui/MainWindow.Connections.cpp` 中整段缺失的 measurement / route 主窗口集成：
  - 重新实例化 `measurementAnalysisController_` / `routeController_`
  - 恢复重构前已有的测量、净空、植被风险、航线规划、航线漫游回调语义
  - 把 route roam speed / view mode 重新收回 `RouteController`
  - 删除此前临时补洞、会与恢复后 controller 重复触发的 direct connect
- 已扩展 `examples/viewer_smoke_test.cpp` 的 `main-backstage`，新增 MainWindow 集成级验证：
  - measurement：
    - controller 创建
    - measurement action -> viewer
    - clear measurement action
    - clearance threshold / preset -> MainWindow 状态
  - vegetation risk：
    - 表选择 -> `selectedVegetationRiskIndex_`
    - create issue from risk
    - clear vegetation risks
  - route：
    - controller 创建
    - generate / clear route
    - route editing action
    - roam speed / view mode -> viewer
    - start / pause / resume / stop roam
- 已重新编译主程序与 smoke，并完成验证：
  - `cmake --build out/build --config Release --target LASPointCloudViewer -- /p:PostBuildEventUseInBuild=false`
  - `cmake --build out/build --config Release --target LASViewerSmokeTest -- /p:PostBuildEventUseInBuild=false`
  - `.\out\build\bin\Release\LASViewerSmokeTest.exe --mode main-backstage --las .\test_data\ezhou_powerline_sample.las`
  - `.\out\build\bin\Release\LASViewerSmokeTest.exe --mode main-settings-restore`
  - `.\out\build\bin\Release\LASViewerSmokeTest.exe --mode measurement-analysis-controller --las .\test_data\ezhou_powerline_sample.las`
  - `.\out\build\bin\Release\LASViewerSmokeTest.exe --mode route-controller`
  - 结果：全部通过

### 当前剩余问题
- controller 漏迁移主链路已基本收口。
- 下一阶段更适合转入回归收口、防复发清单和文档整理。

### 本轮继续新增进展 6
- 已修复 `QString::arg: Argument missing ...` 运行时告警：
  - 修正 `translations/lasviewer_zh_CN.ts` 中两条占位符丢失的中文翻译
  - 坐标系摘要恢复为 `%1 -> %2`
  - 航线摘要恢复为完整的 8 占位符版本
- 已确认本地首次复测仍报错的直接原因不是代码未生效，而是：
  - 使用 `/p:PostBuildEventUseInBuild=false` 构建时，只会更新 `out/build/translations/lasviewer_zh_CN.qm`
  - smoke 运行时读取的是 `out/build/bin/Release/translations/lasviewer_zh_CN.qm`
  - 旧 `.qm` 未部署时，会继续看到旧告警
- 已同步最新 `.qm` 到运行目录，并重新验证：
  - `cmake --build out/build --config Release --target LASPointCloudViewer LASViewerSmokeTest -- /p:PostBuildEventUseInBuild=false`
  - `Copy-Item "out/build/translations/lasviewer_zh_CN.qm" "out/build/bin/Release/translations/lasviewer_zh_CN.qm" -Force`
  - `.\out\build\bin\Release\LASViewerSmokeTest.exe --mode main-backstage --las .\test_data\ezhou_powerline_sample.las`
  - `.\out\build\bin\Release\LASViewerSmokeTest.exe --mode main-settings-restore`
  - 结果：全部通过，`QString::arg: Argument missing ...` 已消失

### 本轮继续新增进展 7
- 已完成 Phase 6 文档收口：
  - 新增 `docs/agent/refactor-regression-report.md`，汇总 `MainWindow` 拆分后的回归家族、修复结论、验证范围、残余风险和后续重点
  - 在 `docs/agent/workflows.md` 新增“MainWindow 重构后对等性检查”，沉淀接线、状态、交互、smoke、翻译部署的固定检查项
  - 在 `docs/agent/README.md` 和 `docs/agent/session-handoff.md` 增加入口，方便后续新会话直接接手
  - `task_plan.md` 中 Phase 6 已从 `pending` 更新为 `complete`
- 本轮仅为文档整理与防复发收口，未新增代码变更，因此未重复执行构建/ smoke

### 本轮继续新增进展 8
- 已按“只做仍有必要的内容”重新审查 `Phase 2-5`：
  - 确认此前主程序级修复和验证已覆盖大多数高风险链路
  - 唯一仍值得补的缺口是 `Project Explorer` 主窗口级 smoke
- 已在 `examples/viewer_smoke_test.cpp` 新增：
  - `project-explorer-mainwindow` mode
  - `QTreeWidget` 右键菜单关闭辅助
  - `QTreeWidget` 双击辅助
  - 项目树 item 查找 helper
- 新 smoke 已覆盖：
  - 搜索过滤
  - image / trajectory / pointCloud 的 `currentItemChanged`
  - 项目树勾选状态到 viewer 可见性的同步
  - 项目树右键菜单当前项切换
  - image item 双击链路
- 已重新编译主程序与 smoke：
  - `cmake --build out/build --config Release --target LASPointCloudViewer LASViewerSmokeTest -- /p:PostBuildEventUseInBuild=false`
- 已完成验证：
  - `.\out\build\bin\Release\LASViewerSmokeTest.exe --mode project-explorer-mainwindow --las .\test_data\ezhou_powerline_sample.las`
  - `.\out\build\bin\Release\LASViewerSmokeTest.exe --mode main-backstage --las .\test_data\ezhou_powerline_sample.las`
  - `.\out\build\bin\Release\LASViewerSmokeTest.exe --mode main-settings-restore`
- 结果：
  - 三个 smoke 全部通过
  - 本轮确认“不要只编 smoke”的要求已满足：`LASPointCloudViewer` 与 `LASViewerSmokeTest` 都已重编
  - `task_plan.md` 中 `Phase 2-5` 已同步收口为 `complete`
