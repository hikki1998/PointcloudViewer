# LAS Point Cloud Viewer

基于 Qt 5.15、OpenSceneGraph 和 LASlib 的桌面点云查看器，支持加载一个或多个 `.las/.laz` 数据，并围绕电力巡检/通道检查场景提供浏览、标注、量测和剖面分析能力。

当前版本还包含一组更接近测绘点云软件的显示与交互能力，包括：
- 点透明度、深度雾化、EDL 风格增强、圆形 splat
- 视图右上角 `X+ / Y+ / Z+` 坐标轴指示
- 鼠标悬停点坐标显示
- 多点连续量测、右键回退和量测覆盖层
- 净空阈值分析、分段明细表、净空 CSV 导出
- 底部档距剖面视图，支持预警分段高亮和杆塔/隐患叠加
- 杆塔编辑、业务属性维护和隐患台账
- 多数据集工程管理、项目树和工程文件保存/加载

## 目录

- `src/`：应用源码
- `examples/`：示例和 smoke test
- `test_data/`：测试数据与生成脚本
- `shaders/`：渲染资源
- `3rd/`：仓库内随代码分发的精简版 release 三方库
- `out/`：推荐的本地构建输出目录，不纳入版本控制

## 构建

推荐使用独立输出目录，避免将编译产物混入源码树：

```powershell
cmake -S . -B out/build -G "Visual Studio 17 2022" -A x64 `
  -DQT_ROOT=E:/code/Qt5.15.2/5.15.2/msvc2019_64

cmake --build out/build --config Release --target LASPointCloudViewer
cmake --build out/build --config Release --target LASViewerSmokeTest
```

默认会使用仓库内 `3rd/` 作为 `THIRDPARTY_ROOT`。其中包含 `osg/`、`qtitan/`、`laslib/`、`lastools/` 这几个精简版 release 依赖目录，每个目录根下都有 `.version` 标记文件。Qt 不随仓库分发，仍需通过 `QT_ROOT` 单独指定。

## 运行

```powershell
.\out\build\bin\Release\LASPointCloudViewer.exe
.\out\build\bin\smoke\Release\LASViewerSmokeTest.exe .\test_data\ezhou_powerline_sample.las
```

## 开发快速入口

如果是第一次进入这个仓库，建议按下面顺序读取：

1. `PROJECT_CONTEXT.md`：快速了解模块职责、当前功能和验证路径
2. `AGENTS.md`：仓库内开发约定
3. `src/gui/MainWindow.cpp`、`src/gui/PointCloudViewer.cpp`：大多数 UI/交互修改的入口
4. `src/domain/InspectionData.*`、`src/domain/ClearanceAnalysis.*`、`src/gui/ProfilePlotWidget.*`：业务模型、净空分析和剖面图入口
5. `src/osg/OsgPointCloudNode.cpp`、`src/osg/PointCloudVisualization.h`：渲染和显示参数入口

常用验证命令：

```powershell
cmake --build out/build --config Release --target LASPointCloudViewer LASViewerSmokeTest
.\out\build\bin\smoke\Release\LASViewerSmokeTest.exe .\test_data\ezhou_powerline_sample.las
```

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
