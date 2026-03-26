# Repository Guidelines

## Project Structure & Module Organization
`src/` contains the application code. Keep UI shell code in `src/gui/`, point-cloud parsing and data models in `src/pointcloud/`, and OpenSceneGraph rendering code in `src/osg/`. `examples/` holds developer utilities such as `viewer_smoke_test.cpp` and `laslib_read_example.cpp`. Test assets and generators live in `test_data/`; `shaders/` is reserved for rendering assets. `3rd/` contains the vendored release-only third-party subset used by default builds. Keep compiled output out of the source tree where possible; use `out/build/` for local builds. Treat `build/`, `build-test/`, `out/`, and `thirdparty_builds/` as generated or local-only space, not source.

## Build, Test, and Development Commands
Configure with Visual Studio 2022 and explicit dependency roots:
```powershell
cmake -S . -B out/build -G "Visual Studio 17 2022" -A x64 -DQT_ROOT=E:/code/Qt5.15.2/5.15.2/msvc2019_64
cmake --build out/build --config Release --target LASPointCloudViewer
cmake --build out/build --config Release --target LASViewerSmokeTest
```
Run the app with `.\out\build\bin\Release\LASPointCloudViewer.exe`. Run the smoke test with `.\out\build\bin\smoke\Release\LASViewerSmokeTest.exe .\test_data\test_pointcloud.las`. Regenerate sample data with `python .\test_data\create_test_las.py`. By default, CMake resolves non-Qt dependencies from `3rd/` using stable names such as `osg/`, `qtitan/`, `laslib/`, and `lastools/`, each with a `.version` marker file at the package root.

## Coding Style & Naming Conventions
Use C++17 and follow the existing Qt/OSG style in `src/`: 4-space indentation, function braces on the next line, and braces on the same line for control statements. Group includes by Qt/third-party/project headers with blank lines between groups. Use `PascalCase` for classes (`PointCloudViewer`), `lowerCamelCase` for functions, `kPrefix` for file-local constants, and trailing underscores for member fields.

## Testing Guidelines
There is no separate unit-test framework checked in; coverage is currently enforced through deterministic smoke testing. Add new executable checks under `examples/` when touching rendering or file loading, and keep test inputs small and reproducible. Prefer fixed seeds and committed `.las` fixtures in `test_data/`.

## Commit & Pull Request Guidelines
Git history is not available in this workspace snapshot, so follow a conservative convention: short imperative commit titles such as `Add LAZ drag-and-drop validation`. Keep commits scoped to one concern. PRs should state the dataset used for validation, list changed modules, and include screenshots for UI or rendering changes. Call out any new dependency path assumptions or CMake cache variables.

## Configuration Tips
Avoid hardcoding machine-specific paths beyond CMake cache defaults. Prefer the repo-local `3rd/` payload for bundled dependencies and `-DQT_ROOT=` for Qt. Keep version-specific folder names out of CMake inputs; upgrade bundled dependencies by replacing the contents of stable directories and updating their `.version` marker files.
