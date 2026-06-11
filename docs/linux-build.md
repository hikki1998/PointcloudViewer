# Linux Build Guide

This project can be built on Ubuntu 22.04 with Qt 5, OpenSceneGraph, PROJ/GDAL, and LASzip. The verified setup is WSL2 Ubuntu 22.04, but the same package set also applies to a normal Ubuntu 22.04 desktop.

## 1. Install Dependencies

From the repository root inside Ubuntu:

```bash
bash scripts/linux/setup-ubuntu-22.04.sh
```

The script installs the compiler, CMake/Ninja, Qt 5 development packages, OpenSceneGraph, PROJ/GDAL, LASzip, OpenGL/XCB support, and Qt Wayland support.

## 2. Configure

Recommended out-of-tree build:

```bash
cmake -S . -B out/linux/build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DQT_ROOT=/usr \
  -DTHIRDPARTY_ROOT="$PWD/3rd" \
  -DPROJ_ROOT=/usr \
  -DLAS_VIEWER_ENABLE_WINDOWS_CAPTURE=OFF
```

On non-Windows platforms, the default LAS reader uses the system LASzip C API:

```text
LAS_VIEWER_ENABLE_LASZIP_API=ON
LAS_VIEWER_ENABLE_LASLIB=OFF
```

On non-Windows platforms, CMake also defaults to the repository QtitanRibbon compatibility shim when `QTITAN_ROOT` is not provided:

```text
LAS_VIEWER_USE_BUNDLED_QTITAN_SHIM=ON
tools/linux/qtitan_shim/
```

This shim is a compatibility layer for the subset of QtitanRibbon APIs used by this application. It is intended for Linux builds and is not a full replacement for the commercial QtitanRibbon package.

## 3. Build

```bash
cmake --build out/linux/build --target LASPointCloudViewer LASViewerSmokeTest -j 4
```

Build outputs are written to:

```text
out/linux/build/bin/
```

## 4. Run

```bash
cd out/linux/build/bin
./LASPointCloudViewer
```

If running under WSLg and the window does not appear correctly, try forcing the Qt Wayland platform:

```bash
QT_QPA_PLATFORM=wayland ./LASPointCloudViewer
```

## 5. Smoke Tests

```bash
cd out/linux/build/bin
./LASViewerSmokeTest --mode main-backstage
./LASViewerSmokeTest --mode viewer-render --las ../../../../test_data/ezhou_powerline_sample.las
./LASViewerSmokeTest --mode route-roam --las ../../../../test_data/ezhou_powerline_sample.las
```

Adjust the LAS path if your build directory is different.

## 6. Optional Standalone Qtitan Shim Install

The main project can build the shim automatically. A standalone install is only needed if you want to provide `QTITAN_ROOT` manually:

```bash
bash scripts/linux/build-qtitan-shim.sh out/linux/qtitan_shim out/linux/thirdparty/qtitan
```

Then configure with:

```bash
cmake -S . -B out/linux/build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DQT_ROOT=/usr \
  -DTHIRDPARTY_ROOT="$PWD/3rd" \
  -DQTITAN_ROOT="$PWD/out/linux/thirdparty/qtitan" \
  -DPROJ_ROOT=/usr \
  -DLAS_VIEWER_ENABLE_WINDOWS_CAPTURE=OFF
```

## 7. Package A Linux Release

After a release build, create the Linux x64 package with:

```bash
bash scripts/linux/package-release.sh v1.3.0 out/linux/build/bin out/release
```

The package contains the main executable, the bundled QtitanRibbon shim library, translations, a launch script, and this Linux build guide.

## WSL Path Notes

Windows drives are mounted under `/mnt`:

```text
E:\data\cloud.las  ->  /mnt/e/data/cloud.las
C:\Users\win       ->  /mnt/c/Users/win
```

The Linux application can open LAS/LAZ, route JSON, project files, and exports directly from `/mnt/c`, `/mnt/d`, `/mnt/e`, etc. For very large point clouds or repeated processing, copying data into the Linux filesystem can be faster than reading from `/mnt`.
