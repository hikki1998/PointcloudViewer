# CRS Implementation Tasks

## Goal
- Move CRS from `Inspection Route Planning` to project-level properties.
- Separate project CRS into:
- `pointCloudCrs`: projected/local working CRS for point cloud coordinates
- `geographicCrs`: geographic CRS for lon/lat, default `EPSG:4326`

## Phase 1: Project Metadata Model
- Add a new project-level metadata structure, for example `ProjectCoordinateSystems`.
- Keep route-planning options focused on route generation and DJI export settings only.
- Suggested new files:
- `src/domain/ProjectMetadata.h`
- `src/domain/ProjectMetadata.cpp`
- Suggested data shape:
```cpp
struct CoordinateSystemRef
{
    QString authName = QStringLiteral("EPSG");
    int code = 0;
    QString displayName;
    QString wkt;
};

struct ProjectCoordinateSystems
{
    CoordinateSystemRef pointCloudCrs;
    CoordinateSystemRef geographicCrs { QStringLiteral("EPSG"), 4326, QStringLiteral("WGS 84"), QString() };
};
```
- Acceptance:
- CRS no longer lives inside `RoutePlanningOptions`.
- Main window has a single source of truth for project CRS.

## Phase 2: Project File Migration
- Add `projectProperties.coordinateSystems` to project JSON.
- Keep backward compatibility with old `routePlanning.crs.sourceEpsg`.
- Migration rule:
- If new project CRS exists, use it.
- Else if legacy `routePlanning.crs.sourceEpsg` exists, migrate it to `pointCloudCrs`.
- Else leave `pointCloudCrs` unset and show warning in UI.
- Target files:
- `src/gui/MainWindow.cpp`
- `src/gui/MainWindow.h`
- Acceptance:
- Old project files still open.
- New project files save CRS under project properties.

## Phase 3: UI Entry And Interaction
- Add a dedicated entry in the Ribbon `Workspace` group:
- `Coordinate Systems` or `Project Properties`
- Add a standalone dialog instead of putting CRS inside the analysis panel.
- Suggested new files:
- `src/gui/ProjectCoordinateSystemDialog.h`
- `src/gui/ProjectCoordinateSystemDialog.cpp`
- Dialog sections:
- `Point Cloud CRS`
- `Geographic CRS`
- Each section should support:
- Common CRS dropdown
- Search box
- Manual EPSG input
- Current selection summary
- Reset to default for geographic CRS
- Acceptance:
- User can inspect and edit project CRS without entering route planning.
- Geographic CRS defaults to `EPSG:4326`.

## Phase 4: Common CRS Catalog
- Start with a curated built-in CRS list instead of full-world free browsing UI.
- First batch should include:
- `EPSG:4326` WGS 84
- `EPSG:4490` CGCS2000
- `EPSG:3857` Web Mercator
- Common UTM zones
- Common China projected systems used in practice
- Store a small curated catalog in code or JSON.
- Suggested file:
- `src/domain/CommonCrsCatalog.h`
- `src/domain/CommonCrsCatalog.cpp`
- Acceptance:
- User can choose from practical common CRS without typing EPSG every time.

## Phase 5: LAS Metadata Suggestion
- Parse LAS metadata and surface a suggested CRS candidate.
- Reuse existing `projectionText` extraction as input.
- If projection can be recognized, offer a non-destructive suggestion, not silent auto-overwrite.
- Target files:
- `src/pointcloud/LasReader.h`
- `src/pointcloud/LasReader.cpp`
- `src/gui/MainWindow.cpp`
- Acceptance:
- Loading a dataset with recognizable CRS gives the user a suggestion to adopt as project CRS.

## Phase 6: Route Import/Export Refactor
- Replace `routePlanningOptions_.crs` usage with project-level CRS.
- KML/KMZ import/export should transform between:
- local route points in `pointCloudCrs`
- lon/lat route points in `geographicCrs`
- Target files:
- `src/domain/InspectionRoutePlanning.h`
- `src/domain/InspectionRoutePlanning.cpp`
- `src/domain/RouteInterop.cpp`
- `src/gui/MainWindow.cpp`
- Acceptance:
- Route import/export works without any CRS control in the route-planning panel.

## Phase 7: Project Tree And Details
- Add a project-level item or `Project` group in the project explorer.
- Show current CRS summary in project details dialog.
- Optional project tree items:
- `Coordinate Systems`
- `Project Properties`
- Target files:
- `src/gui/MainWindow.cpp`
- `src/gui/MainWindow.h`
- Acceptance:
- CRS is discoverable from the project tree and clearly presented as an engineering property.

## Phase 8: Validation And Warnings
- Add validation rules:
- Warn if `pointCloudCrs` is unset before route import/export.
- Warn if selected projected CRS looks geographic.
- Warn when multiple loaded datasets appear to have conflicting CRS descriptions.
- Acceptance:
- User gets actionable warnings before export/import errors happen.

## Phase 9: Translation
- Add all new UI texts to:
- `translations/lasviewer_zh_CN.ts`
- Acceptance:
- No new untranslated Chinese UI text remains.

## Phase 10: Build And Dependency Follow-up
- Keep `PROJ` as the coordinate transformation backend.
- Add a future optional `GDAL_ROOT` hook only when code starts calling GDAL APIs.
- Do not introduce GDAL into the link graph before actual usage.
- Acceptance:
- CRS refactor remains scoped and does not bloat the current app prematurely.

## Validation Checklist
- Build:
```powershell
cmake -S . -B out/build -G "Visual Studio 17 2022" -A x64 -DQT_ROOT=E:/code/Qt5.15.2/5.15.2/msvc2019_64
cmake --build out/build --config Release --target LASPointCloudViewer LASViewerSmokeTest
```
- Smoke:
```powershell
.\out\build\bin\Release\LASViewerSmokeTest.exe .\test_data\ezhou_powerline_sample.las
```
- Manual:
- Open old project file and confirm CRS migration.
- Open new project file and confirm CRS persistence.
- Export route KML and DJI KMZ after setting project CRS.
- Import route KML and confirm transformed local coordinates are correct.

## Recommended Delivery Order
1. Phase 1 and Phase 2
2. Phase 3
3. Phase 6
4. Phase 8
5. Phase 5
6. Phase 7 and Phase 9

## Dependency Notes
- `PROJ` remains the required transform engine.
- Current `E:\code\thirdparty\gdal` is prepared as an external runtime/tooling package for later GIS work.
- If future code needs direct C/C++ GDAL linking, add compile-time headers/import libraries at that time instead of coupling them into this CRS refactor early.
