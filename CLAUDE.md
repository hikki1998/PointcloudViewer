# CLAUDE.md

兼容性入口文档。请不要把完整仓库说明继续堆在这里，避免和其他入口长期漂移。

## 禁止操作

- 禁止格式化磁盘或清理非当前工程目录以外的任何路径，如有需要，向我确认。

## First Read

1. `docs/agent/context.md`
2. `AGENTS.md`
3. `docs/agent/README.md`

## Common Commands

```powershell
cmake -S . -B out/build -G "Visual Studio 17 2022" -A x64 -DQT_ROOT=E:/code/Qt5.15.2/5.15.2/msvc2019_64
cmake --build out/build --config Release --target LASPointCloudViewer LASViewerSmokeTest
.\out\build\bin\Release\LASPointCloudViewer.exe
.\out\build\bin\Release\LASViewerSmokeTest.exe --mode viewer-render --las .\test_data\ezhou_powerline_sample.las
.\out\build\bin\Release\LASViewerSmokeTest.exe --mode route-roam --las .\test_data\ezhou_powerline_sample.las
E:\code\Qt5.15.2\5.15.2\msvc2019_64\bin\lupdate.exe src -ts translations\lasviewer_zh_CN.ts
```

## Where To Dive Deeper

- 模块边界与热文件：
  - `docs/agent/architecture.md`
- 当前功能状态：
  - `docs/agent/product-state.md`
- 常见改动路径、验证和发布：
  - `docs/agent/workflows.md`
