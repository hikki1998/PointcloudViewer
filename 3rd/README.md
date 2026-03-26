# Bundled Third-Party Layout

This directory contains the minimal non-Qt dependencies required to build the project in `Release`.

Included packages:

- `osg/`: release headers, import libraries, and runtime DLLs needed by this viewer
- `qtitan/`: release headers plus `qtnribbon4.dll/.lib`
- `laslib/`: release headers and `LASlib64.lib` / `laszip64.lib`
- `lastools/`: only `LASzip/src/`, which is required by LASlib headers

Not included:

- Qt: keep using `-DQT_ROOT=<Qt 5.15.2 msvc2019_64 root>`
- PCL: optional and still expected from an external path when enabled
- Debug libraries, PDB files, examples, installers, and unrelated tools

Default configure command:

```powershell
cmake -S . -B out/build -G "Visual Studio 17 2022" -A x64 -DQT_ROOT=E:/code/Qt5.15.2/5.15.2/msvc2019_64
```
