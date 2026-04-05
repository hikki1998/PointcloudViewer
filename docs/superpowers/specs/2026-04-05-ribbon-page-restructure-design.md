# Ribbon 页面重组设计

日期：2026-04-05

## 背景

当前 [MainWindow.cpp](/E:/code/VibeCodingProject/las_pointcloud_viewer/src/gui/MainWindow.cpp) 的 `Home` Ribbon page 同时承载数据集、相机、场景辅助、量测、分类编辑、植被风险、航线规划、隐患管理、工作区等多类功能。首页信息密度过高，通用浏览能力与业务操作混杂，导致首次使用和日常定位按钮的成本都偏高。

## 目标

本次调整只重组 Ribbon 的页面与分组结构，不改变已有业务 action 的行为和槽连接。

目标如下：

1. `Home` 只保留通用浏览与高频基础能力。
2. 航线、隐患、分析类功能拆到独立业务页签。
3. `Measure` 保留在 `Home`。
4. 分类相关操作从 `Measure` 中拆出，单独放到 `Home` 的 `Classification` group。
5. `Tower` 和 `Appearance` 保持现有能力，不在本轮做额外功能重构。

## 设计原则

- 只重排现有 `QAction` 的归属，不新增业务动作，不改动作含义。
- 保持 `Home` 为默认第一页。
- 保持 Quick Access Bar 不变，避免高频操作路径被打乱。
- 组内动作按“主操作在前，辅助操作在后”排列。
- UI 风格延续当前浅色、高可读 Ribbon 样式。

## 页签结构

### Home

`Home` 只保留通用入口和高频基础操作，包含以下 group：

1. `Dataset`
   - `Open`
   - `Add LAS Files`
   - `Open Project`
   - `Save Project`
   - `Save Project As`
   - `Clear`

2. `Camera`
   - `Fit Scene`
   - `Top`
   - `Front`
   - `Right`

3. `Scene`
   - `Axes`
   - `Bounds`
   - `Dark`
   - `Light`

4. `Measure`
   - `Measure`
   - `Clear Measure`
   - `Profile View`

5. `Classification`
   - `Profile Classify`
   - `Classify Panel`
   - `Save Classify Edits`
   - `Undo`
   - `Redo`
   - `Clear Classify Edits`
   - `Export Clearance CSV`

6. `Workspace`
   - `Project Coordinate Systems`
   - `Log`
   - `Exit`

### Route

新增 `Route` page，承接航线相关操作，包含以下 group：

1. `Route Planning`
   - `Generate Route`
   - `Regenerate Route`
   - `Clear Route`
   - `Focus Route Point`

2. `Route Files`
   - `Import Route File`
   - `Save Route File`
   - `Reload Route File`
   - `Import Route KML`
   - `Export Route KML`
   - `Export DJI KMZ`

### Analysis

新增 `Analysis` page，用于分析与巡检派生业务，当前只承接植被风险，包含以下 group：

1. `Vegetation Risks`
   - `Analyze Risks`
   - `Focus Current Risk`
   - `Create Issue`
   - `Create All Issues`
   - `Clear Risks`

### Issue

新增 `Issue` page，承接隐患管理相关操作，包含以下 group：

1. `Inspection Issues`
   - `Mark Issue`
   - `Cancel Issue Tool`
   - `Focus Current Issue`
   - `Remove Current Issue`
   - `Clear Issues`
   - `Export CSV`
   - `Export Report`

### Tower

保留现有 `Tower` page 和 `Tower Editing` group，不改本轮结构。

### Appearance

保留现有 `Appearance` page，继续包含：

1. `Point Colors`
2. `Office Theme`
3. `Language`

## 交互约束

### 默认路径

- 应用启动后仍默认进入 `Home`。
- Quick Access Bar 继续保留：
  - `Open`
  - `Save Project`
  - `Fit Scene`
  - `Axes`
  - `Measure`

### 操作稳定性

- 所有动作继续复用现有 `QAction` 实例。
- 不修改现有触发逻辑、启用禁用逻辑、翻译上下文和快捷入口。
- 只允许调整 Ribbon 中 `page -> group -> action` 的组织关系。

## 实现范围

本次实现仅包含：

1. 在 [MainWindow.h](/E:/code/VibeCodingProject/las_pointcloud_viewer/src/gui/MainWindow.h) 中补充新 page/group 成员声明。
2. 在 [MainWindow.cpp](/E:/code/VibeCodingProject/las_pointcloud_viewer/src/gui/MainWindow.cpp) 的 `createRibbon()` 中重组页签与 group。
3. 新增页签名称与分组名称时，同步更新 [lasviewer_zh_CN.ts](/E:/code/VibeCodingProject/las_pointcloud_viewer/translations/lasviewer_zh_CN.ts)。

本次明确不包含：

- 新业务 action
- 动作行为重写
- Quick Access Bar 重构
- Dock、表格或场景交互改造
- `Tower`、`Appearance` 页内的额外细分

## 风险与缓解

1. 风险：只移动按钮但遗漏某个 action，导致入口消失。
   缓解：对照当前 `createRibbon()` 的 action 列表逐项迁移。

2. 风险：新增 page/group 文本后出现漏翻译。
   缓解：更新 `translations/lasviewer_zh_CN.ts` 并重新构建生成 `.qm`。

3. 风险：主页过度瘦身后影响高频操作。
   缓解：保留 `Measure` 在 `Home`，并保持 Quick Access Bar 不变。

## 验证

至少执行以下验证：

1. 构建：
   - `cmake --build out/build --config Release --target LASPointCloudViewer LASViewerSmokeTest`
2. GUI 启动检查：
   - `.\out\build\bin\Release\LASPointCloudViewer.exe`
   - 确认 Ribbon 页签结构与分组顺序符合本设计。
3. 视情况补充 smoke test：
   - `.\out\build\bin\Release\LASViewerSmokeTest.exe .\test_data\ezhou_powerline_sample.las`
