# Bundled Third-Party Layout

This directory contains the minimal non-Qt dependencies required to build the project in `Release`.

Included packages:

- `osg/`: release headers, import libraries, and runtime DLLs needed by this viewer
- `qtitan/`: release headers plus `qtnribbon4.dll/.lib`
- `gdal/`: minimal GDAL/PROJ bundle for current CRS features; includes `include/gdal.h`, `include/proj9/`, `lib/proj9.lib`, `bin/proj_9.dll` and its runtime DLLs, plus `bin/proj9/share`
- `laslib/`: release headers and `LASlib64.lib` / `laszip64.lib`
- `lastools/`: only `LASzip/src/`, which is required by LASlib headers

Not included:

- Qt: keep using `-DQT_ROOT=<Qt 5.15.2 msvc2019_64 root>`
- PCL: optional and still expected from an external path when enabled
- Full GDAL API is not linked into the app yet; the bundled `gdal/` directory is intentionally trimmed to the pieces needed by current PROJ-based CRS discovery, transform, and runtime data deployment
- Debug libraries, PDB files, examples, installers, and unrelated tools

Default configure command:

```powershell
cmake -S . -B out/build -G "Visual Studio 17 2022" -A x64 -DQT_ROOT=E:/code/Qt5.15.2/5.15.2/msvc2019_64
```
