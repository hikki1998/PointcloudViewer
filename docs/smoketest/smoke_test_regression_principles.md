# LASViewer Smoke Test 各模式回归原理（代码结合版）

本文说明项目如何在**无人工干预**情况下执行 GUI smoke test，并按 mode 记录回归目标与当前源码位置。

## 1. 总体架构

仓库保持单一冒烟可执行文件 `LASViewerSmokeTest.exe`：

- 目标声明：[CMakeLists.txt#L110](../../CMakeLists.txt#L110)
- smoke 源文件清单：[examples/CMakeLists.txt#L1](../../examples/CMakeLists.txt#L1)
- `las_viewer_add_smoke_sources()`：[cmake/LASViewerTargetConfig.cmake#L52](../../cmake/LASViewerTargetConfig.cmake#L52)
- 共享声明：[examples/SmokeTestSupport.h#L1](../../examples/SmokeTestSupport.h#L1)

实现已按职责拆成正式编译单元：

| 文件 | 职责 |
|---|---|
| [viewer_smoke_test.cpp](../../examples/viewer_smoke_test.cpp) | 公共 helper、CLI、mode 校验、注册与调度 |
| [ViewerRenderSmoke.cpp](../../examples/ViewerRenderSmoke.cpp) | Viewer 渲染和真实点云加载 |
| [MainWindowSmoke.cpp](../../examples/MainWindowSmoke.cpp) | MainWindow、设置恢复、录屏、日志 |
| [ProjectAndClassificationSmoke.cpp](../../examples/ProjectAndClassificationSmoke.cpp) | Project Explorer、分类、显示和量测 controller |
| [RouteSmoke.cpp](../../examples/RouteSmoke.cpp) | 航线 controller、漫游、JSON 和互操作 |
| [TowerIssueSmoke.cpp](../../examples/TowerIssueSmoke.cpp) | 杆塔、隐患和杆塔文件/工程链接 |

每个 case 仍由同一 `SmokeCase` 结构注册：

- 结构定义：[examples/viewer_smoke_test.cpp#L88](../../examples/viewer_smoke_test.cpp#L88)
- 20 个 case 的注册列表：[examples/viewer_smoke_test.cpp#L975](../../examples/viewer_smoke_test.cpp#L975)

结论：扩展方式仍是“**单 exe + mode 分发**”，拆分源码不会增加 smoke executable。

## 2. 从命令行到模式分发

### 2.1 CLI 与输入

- 逗号分隔参数解析：[examples/viewer_smoke_test.cpp#L756](../../examples/viewer_smoke_test.cpp#L756)
- usage 与模式清单：[examples/viewer_smoke_test.cpp#L771](../../examples/viewer_smoke_test.cpp#L771)
- LAS 输入和默认样本解析：[examples/viewer_smoke_test.cpp#L792](../../examples/viewer_smoke_test.cpp#L792)
- mode/category 合法性校验：[examples/viewer_smoke_test.cpp#L813](../../examples/viewer_smoke_test.cpp#L813)
- `main()` 参数注册：[examples/viewer_smoke_test.cpp#L926](../../examples/viewer_smoke_test.cpp#L926)

若未提供 `--las`，默认使用：

```text
./test_data/ezhou_powerline_sample.las
```

### 2.2 选择和顺序执行

`runSelectedSmokes()` 负责：

- 根据 mode/category 过滤 case
- 检查 requiresLas 输入
- 按注册顺序执行
- 输出 selected/passed/failed summary

代码：[examples/viewer_smoke_test.cpp#L868](../../examples/viewer_smoke_test.cpp#L868)

`--mode all` 或不传 mode/category 时，会运行注册列表中的全部 20 项。

## 3. 无人工干预机制

通用 Qt 驱动 helper 仍位于入口文件：

- 事件泵 `pumpEvents()`：[examples/viewer_smoke_test.cpp#L99](../../examples/viewer_smoke_test.cpp#L99)
- 表格右键菜单：[examples/viewer_smoke_test.cpp#L128](../../examples/viewer_smoke_test.cpp#L128)
- 树右键菜单：[examples/viewer_smoke_test.cpp#L164](../../examples/viewer_smoke_test.cpp#L164)
- 表格双击：[examples/viewer_smoke_test.cpp#L200](../../examples/viewer_smoke_test.cpp#L200)
- 树节点双击：[examples/viewer_smoke_test.cpp#L222](../../examples/viewer_smoke_test.cpp#L222)
- 颜色对话框自动接受：[examples/viewer_smoke_test.cpp#L248](../../examples/viewer_smoke_test.cpp#L248)

测试通过 QAction、控件 setter、Qt signal、`QMetaObject::invokeMethod` 和 `QApplication::sendEvent` 驱动真实信号槽，而不是依赖人工鼠标操作。

异步 UI 对象的测试应验证真实产品契约。例如带 `WA_DeleteOnClose` 的非模态对话框，Cancel 的契约是“关闭/不可见”，对象析构可能由 `deleteLater()` 延迟完成；测试使用 `QPointer` 防止悬空访问，但不要求同步销毁。

## 4. 真实数据加载路径

场景实现中的真实加载入口：

- Viewer render：[examples/ViewerRenderSmoke.cpp#L88](../../examples/ViewerRenderSmoke.cpp#L88)
- MainWindow 集成：[examples/MainWindowSmoke.cpp#L90](../../examples/MainWindowSmoke.cpp#L90)
- Project Explorer MainWindow：[examples/ProjectAndClassificationSmoke.cpp#L213](../../examples/ProjectAndClassificationSmoke.cpp#L213)
- Profile Classification Controller：[examples/ProjectAndClassificationSmoke.cpp#L487](../../examples/ProjectAndClassificationSmoke.cpp#L487)
- Measurement Analysis Controller：[examples/ProjectAndClassificationSmoke.cpp#L719](../../examples/ProjectAndClassificationSmoke.cpp#L719)
- Route Roam：[examples/RouteSmoke.cpp#L300](../../examples/RouteSmoke.cpp#L300)

Viewer 产品代码已拆至：

- `loadPointCloudFiles()`：[src/gui/PointCloudViewer.Loading.cpp#L148](../../src/gui/PointCloudViewer.Loading.cpp#L148)
- `appendPointCloudFiles()`：[src/gui/PointCloudViewer.Loading.cpp#L414](../../src/gui/PointCloudViewer.Loading.cpp#L414)
- `clearPointCloud()`：[src/gui/PointCloudViewer.Loading.cpp#L586](../../src/gui/PointCloudViewer.Loading.cpp#L586)

MainWindow 封装入口：

- 加载：[src/gui/MainWindow.PointCloud.cpp#L68](../../src/gui/MainWindow.PointCloud.cpp#L68)
- 追加：[src/gui/MainWindow.PointCloud.cpp#L105](../../src/gui/MainWindow.PointCloud.cpp#L105)
- 清空：[src/gui/MainWindow.PointCloud.cpp#L141](../../src/gui/MainWindow.PointCloud.cpp#L141)
- 工程加载：[src/gui/MainWindow.ProjectSerializer.cpp#L22](../../src/gui/MainWindow.ProjectSerializer.cpp#L22)

## 5. 20 个 mode 的回归目标

### 5.1 render

1. `viewer-render`（requiresLas=true）
   - 实现：[examples/ViewerRenderSmoke.cpp](../../examples/ViewerRenderSmoke.cpp)
   - LAS/LAZ：检查 framebuffer 可见像素、点击后渲染和轨道拖拽/反转映射。
   - Gaussian PLY：同一 mode 把 `--las` 指向受支持 `.ply`，额外验证异步加载、自由轨迹球、交互降级状态和连续渲染；当前没有独立 `gaussian-render` mode。

### 5.2 ui

2. `main-backstage`（false）
   - 实现：[examples/MainWindowSmoke.cpp](../../examples/MainWindowSmoke.cpp)
   - MainWindow 大集成，覆盖 Dock、Ribbon、导航、量测、净空、航线、杆塔、隐患和 Backstage。
   - 同时覆盖空场景最近工作台、缩略图缓存命中/失效与测试缓存隔离。
   - 航点编辑器通过生产 viewer 双击信号打开，Cancel 后验证对话框关闭。

3. `main-settings-restore`（false）
   - 实现：[examples/MainWindowSmoke.cpp#L1285](../../examples/MainWindowSmoke.cpp#L1285)
   - 使用隔离的 QSettings 路径验证窗口、交互和 UI 状态重启恢复。

4. `screen-recording`（false）
   - 实现：[examples/MainWindowSmoke.cpp#L1778](../../examples/MainWindowSmoke.cpp#L1778)
   - 验证录屏入口、状态和输出流程；平台能力不可用时按实现约定处理。

5. `log-panel`（false）
   - 实现：[examples/MainWindowSmoke.cpp#L1894](../../examples/MainWindowSmoke.cpp#L1894)
   - 验证日志总量、级别过滤、搜索和清空联动。

6. `project-explorer-dock`（false）
   - 实现：[examples/ProjectAndClassificationSmoke.cpp#L88](../../examples/ProjectAndClassificationSmoke.cpp#L88)
   - 验证 Dock、toolbar、tree 和 search 基础契约。

7. `project-explorer-controller`（false）
   - 实现：[examples/ProjectAndClassificationSmoke.cpp#L120](../../examples/ProjectAndClassificationSmoke.cpp#L120)
   - 验证过滤、展开折叠和 controller action wiring。

8. `project-explorer-mainwindow`（true）
   - 实现：[examples/ProjectAndClassificationSmoke.cpp#L213](../../examples/ProjectAndClassificationSmoke.cpp#L213)
   - 真实加载后验证项目树构建、过滤、显隐、上下文菜单和定位联动。

9. `visualization-panel-controller`（false）
   - 实现：[examples/ProjectAndClassificationSmoke.cpp#L575](../../examples/ProjectAndClassificationSmoke.cpp#L575)
   - 验证点大小、透明度、背景和着色模式同步。

10. `measurement-analysis-controller`（true）
    - 实现：[examples/ProjectAndClassificationSmoke.cpp#L719](../../examples/ProjectAndClassificationSmoke.cpp#L719)
    - 验证量测模式、净空/植被风险 action、callback 和表格选择链路。

11. `profile-classification-widget`（false）
    - 实现：[examples/ProjectAndClassificationSmoke.cpp#L460](../../examples/ProjectAndClassificationSmoke.cpp#L460)
    - 验证分类 Widget 的标题、模式、源类别和目标类别基础契约。

12. `profile-classification-controller`（true）
    - 实现：[examples/ProjectAndClassificationSmoke.cpp#L487](../../examples/ProjectAndClassificationSmoke.cpp#L487)
    - LAS 加载后验证分类初始化、全选/清空、目标类别和选择模式同步。

13. `route-controller`（false）
    - 实现：[examples/RouteSmoke.cpp#L88](../../examples/RouteSmoke.cpp#L88)
    - 验证航线 action/button/spinbox/combobox 的 controller 回调桥接。

14. `tower-controller`（false）
    - 实现：[examples/TowerIssueSmoke.cpp#L88](../../examples/TowerIssueSmoke.cpp#L88)
    - 验证杆塔编辑 action、表格列显隐、选择和详情提交。

15. `issue-controller`（false）
    - 实现：[examples/TowerIssueSmoke.cpp#L286](../../examples/TowerIssueSmoke.cpp#L286)
    - 验证隐患编辑/导出 action、表格选择和详情提交。

### 5.3 route

16. `route-json`（false）
    - 实现：[examples/RouteSmoke.cpp#L442](../../examples/RouteSmoke.cpp#L442)
    - 验证模板导入、JSON roundtrip 和辅助航点字段保真。

17. `route-interop`（false）
    - 实现：[examples/RouteSmoke.cpp#L530](../../examples/RouteSmoke.cpp#L530)
    - 验证 CRS、风险点生成航线、桥接结构以及 KML/KMZ 互操作。

18. `route-roam`（true）
    - 实现：[examples/RouteSmoke.cpp#L300](../../examples/RouteSmoke.cpp#L300)
    - 验证 start/pause/resume/stop、速度边界及数据清理后的自动停止。

### 5.4 tower

19. `tower-file`（false）
    - 实现：[examples/TowerIssueSmoke.cpp#L405](../../examples/TowerIssueSmoke.cpp#L405)
    - 验证 LiTower 导入导出格式、字段和浮点精度。

20. `tower-project-link`（false）
    - 实现：[examples/TowerIssueSmoke.cpp#L488](../../examples/TowerIssueSmoke.cpp#L488)
    - 验证工程内 linked tower file 路径、索引归一化和回写一致性。

## 6. `all` 模式执行顺序

执行顺序与 [注册列表](../../examples/viewer_smoke_test.cpp#L975) 一致：

1. viewer-render
2. main-backstage
3. main-settings-restore
4. screen-recording
5. log-panel
6. project-explorer-dock
7. project-explorer-controller
8. project-explorer-mainwindow
9. visualization-panel-controller
10. measurement-analysis-controller
11. profile-classification-widget
12. profile-classification-controller
13. route-controller
14. tower-controller
15. issue-controller
16. route-json
17. route-interop
18. route-roam
19. tower-file
20. tower-project-link

## 7. 当前回归边界

- 默认 LAS 路径仍依赖仓库测试数据：[resolveLasInputs](../../examples/viewer_smoke_test.cpp#L792)
- `pumpEvents(固定毫秒)` 仍可能受慢机影响：[pumpEvents](../../examples/viewer_smoke_test.cpp#L99)
- render 检查以 framebuffer 可见像素为主，是轻量图像启发式，不是像素级黄金图比较。
- smoke 重点是 GUI wiring、状态机和导入导出链路，不替代算法精度或超大数据压力测试。

## 8. 新增 mode 的规范

新增 mode 时必须同步：

1. 在对应职责的 `*Smoke.cpp` 中实现 `runXxxSmoke(const QStringList&)`。
2. 在 [SmokeTestSupport.h](../../examples/SmokeTestSupport.h) 声明。
3. 在 [注册列表](../../examples/viewer_smoke_test.cpp#L975) 添加 case。
4. 在 [validateSelections()](../../examples/viewer_smoke_test.cpp#L813) 添加合法 mode。
5. 更新 [printUsageSummary()](../../examples/viewer_smoke_test.cpp#L771) 和本文的模式数量/顺序。
6. 继续复用唯一的 `LASViewerSmokeTest.exe`，不要新增独立 smoke executable。
