# LAS Point Cloud Viewer

基于 Qt 5.15、OpenSceneGraph 和 LASlib 的 Windows 桌面点云查看器，面向电力巡检/通道检查场景，支持点云浏览、量测、净空分析、杆塔/隐患管理和巡检航线编辑。

如果你是第一次进入这个仓库，不要直接散读源码。先按下面顺序建立上下文：

1. `AGENTS.md`
2. `CLAUDE.md`（如果你使用 Claude Code）
3. `docs/agent/README.md`

## 目录

- `src/`：应用源码
- `examples/`：示例和 smoke test
- `test_data/`：测试数据与生成脚本
- `shaders/`：渲染资源
- `3rd/`：仓库内随代码分发的精简版 release 三方库
- `out/`：推荐的本地构建输出目录，不纳入版本控制
- `docs/agent/`：面向 agent 的渐进式披露文档
- `planning/`：规划与路线图文档

源码声明采用按目录就近维护：

- 顶层 `CMakeLists.txt` 只负责项目入口、目标创建和模块接线
- `cmake/*.cmake` 负责依赖、翻译、目标配置和 runtime deploy
- `src/*/CMakeLists.txt` 与 `examples/CMakeLists.txt` 负责各目录源码通过 `target_sources()` 加入目标

## 构建

推荐使用独立输出目录，避免将编译产物混入源码树：

```powershell
cmake -S . -B out/build -G "Visual Studio 17 2022" -A x64 `
  -DQT_ROOT=E:/code/Qt5.15.2/5.15.2/msvc2019_64

cmake --build out/build --config Release --target LASPointCloudViewer
cmake --build out/build --config Release --target LASViewerSmokeTest
```

默认会使用仓库内 `3rd/` 作为 `THIRDPARTY_ROOT`。其中包含 `osg/`、`qtitan/`、`laslib/`、`lastools/` 这几个精简版 release 依赖目录，每个目录根下都有 `.version` 标记文件。Qt 不随仓库分发，仍需通过 `QT_ROOT` 单独指定。

新增源码文件时，不需要回头修改顶层 `CMakeLists.txt`，只需要更新所属目录下的本地 `CMakeLists.txt`。

## 运行

```powershell
.\out\build\bin\Release\LASPointCloudViewer.exe
.\out\build\bin\Release\LASViewerSmokeTest.exe --mode viewer-render --las .\test_data\ezhou_powerline_sample.las
```

统一 Smoke Test 支持按模式和类别切换：

```powershell
# 查看帮助
.\out\build\bin\Release\LASViewerSmokeTest.exe --help

# 按模式执行（示例：相机漫游状态机回归）
.\out\build\bin\Release\LASViewerSmokeTest.exe --mode route-roam --las .\test_data\ezhou_powerline_sample.las

# 按类别执行（示例：全部航线相关 smoke）
.\out\build\bin\Release\LASViewerSmokeTest.exe --category route --las .\test_data\ezhou_powerline_sample.las

# 执行全部 smoke
.\out\build\bin\Release\LASViewerSmokeTest.exe --mode all --las .\test_data\ezhou_powerline_sample.las
```

## 发布打包

首版发布使用“最小可运行 ZIP 包”方式分发，默认面向 Windows x64 用户，不附带大型测试数据。

先执行 Release 构建：

```powershell
cmake --build out/build --config Release --target LASPointCloudViewer
```

然后执行打包脚本：

```powershell
.\scripts\package_release.ps1 -Version v1.3.0 -Config Release -BuildBinDir out/build/bin/Release -OutputDir out/release
```

默认会生成：

```text
out/release/LASPointCloudViewer-v1.3.0-windows-x64/
out/release/LASPointCloudViewer-v1.3.0-windows-x64.zip
out/release/LASPointCloudViewer-v1.3.0-linux-x64.tar.gz
```

发布说明模板见：

```text
docs/releases/v1.3.0.md
```

GitHub Release 建议流程：

1. 打开仓库 Releases 页面
2. 选择 `Draft a new release`
3. 选择 tag `v1.3.0`
4. 标题填写 `Release20260611-v1.3.0`
5. 粘贴 `docs/releases/v1.3.0.md` 的内容
6. 上传 `LASPointCloudViewer-v1.3.0-windows-x64.zip` 和 `LASPointCloudViewer-v1.3.0-linux-x64.tar.gz`
7. 发布

## 文档导航

### 第一跳
- `AGENTS.md`
  - Codex 仓库级自动入口和强约束
- `CLAUDE.md`
  - Claude Code 仓库级兼容入口

### 渐进式披露
- `docs/agent/context.md`
  - 5 分钟上下文、热文件、当前能力、验证基线
- `docs/agent/README.md`
  - 文档总入口和阅读路径
- `docs/agent/architecture.md`
  - 模块边界、热文件和核心链路
- `docs/agent/product-state.md`
  - 当前已经具备的用户可见能力
- `docs/agent/workflows.md`
  - 常见改动路径、验证、翻译、发布流程

### 专题文档
- `planning/PLAN.md`
  - 标准航线 JSON / IO 方案
- `planning/ROUTE_MODULE_ROADMAP.md`
  - 航线编辑与显示模块优化路线图
- `docs/history/codex-collaboration-retrospective.md`
  - 项目过程复盘，不是日常 onboarding 主入口

## 测试数据

```powershell
python .\test_data\create_test_las.py
python .\test_data\create_sampled_las.py <source.las> .\test_data\ezhou_powerline_sample.las --target-points 12000
```

## 依赖

- Qt 5.15.2
- OpenSceneGraph 3.6.5
- QtitanRibbon
- LASlib / LASzip
- Visual Studio 2022
- CMake 3.16+

仓库内 `3rd/` 目录布局：

```text
3rd/
  osg/
  qtitan/
  laslib/
  lastools/
```

