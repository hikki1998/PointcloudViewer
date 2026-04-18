# 重构回归深度排查与修复计划

## 目标

针对 `MainWindow` 拆分重构后可能出现的“功能仍在、接线/状态/交互丢失”类问题，建立一套系统性的排查与修复流程，避免继续靠用户逐个触发隐蔽回归。

## 当前背景

- 2026-04-15 的 `b9a8e3b` 进行了大规模 `MainWindow` 边界重构。
- 重构后已出现并修复/定位的回归包括：
  - 主窗口标题区双击/拖拽/最大化交互异常
  - 全屏状态点击导致闪屏
  - 右侧 dock 在 1080P 下过宽且不可继续缩小
  - `Project Explorer` 右键菜单失效
- 最近一次新增证据表明：`ProjectExplorerController` 的信号仍然发出，但 `MainWindow.Connections.cpp` 漏接了 `customContextMenuRequested`、`itemChanged`、`currentItemChanged`、`itemDoubleClicked`、`searchTextChanged` 这组项目树相关接线。

## 风险假设

这类回归不是单点 bug，而是重构后的典型风险模式：

1. 原先在 `MainWindow.cpp` 中直接连接的 widget 信号，拆分后漏迁移到 `MainWindow.Connections.cpp`
2. 直接 widget 逻辑改成 controller 转发后，controller -> MainWindow 的二段接线缺失
3. `QSettings` / `saveState()` / `restoreState()` 相关恢复链条断裂
4. dock / tab / objectName / minimumSize / screenChanged 等窗口状态约束在拆分后分散，导致交互退化
5. 旧 smoke 覆盖 controller 单体行为，但没有覆盖“controller 已接到 MainWindow 后是否仍工作”
6. `retranslateUi()`、`updateActionState()`、selection 同步、显示状态同步等跨模块副作用漏迁移

## 本轮排查范围

- `src/gui/MainWindow.*`
- `src/gui/PointCloudViewer.*`
- `src/gui/*Controller.*`
- `src/gui/*Dock.*`
- `examples/viewer_smoke_test.cpp`
- 必要时补充 `docs/agent/*.md` 中的验证与接手规则

## 不在本轮首批范围

- 点云渲染算法正确性
- 巡检业务计算结果正确性
- 大规模 UI 样式重做

## 阶段计划

### Phase 1 - 建立回归基线
状态：`complete`

目标：
- 把当前已知回归、近期修复点、重构边界和未覆盖区域整理成统一台账。

动作：
- 汇总 `b9a8e3b` 之后已经发现的回归模式
- 标出高风险拆分文件：`MainWindow.Connections.cpp`、`MainWindow.Core.cpp`、`MainWindow.Docks.cpp`、`MainWindow.SettingsStore.cpp`
- 盘点现有 smoke 模式与空白区

产出：
- `findings.md` 的风险地图
- 回归家族清单

### Phase 2 - 静态对等性审计
状态：`complete`

目标：
- 以“重构前 `MainWindow.cpp` 对等功能是否全部迁移”为主线做静态审计。

动作：
- 对比重构前后以下维度：
  - widget / controller / viewer 信号槽接线
  - QAction 创建后是否仍有 trigger 接收方
  - dock 创建后是否仍有 visibility / tabify / state 恢复处理
  - `objectName` 是否满足 `saveState()` / `restoreState()`
  - `retranslateUi()` 及 `updateActionState()` 是否仍覆盖所有入口
- 优先审计这些功能面：
  - `Project Explorer`
  - Backstage / Ribbon
  - Route 相关 dock、表格和场景联动
  - Tower / Issue 列表与详情联动
  - Profile / Measurement / Classification 面板

产出：
- 一份“旧实现 -> 新落点”的对照表
- 一份“已迁移 / 疑似漏迁移 / 已确认回归”的问题清单

### Phase 3 - 动态交互审计
状态：`complete`

目标：
- 覆盖“代码存在但操作链路断开”的交互类回归。

动作：
- 为每个高风险面设计最小人工/半自动检查清单：
  - 左侧项目树：搜索、勾选、双击、右键、选中同步
  - Ribbon / Backstage：按钮跳转、页面切换、窗口控制
  - dock：显示/隐藏、tabify、宽度恢复、拖拽、浮动
  - route：表格选中、场景高亮、dock 显示、QA
  - tower / issue：列表选中、聚焦、详情编辑、导入导出入口
  - viewer：拾取、量测、状态栏、标题区交互
- 将人工步骤转成 smoke 候选

产出：
- 模块化交互检查表
- smoke 补点优先级列表

### Phase 4 - 补齐自动化回归验证
状态：`complete`

目标：
- 把本次重构最容易漏的“接线型回归”尽量沉淀到 `LASViewerSmokeTest`。

动作：
- 新增或扩展 smoke：
  - `Project Explorer` 主窗口级交互 smoke
  - dock 状态恢复 / 宽度钳制 smoke
  - 标题区空白交互 smoke
  - route / issue / tower 关键表格-场景联动 smoke
- 明确哪些 smoke 是 controller 单体，哪些 smoke 必须走 MainWindow 集成路径

产出：
- 新 smoke mode / 断言清单

### Phase 5 - 按问题家族分批修复
状态：`complete`

目标：
- 避免按用户报障顺序零散修，转为“同类问题一次修一批”。

批次建议：
- 批次 A：连接缺失类
  - controller -> MainWindow
  - widget -> MainWindow
  - action -> handler
- 批次 B：窗口 / dock / state 恢复类
  - `objectName`
  - `saveState()` / `restoreState()`
  - screen / fullscreen / maximize / tabify / min size
- 批次 C：选择 / 展示同步类
  - 列表选中 -> viewer
  - viewer -> 面板
  - `updateActionState()` / `retranslateUi()`
- 批次 D：验证与文档类
  - smoke
  - `docs/agent/*`

产出：
- 每批修复后单独构建与 smoke 记录

完成情况：
- 已完成 `Project Explorer`、Backstage / Ribbon、tower / issue / measurement / route、dock / settings 恢复等主窗口级与控制器级审查
- 审查 `Phase 2-5` 后，唯一仍有必要补的自动化缺口是 `Project Explorer` 主窗口级集成 smoke
- 已在 `examples/viewer_smoke_test.cpp` 新增 `project-explorer-mainwindow` mode，覆盖：
  - 搜索过滤
  - `currentItemChanged`
  - `itemChanged`
  - `itemDoubleClicked`
  - `customContextMenuRequested`
- 已完成验证：
  - `cmake --build out/build --config Release --target LASPointCloudViewer LASViewerSmokeTest -- /p:PostBuildEventUseInBuild=false`
  - `.\out\build\bin\Release\LASViewerSmokeTest.exe --mode project-explorer-mainwindow --las .\test_data\ezhou_powerline_sample.las`
  - `.\out\build\bin\Release\LASViewerSmokeTest.exe --mode main-backstage --las .\test_data\ezhou_powerline_sample.las`
  - `.\out\build\bin\Release\LASViewerSmokeTest.exe --mode main-settings-restore`
- 结论：当前 `Phase 2-5` 已无新的高优先级 actionable gap，后续工作可转为常规功能开发或新增需求

### Phase 6 - 回归收口与防复发
状态：`complete`

目标：
- 让后续继续拆分或重构时，优先暴露“漏接线”而不是靠用户反馈。

动作：
- 给 `MainWindow.Connections.cpp` 建立排查规则
- 总结“拆分后必须核对”的检查清单
- 如有必要，在文档中新增“重构后对等性检查”章节

产出：
- 最终回归报告
- 重构检查清单

完成情况：
- 已新增 `docs/agent/refactor-regression-report.md`
- 已在 `docs/agent/workflows.md` 补充“MainWindow 重构后对等性检查”
- 已在 `docs/agent/README.md` / `docs/agent/session-handoff.md` 增加入口，便于后续会话直接接手

## 核心检查清单

### 连接类
- 每个 controller 的 `signals:` 是否都在 `MainWindow.Connections.cpp` 有接收方
- 每个关键 widget 信号是否有主窗口集成路径
- 每个 QAction 是否仍然可达业务处理逻辑

### 状态类
- 每个 `QDockWidget` 是否有稳定 `objectName`
- 影响 `saveState()` 的 dock / tab / toolbar 是否全具名
- `loadWindowSettings()` 后是否重新归一化窗口 / dock 状态

### 交互类
- 选中、双击、右键、勾选、过滤是否都触发 UI 与场景同步
- 标题区、全屏、最大化、窗口拖拽、边缘缩放是否互不干扰

### 验证类
- 是否只有 controller 单体 smoke，没有 MainWindow 集成 smoke
- 是否有“能编译但实际链路断开”的交互空白区

## 当前优先级排序

1. `MainWindow.Connections.cpp` 对等性审计
2. `MainWindow.SettingsStore.cpp` / dock 状态恢复审计
3. `MainWindow.Core.cpp` 窗口事件与标题区交互审计
4. `MainWindow.Docks.cpp` / `*Dock.cpp` 尺寸与 objectName 审计
5. smoke 覆盖补齐

## 验证基线

```powershell
cmake --build out/build --config Release --target LASPointCloudViewer LASViewerSmokeTest -- /p:PostBuildEventUseInBuild=false
.\out\build\bin\Release\LASViewerSmokeTest.exe --mode main-backstage
.\out\build\bin\Release\LASViewerSmokeTest.exe --mode viewer-render --las .\test_data\ezhou_powerline_sample.las
```

后续扩展验证优先加入：

```powershell
.\out\build\bin\Release\LASViewerSmokeTest.exe --mode project-explorer-controller
```

## 当前已知限制

- 用户工作区存在未提交的 `AGENTS.md` 和 `.gitignore`，后续修复提交时必须排除。
- 使用 `/p:PostBuildEventUseInBuild=false` 构建时，翻译 `.qm` 不会自动部署到 `out/build/bin/Release/translations/`；若要验证最新中文翻译，需要额外同步运行目录资源或改用带 post-build 的标准部署流程。

---

## 2026-04-18 新任务：竞品能力对标与可迁移功能规划

### 目标

- 调研行业常见点云巡检/处理软件的核心能力。
- 基于本项目现状提出可迁移性强、可分期落地的新功能路线图。

### 阶段计划

#### Phase A - 竞品公开能力采样
状态：`complete`

动作：
- 抓取可访问的产品页，提取核心能力关键词。
- 对反爬/404页面进行替代采样，优先保证能力覆盖面。

#### Phase B - 能力归一化与差距映射
状态：`complete`

动作：
- 将竞品能力归一为统一能力域：数据接入、自动化处理、巡检闭环、协同与开放。
- 对照本项目已具备能力，识别“缺失但可迁移”的机会点。

#### Phase C - 新功能路线图（强调可迁移性）
状态：`complete`

动作：
- 形成分阶段（近期/中期/远期）功能路线。
- 每个候选功能标注：迁移价值、工程复杂度、依赖风险、对现有架构落点。

#### Phase D - 输出决策版建议
状态：`complete`

动作：
- 给出推荐优先级与最小可行里程碑。
- 给出兼容当前 Qt/OSG/C++ 架构的实施原则，避免耦合到单一硬件/平台。
