# CLAUDE.md

本文件为 Claude Code 兼容入口。**完整约束与规范请参见 [AGENTS.md](AGENTS.md)**，本文件仅保留常用命令速查。

## 禁止操作

- 禁止格式化磁盘或清理非当前工程目录以外的任何路径，如有需要，向我确认。

## 常用命令

```powershell
# 配置
cmake -S . -B out/build -G "Visual Studio 17 2022" -A x64 -DQT_ROOT=E:/code/Qt5.15.2/5.15.2/msvc2019_64

# 构建
cmake --build out/build --config Release --target LASPointCloudViewer LASViewerSmokeTest

# 主程序
.\out\build\bin\Release\LASPointCloudViewer.exe

# smoke test
.\out\build\bin\Release\LASViewerSmokeTest.exe --mode viewer-render --las .\test_data\ezhou_powerline_sample.las
.\out\build\bin\Release\LASViewerSmokeTest.exe --mode route-roam --las .\test_data\ezhou_powerline_sample.las
.\out\build\bin\Release\LASViewerSmokeTest.exe --mode main-backstage

# 翻译
E:\code\Qt5.15.2\5.15.2\msvc2019_64\bin\lupdate.exe src -ts translations\lasviewer_zh_CN.ts
```
