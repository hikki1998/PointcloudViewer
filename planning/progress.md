# Progress

## 2026-04-04
- 初始化 `planning/task_plan.md`、`planning/findings.md`、`planning/progress.md`。
- 已读取技能说明与 `PROJECT_CONTEXT.md`，开始调研 CRS 相关代码落点。
- 已确认当前 CRS 配置绑定在 `Inspection Route Planning` 面板与 `routePlanning` 序列化节点中。
- 已确认工程文件尚无工程级 CRS 元数据结构，工程树也没有项目属性入口。
- 已确认项目代码已经支持 PROJ 转换接口，但仓库 `3rd/` 中未内置 `proj` 依赖目录。
- 已新增 [CRS_IMPLEMENTATION_TASKS.md](/E:/code/VibeCodingProject/las_pointcloud_viewer/planning/CRS_IMPLEMENTATION_TASKS.md) 作为后续开发任务清单。
- 已将 GDAL 3.10.3 release 包解压到 `E:\code\thirdparty\gdal`，并补充 `.version` 与 `gdal-env.bat`。
- 已验证 GDAL 命令行工具可运行。
- 已新增 `src/domain/ProjectMetadata.h` 作为工程级 CRS 类型别名入口，并接入主窗口保存/加载流程。
- 已在 `MainWindow` 中加入工程级 CRS 成员与同步函数，项目保存版本提升到 `7`，新增 `projectProperties.coordinateSystems` 持久化。
- 已保留旧 `routePlanning.crs` 读取兼容逻辑，同时在加载新字段时优先使用工程级 CRS。
- 已完成 Release 构建验证与 smoke test；构建中曾因 `LASPointCloudViewer.exe` 占用触发 `LNK1104`，结束进程后恢复。
- 已将 `src/crs/` 模块接入 `CMakeLists.txt` 与 smoke test 公共源码，移除不存在的 `src/domain/ProjectMetadata.cpp` 引用，并调整 `LASViewerCrsExport.h` 以适配当前静态编译方式。
- 已新增 `Project Coordinate Systems` 对话框入口到工程树：工程根节点下现在可直接访问 `Coordinate Systems`，右键菜单和双击均可打开工程坐标系设置。
- 已将航线 KML/KMZ 导入导出改为直接使用工程级 `ProjectCoordinateSystems`，不再依赖旧 `routePlanningOptions_.crs` 作为运行时主数据源。
- 已执行 `lupdate` 更新 `translations/lasviewer_zh_CN.ts`，并补全 CRS 相关中文翻译；重新构建后 `.qm` 生成结果为 `605 finished / 0 unfinished`。
- 已再次完成 Release 构建与 `LASViewerSmokeTest.exe .\\test_data\\ezhou_powerline_sample.las` 验证，smoke test 通过。
- 已确认 `E:\code\thirdparty\gdal` 仅包含运行时文件，不包含 `include/lib`，因此不能直接作为 `PROJ_ROOT` 使用。
- 已下载并解压 GISInternals SDK `release-1944-x64-dev.zip` 到 `E:\code\thirdparty\gdal_sdk_1944_x64`，其中包含 `include\proj9\proj.h`、`lib\proj9.lib`、`bin\proj_9.dll`。
- 已调整 `CMakeLists.txt` 的 PROJ 探测逻辑，支持 GISInternals 的 `include/proj9` 与 `proj9.lib` 命名。
- 已重新运行 CMake 配置命令并传入 `-DPROJ_ROOT=E:/code/thirdparty/gdal_sdk_1944_x64`；配置输出已识别 `Using PROJ: ...\\proj9.lib`。
- 已再次完成 Release 构建与 smoke test，并确认 `out\\build\\bin\\Release` 中已部署 `proj_9.dll`。
- 已将 `E:\code\thirdparty\gdal_sdk_1944_x64` 升格为稳定目录 `E:\code\thirdparty\gdal`，并把旧运行时包备份到 `E:\code\thirdparty\gdal_runtime_legacy_3103_20260404`。
- 已为新的稳定 `gdal` 根目录补充 `.version` 与 `gdal-env.bat`，目录结构现与其他三方库一致。
- 已在 `CMakeLists.txt` 中增加 `GDAL_ROOT` 支持和从 `GDAL_ROOT` 自动推导 `PROJ_ROOT` 的逻辑，同时让运行时依赖扫描包含 `@GDAL_ROOT@/bin`。
- 已重新运行 `cmake -DTHIRDPARTY_ROOT=E:/code/thirdparty -DGDAL_ROOT=E:/code/thirdparty/gdal -DPROJ_ROOT=`，配置输出已识别 `Using GDAL SDK` 和新的稳定 `PROJ` 路径。
- 已再次完成 Release 构建、`gdal-env.bat` 环境脚本验证，以及 `LASViewerSmokeTest.exe .\\test_data\\ezhou_powerline_sample.las` 验证。
