# LAS Point Cloud Viewer

基于 Qt 5.15、OpenSceneGraph 和 LASlib 的桌面点云查看器，支持加载 `.las/.laz` 数据并进行基础浏览、配色和相机切换。

## 目录

- `src/`：应用源码
- `examples/`：示例和 smoke test
- `test_data/`：测试数据与生成脚本
- `shaders/`：渲染资源
- `out/`：推荐的本地构建输出目录，不纳入版本控制

## 构建

推荐使用独立输出目录，避免将编译产物混入源码树：

```powershell
cmake -S . -B out/build -G "Visual Studio 17 2022" -A x64 `
  -DQT_ROOT=E:/code/Qt5.15.2/5.15.2/msvc2019_64 `
  -DOSG_ROOT=E:/code/thirdparty/osg3.6.5 `
  -DQTITAN_ROOT=E:/code/thirdparty/QtitanRibbon `
  -DLASLIB_ROOT=E:/code/thirdparty/laslib-v2.0.4-win64

cmake --build out/build --config Release --target LASPointCloudViewer
cmake --build out/build --config Release --target LASViewerSmokeTest
```

## 运行

```powershell
.\out\build\bin\Release\LASPointCloudViewer.exe
.\out\build\bin\smoke\Release\LASViewerSmokeTest.exe .\test_data\test_pointcloud.las
```

## 测试数据

```powershell
python .\test_data\create_test_las.py
```

## 依赖

- Qt 5.15.2
- OpenSceneGraph 3.6.5
- QtitanRibbon
- LASlib / LASzip
- Visual Studio 2022
- CMake 3.16+
