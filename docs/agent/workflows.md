# Workflows

## Windows Build

```powershell
cmake -S . -B out/build -G "Visual Studio 17 2022" -A x64 -DQT_ROOT=E:/code/Qt5.15.2/5.15.2/msvc2019_64
cmake --build out/build --config Release --target LASPointCloudViewer LASViewerSmokeTest
```

只验证编译、跳过运行时部署：

```powershell
cmake --build out/build --config Release --target LASPointCloudViewer LASViewerSmokeTest -- /p:PostBuildEventUseInBuild=false
```

运行：

```powershell
.\out\build\bin\Release\LASPointCloudViewer.exe
```

## Smoke Test

仓库只保留一个 `LASViewerSmokeTest.exe`。当前 mode/category 以可执行文件 `--help` 和 `examples/viewer_smoke_test.cpp` 注册表为准。

```powershell
.\out\build\bin\Release\LASViewerSmokeTest.exe --help
.\out\build\bin\Release\LASViewerSmokeTest.exe --mode viewer-render --las .\test_data\ezhou_powerline_sample.las
.\out\build\bin\Release\LASViewerSmokeTest.exe --mode route-roam --las .\test_data\ezhou_powerline_sample.las
.\out\build\bin\Release\LASViewerSmokeTest.exe --mode main-backstage
.\out\build\bin\Release\LASViewerSmokeTest.exe --mode all --las .\test_data\ezhou_powerline_sample.las
```

Gaussian 复用 `viewer-render`，把 `--las` 指向受支持的小型 `.ply`。

### Smoke Rules

- 新场景加入现有 exe 的 mode/category，不新增独立 smoke executable。
- 测试真实产品入口、信号槽和状态契约，不复制一套测试逻辑。
- UI/渲染/交互改动至少跑相关 mode；较大改动跑 `--mode all`。
- `viewer-render` 覆盖异步 API、preview、UI heartbeat、交互 LOD、异步追加、多数据集、按需缓存和 framebuffer 非空检查。
- 小型仓库样本只防功能回归，不替代大数据内存、FPS 或 GPU 上传压力测试。
- 实现位于 `examples/*Smoke.cpp`；注册和 CLI 位于 `examples/viewer_smoke_test.cpp`。

## Validation Matrix

- UI、交互、渲染、点选、翻译、构建脚本：至少 Release 构建。
- 显示结果变化：运行 `viewer-render`。
- 航线显示/漫游/互操作：运行 route 类 smoke。
- Backstage、dock、设置恢复：运行 `main-backstage` 和 `main-settings-restore`。
- 修改 MainWindow/controller 接线：除单体 controller smoke 外，至少跑一条 MainWindow 集成路径。
- UI 样式：人工检查 Ribbon、dock、Message Box、菜单、表格、ComboBox 本体/下拉列表和覆盖层可读性。

## Translation

```powershell
E:\code\Qt5.15.2\5.15.2\msvc2019_64\bin\lupdate.exe src -ts translations\lasviewer_zh_CN.ts
cmake --build out/build --config Release --target LASPointCloudViewer
```

补全所有 unfinished 翻译。若构建时跳过部署，验证中文运行时前手动同步：

```powershell
Copy-Item "out/build/translations/lasviewer_zh_CN.qm" "out/build/bin/Release/translations/lasviewer_zh_CN.qm" -Force
```

## Linux

当前 Ubuntu 22.04 操作指南见 `docs/linux-build.md`。

```bash
bash scripts/linux/setup-ubuntu-22.04.sh
cmake -S . -B out/linux/build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DQT_ROOT=/usr \
  -DTHIRDPARTY_ROOT="$PWD/3rd" \
  -DPROJ_ROOT=/usr \
  -DLAS_VIEWER_ENABLE_WINDOWS_CAPTURE=OFF
cmake --build out/linux/build --target LASPointCloudViewer LASViewerSmokeTest -j 4
```

Linux 默认使用系统 LASzip API 和仓库 Qtitan shim，不包含 Windows 录屏后端。

## Packaging

Windows：

```powershell
.\scripts\package_release.ps1 -Version v1.4.0 -Config Release -BuildBinDir out/build/bin/Release -OutputDir out/release
```

Linux：

```bash
bash scripts/linux/package-release.sh v1.4.0 out/linux/build/bin out/release
```

发布说明位于 `docs/releases/`。

## Planning And Workspace

- named plan 只放 `.planning/<plan-id>/`，不要在项目根目录或 `docs/` 放 planning 状态。
- `.planning/` 是本地工作记忆，不是正式产品文档。
- 不提交 `out/`、`.qm`、本地截图或大型测试数据。
- 测试数据优先使用 `test_data/ezhou_powerline_sample.las`；新增数据需裁剪并附生成脚本。

## Documentation Ownership

- 当前能力和限制：`product-state.md`
- 架构、文件定位和核心链路：`context.md`
- 构建、验证、翻译和发布：本文档
- 强约束：根目录 `AGENTS.md`
- 历史版本：`docs/releases/`
