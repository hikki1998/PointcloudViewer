cmake_minimum_required(VERSION 3.16)

project(LASPointCloudViewer VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(MSVC)
    list(APPEND CMAKE_VS_GLOBALS
        UseMultiToolTask=true
        EnforceProcessCountAcrossBuilds=true
    )
endif()

option(LAS_VIEWER_ENABLE_PCL "Enable PCL integration" OFF)
option(LAS_VIEWER_ENABLE_LASLIB "Enable LASlib-based LAS/LAZ reader" ON)
option(LAS_VIEWER_ENABLE_PROJ "Enable PROJ-based CRS transformation" ON)

set(THIRDPARTY_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/3rd" CACHE PATH "Root directory containing project third-party dependencies")
set(QT_ROOT "E:/env/qt/5.15.2" CACHE PATH "Qt installation root")
set(OSG_ROOT "" CACHE PATH "Legacy override for OpenSceneGraph installation root")
set(QTITAN_ROOT "" CACHE PATH "Legacy override for QtitanRibbon package root")
set(GDAL_ROOT "" CACHE PATH "Legacy override for GDAL SDK/runtime root")
set(PCL_ROOT "" CACHE PATH "Legacy override for PCL installation root")
set(LASLIB_ROOT "" CACHE PATH "Legacy override for LASlib package root")
set(LASTOOLS_ROOT "" CACHE PATH "Legacy override for LAStools package root")
set(LASZIP_ROOT "" CACHE PATH "Legacy override for LASzip source root")
set(PROJ_ROOT "" CACHE PATH "Optional PROJ installation root")

function(las_viewer_read_version_marker marker_file out_name out_version)
    set(package_name "")
    set(package_version "")

    if(EXISTS "${marker_file}")
        file(STRINGS "${marker_file}" marker_lines)
        foreach(marker_line IN LISTS marker_lines)
            if(marker_line MATCHES "^NAME=(.+)$")
                set(package_name "${CMAKE_MATCH_1}")
            elseif(marker_line MATCHES "^VERSION=(.+)$")
                set(package_version "${CMAKE_MATCH_1}")
            endif()
        endforeach()
    endif()

    set(${out_name} "${package_name}" PARENT_SCOPE)
    set(${out_version} "${package_version}" PARENT_SCOPE)
endfunction()

function(las_viewer_resolve_package_root out_var package_label stable_dir)
    set(options)
    set(one_value_args LEGACY_ROOT)
    set(multi_value_args REQUIRED_PATHS)
    cmake_parse_arguments(LAS_VIEWER "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})

    set(candidate_roots)
    if(LAS_VIEWER_LEGACY_ROOT)
        list(APPEND candidate_roots "${LAS_VIEWER_LEGACY_ROOT}")
    endif()
    list(APPEND candidate_roots "${THIRDPARTY_ROOT}/${stable_dir}")
    list(REMOVE_DUPLICATES candidate_roots)

    set(validation_failures)
    foreach(candidate_root IN LISTS candidate_roots)
        if(NOT candidate_root OR NOT IS_DIRECTORY "${candidate_root}")
            continue()
        endif()

        set(missing_entries)
        if(candidate_root STREQUAL "${THIRDPARTY_ROOT}/${stable_dir}" AND NOT EXISTS "${candidate_root}/.version")
            list(APPEND missing_entries ".version")
        endif()

        foreach(required_path IN LISTS LAS_VIEWER_REQUIRED_PATHS)
            if(NOT EXISTS "${candidate_root}/${required_path}")
                list(APPEND missing_entries "${required_path}")
            endif()
        endforeach()

        if(NOT missing_entries)
            if(EXISTS "${candidate_root}/.version")
                las_viewer_read_version_marker("${candidate_root}/.version" package_name package_version)
                if(package_name OR package_version)
                    message(STATUS "Using ${package_label}: ${candidate_root} (${package_name} ${package_version})")
                else()
                    message(STATUS "Using ${package_label}: ${candidate_root}")
                endif()
            else()
                message(STATUS "Using ${package_label}: ${candidate_root} (legacy override)")
            endif()

            set(${out_var} "${candidate_root}" PARENT_SCOPE)
            return()
        endif()

        list(JOIN missing_entries ", " missing_summary)
        list(APPEND validation_failures "${candidate_root} [missing: ${missing_summary}]")
    endforeach()

    if(validation_failures)
        string(JOIN "\n  " validation_report ${validation_failures})
        message(FATAL_ERROR
            "Unable to locate ${package_label}.\n"
            "Checked:\n  ${validation_report}\n"
            "Expected stable directory: ${THIRDPARTY_ROOT}/${stable_dir}"
        )
    endif()

    message(FATAL_ERROR
        "Unable to locate ${package_label}.\n"
        "Expected stable directory: ${THIRDPARTY_ROOT}/${stable_dir}\n"
        "Required entries: ${LAS_VIEWER_REQUIRED_PATHS} and .version"
    )
endfunction()

if(NOT EXISTS "${QT_ROOT}/lib/cmake/Qt5")
    if(EXISTS "${QT_ROOT}/msvc2019_64/lib/cmake/Qt5")
        set(QT_ROOT "${QT_ROOT}/msvc2019_64" CACHE PATH "Qt installation root" FORCE)
    elseif(EXISTS "${QT_ROOT}/msvc2022_64/lib/cmake/Qt5")
        set(QT_ROOT "${QT_ROOT}/msvc2022_64" CACHE PATH "Qt installation root" FORCE)
    elseif(EXISTS "E:/env/qt/5.15.2/msvc2019_64/lib/cmake/Qt5")
        set(QT_ROOT "E:/env/qt/5.15.2/msvc2019_64" CACHE PATH "Qt installation root" FORCE)
    endif()
endif()

if(NOT EXISTS "${QT_ROOT}/lib/cmake/Qt5")
    message(FATAL_ERROR
        "Qt5 was not found under QT_ROOT='${QT_ROOT}'. "
        "Expected '${QT_ROOT}/lib/cmake/Qt5'."
    )
endif()

if(LASZIP_ROOT AND NOT LASTOOLS_ROOT)
    get_filename_component(derived_lastools_root "${LASZIP_ROOT}" DIRECTORY)
    set(LASTOOLS_ROOT "${derived_lastools_root}" CACHE PATH "Legacy override for LAStools package root" FORCE)
endif()

las_viewer_resolve_package_root(RESOLVED_OSG_ROOT "OpenSceneGraph" "osg"
    LEGACY_ROOT "${OSG_ROOT}"
    REQUIRED_PATHS "include/osg/Node" "lib" "bin"
)
set(OSG_ROOT "${RESOLVED_OSG_ROOT}" CACHE PATH "OpenSceneGraph installation root" FORCE)

las_viewer_resolve_package_root(RESOLVED_QTITAN_ROOT "QtitanRibbon" "qtitan"
    LEGACY_ROOT "${QTITAN_ROOT}"
    REQUIRED_PATHS "include/QtitanRibbon.h" "bin"
)
set(QTITAN_ROOT "${RESOLVED_QTITAN_ROOT}" CACHE PATH "QtitanRibbon package root" FORCE)

set(default_gdal_root "${THIRDPARTY_ROOT}/gdal")
if(GDAL_ROOT OR EXISTS "${default_gdal_root}")
    las_viewer_resolve_package_root(RESOLVED_GDAL_ROOT "GDAL SDK" "gdal"
        LEGACY_ROOT "${GDAL_ROOT}"
        REQUIRED_PATHS "include/gdal.h" "lib" "bin"
    )
    set(GDAL_ROOT "${RESOLVED_GDAL_ROOT}" CACHE PATH "GDAL SDK/runtime root" FORCE)
endif()

if((NOT PROJ_ROOT
    OR (NOT EXISTS "${PROJ_ROOT}/include/proj9/proj.h" AND NOT EXISTS "${PROJ_ROOT}/include/proj.h"))
    AND GDAL_ROOT
    AND EXISTS "${GDAL_ROOT}/include/proj9/proj.h"
    AND EXISTS "${GDAL_ROOT}/lib/proj9.lib")
    set(PROJ_ROOT "${GDAL_ROOT}" CACHE PATH "Optional PROJ installation root" FORCE)
endif()

if(LAS_VIEWER_ENABLE_PCL)
    las_viewer_resolve_package_root(RESOLVED_PCL_ROOT "PCL" "pcl"
        LEGACY_ROOT "${PCL_ROOT}"
        REQUIRED_PATHS "include" "cmake"
    )
    set(PCL_ROOT "${RESOLVED_PCL_ROOT}" CACHE PATH "PCL installation root" FORCE)
endif()

if(LAS_VIEWER_ENABLE_LASLIB)
    las_viewer_resolve_package_root(RESOLVED_LASLIB_ROOT "LASlib" "laslib"
        LEGACY_ROOT "${LASLIB_ROOT}"
        REQUIRED_PATHS "include/laslib/lasreader.hpp" "include/laszip/laszip_common.h" "lib"
    )
    set(LASLIB_ROOT "${RESOLVED_LASLIB_ROOT}" CACHE PATH "LASlib package root" FORCE)

    las_viewer_resolve_package_root(RESOLVED_LASTOOLS_ROOT "LAStools" "lastools"
        LEGACY_ROOT "${LASTOOLS_ROOT}"
        REQUIRED_PATHS "LASzip/src/laszip.hpp"
    )
    set(LASTOOLS_ROOT "${RESOLVED_LASTOOLS_ROOT}" CACHE PATH "LAStools package root" FORCE)

    set(default_laszip_root "${LASTOOLS_ROOT}/LASzip")
    if(LASZIP_ROOT AND EXISTS "${LASZIP_ROOT}/src/laszip.hpp")
        set(LASZIP_ROOT "${LASZIP_ROOT}" CACHE PATH "LASzip source root" FORCE)
    elseif(EXISTS "${default_laszip_root}/src/laszip.hpp")
        if(LASZIP_ROOT AND NOT LASZIP_ROOT STREQUAL "${default_laszip_root}")
            message(STATUS
                "Ignoring invalid LASZIP_ROOT='${LASZIP_ROOT}' and using '${default_laszip_root}' instead."
            )
        endif()
        set(LASZIP_ROOT "${default_laszip_root}" CACHE PATH "LASzip source root" FORCE)
    else()
        message(FATAL_ERROR
            "LASzip source root was not found.\n"
            "Checked LASZIP_ROOT='${LASZIP_ROOT}' and expected '${default_laszip_root}/src/laszip.hpp'."
        )
    endif()
endif()

list(PREPEND CMAKE_PREFIX_PATH
    "${QT_ROOT}"
    "${QT_ROOT}/lib/cmake"
)

if(LAS_VIEWER_ENABLE_PCL)
    list(PREPEND CMAKE_PREFIX_PATH
        "${PCL_ROOT}"
        "${PCL_ROOT}/cmake"
    )
endif()

foreach(cached_dependency_var
    QTITAN_INCLUDE_DIR
    QTITAN_RELEASE_LIBRARY
    QTITAN_DEBUG_LIBRARY
    LASLIB_INCLUDE_DIR
    LASZIP_INCLUDE_DIR
    LASZIP_SRC_INCLUDE_DIR
    LASLIB_LIBRARY
    LASZIP_LIBRARY
    LASLIB_DEBUG_LIBRARY
    LASZIP_DEBUG_LIBRARY
    OSG_INCLUDE_DIR
    OPENTHREADS_RELEASE_LIBRARY
    OPENTHREADS_DEBUG_LIBRARY
    OSG_RELEASE_LIBRARY
    OSG_DEBUG_LIBRARY
    OSGDB_RELEASE_LIBRARY
    OSGDB_DEBUG_LIBRARY
    OSGGA_RELEASE_LIBRARY
    OSGGA_DEBUG_LIBRARY
    OSGVIEWER_RELEASE_LIBRARY
    OSGVIEWER_DEBUG_LIBRARY
    OSGUTIL_RELEASE_LIBRARY
    OSGUTIL_DEBUG_LIBRARY
    PROJ_INCLUDE_DIR
    PROJ_LIBRARY
)
    unset(${cached_dependency_var} CACHE)
endforeach()

find_package(Qt5 5.15 REQUIRED COMPONENTS Core Gui Widgets OpenGL)

find_path(QTITAN_INCLUDE_DIR
    NAMES QtitanRibbon.h
    PATHS "${QTITAN_ROOT}/include"
    NO_DEFAULT_PATH
)

find_library(QTITAN_RELEASE_LIBRARY
    NAMES qtnribbon4
    PATHS "${QTITAN_ROOT}/bin"
    NO_DEFAULT_PATH
)

find_library(QTITAN_DEBUG_LIBRARY
    NAMES qtnribbond4
    PATHS "${QTITAN_ROOT}/bin"
    NO_DEFAULT_PATH
)

if(LAS_VIEWER_ENABLE_PCL)
    find_package(PCL 1.8 QUIET COMPONENTS common io)
endif()

if(LAS_VIEWER_ENABLE_LASLIB)
    find_path(LASLIB_INCLUDE_DIR
        NAMES lasreader.hpp
        PATHS "${LASLIB_ROOT}/include/laslib"
        NO_DEFAULT_PATH
    )

    find_path(LASZIP_INCLUDE_DIR
        NAMES laszip_common.h
        PATHS
            "${LASLIB_ROOT}/include/laszip"
            "${LASZIP_ROOT}/include/laszip"
        NO_DEFAULT_PATH
    )

    find_path(LASZIP_SRC_INCLUDE_DIR
        NAMES laszip.hpp
        PATHS "${LASZIP_ROOT}/src"
        NO_DEFAULT_PATH
    )

    find_library(LASLIB_LIBRARY
        NAMES LASlib64
        PATHS "${LASLIB_ROOT}/lib"
        NO_DEFAULT_PATH
    )

    find_library(LASZIP_LIBRARY
        NAMES laszip64
        PATHS "${LASLIB_ROOT}/lib"
        NO_DEFAULT_PATH
    )

    find_library(LASLIB_DEBUG_LIBRARY
        NAMES LASlib64d LASlibd
        PATHS "${LASLIB_ROOT}/lib"
        NO_DEFAULT_PATH
    )

    find_library(LASZIP_DEBUG_LIBRARY
        NAMES laszip64d laszipd
        PATHS "${LASLIB_ROOT}/lib"
        NO_DEFAULT_PATH
    )
endif()

find_path(OSG_INCLUDE_DIR
    NAMES osg/Node
    PATHS "${OSG_ROOT}/include"
    NO_DEFAULT_PATH
)

find_library(OPENTHREADS_RELEASE_LIBRARY
    NAMES OpenThreads
    PATHS "${OSG_ROOT}/lib"
    NO_DEFAULT_PATH
)

find_library(OPENTHREADS_DEBUG_LIBRARY
    NAMES OpenThreadsd
    PATHS "${OSG_ROOT}/lib"
    NO_DEFAULT_PATH
)

find_library(OSG_RELEASE_LIBRARY
    NAMES osg
    PATHS "${OSG_ROOT}/lib"
    NO_DEFAULT_PATH
)

find_library(OSG_DEBUG_LIBRARY
    NAMES osgd
    PATHS "${OSG_ROOT}/lib"
    NO_DEFAULT_PATH
)

find_library(OSGDB_RELEASE_LIBRARY
    NAMES osgDB
    PATHS "${OSG_ROOT}/lib"
    NO_DEFAULT_PATH
)

find_library(OSGDB_DEBUG_LIBRARY
    NAMES osgDBd
    PATHS "${OSG_ROOT}/lib"
    NO_DEFAULT_PATH
)

find_library(OSGGA_RELEASE_LIBRARY
    NAMES osgGA
    PATHS "${OSG_ROOT}/lib"
    NO_DEFAULT_PATH
)

find_library(OSGGA_DEBUG_LIBRARY
    NAMES osgGAd
    PATHS "${OSG_ROOT}/lib"
    NO_DEFAULT_PATH
)

find_library(OSGVIEWER_RELEASE_LIBRARY
    NAMES osgViewer
    PATHS "${OSG_ROOT}/lib"
    NO_DEFAULT_PATH
)

find_library(OSGVIEWER_DEBUG_LIBRARY
    NAMES osgViewerd
    PATHS "${OSG_ROOT}/lib"
    NO_DEFAULT_PATH
)

find_library(OSGUTIL_RELEASE_LIBRARY
    NAMES osgUtil
    PATHS "${OSG_ROOT}/lib"
    NO_DEFAULT_PATH
)

find_library(OSGUTIL_DEBUG_LIBRARY
    NAMES osgUtild
    PATHS "${OSG_ROOT}/lib"
    NO_DEFAULT_PATH
)

set(LAS_VIEWER_HAS_PROJ OFF)
if(LAS_VIEWER_ENABLE_PROJ)
    find_path(PROJ_INCLUDE_DIR
        NAMES proj.h
        PATHS
            "${PROJ_ROOT}/include"
            "${PROJ_ROOT}/include/proj9"
            "${THIRDPARTY_ROOT}/proj/include"
            "${THIRDPARTY_ROOT}/proj/include/proj9"
    )

    find_library(PROJ_LIBRARY
        NAMES proj proj9 proj_9 proj_9_6 proj_9_5 proj_9_4 proj_9_3 proj_9_2 proj_9_1 proj_9_0
        PATHS
            "${PROJ_ROOT}/lib"
            "${THIRDPARTY_ROOT}/proj/lib"
    )

    if(PROJ_INCLUDE_DIR AND PROJ_LIBRARY)
        set(LAS_VIEWER_HAS_PROJ ON)
        message(STATUS "Using PROJ: ${PROJ_LIBRARY}")
    else()
        message(WARNING
            "PROJ was enabled but not found. CRS transform will be disabled. "
            "Set PROJ_ROOT to a valid installation root to enable it."
        )
    endif()
endif()

set(LAS_VIEWER_FORCE_RELEASE_THIRDPARTY_FOR_DEBUG OFF)
if(MSVC AND (
    NOT QTITAN_DEBUG_LIBRARY OR
    NOT OPENTHREADS_DEBUG_LIBRARY OR
    NOT OSG_DEBUG_LIBRARY OR
    NOT OSGDB_DEBUG_LIBRARY OR
    NOT OSGGA_DEBUG_LIBRARY OR
    NOT OSGVIEWER_DEBUG_LIBRARY OR
    NOT OSGUTIL_DEBUG_LIBRARY OR
    (LAS_VIEWER_ENABLE_LASLIB AND (NOT LASLIB_DEBUG_LIBRARY OR NOT LASZIP_DEBUG_LIBRARY))
))
    set(LAS_VIEWER_FORCE_RELEASE_THIRDPARTY_FOR_DEBUG ON)
endif()

if(LAS_VIEWER_FORCE_RELEASE_THIRDPARTY_FOR_DEBUG)
    set(OSG_LIBRARIES
        "${OPENTHREADS_RELEASE_LIBRARY}"
        "${OSG_RELEASE_LIBRARY}"
        "${OSGDB_RELEASE_LIBRARY}"
        "${OSGGA_RELEASE_LIBRARY}"
        "${OSGVIEWER_RELEASE_LIBRARY}"
        "${OSGUTIL_RELEASE_LIBRARY}"
    )

    foreach(qt_target Qt5::Core Qt5::Gui Qt5::Widgets Qt5::OpenGL)
        set_property(TARGET ${qt_target} PROPERTY MAP_IMPORTED_CONFIG_DEBUG Release)
    endforeach()
else()
    set(OSG_LIBRARIES
        optimized "${OPENTHREADS_RELEASE_LIBRARY}"
        debug "${OPENTHREADS_DEBUG_LIBRARY}"
        optimized "${OSG_RELEASE_LIBRARY}"
        debug "${OSG_DEBUG_LIBRARY}"
        optimized "${OSGDB_RELEASE_LIBRARY}"
        debug "${OSGDB_DEBUG_LIBRARY}"
        optimized "${OSGGA_RELEASE_LIBRARY}"
        debug "${OSGGA_DEBUG_LIBRARY}"
        optimized "${OSGVIEWER_RELEASE_LIBRARY}"
        debug "${OSGVIEWER_DEBUG_LIBRARY}"
        optimized "${OSGUTIL_RELEASE_LIBRARY}"
        debug "${OSGUTIL_DEBUG_LIBRARY}"
    )
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OpenSceneGraph
    REQUIRED_VARS
        OSG_INCLUDE_DIR
        OPENTHREADS_RELEASE_LIBRARY
        OSG_RELEASE_LIBRARY
        OSGDB_RELEASE_LIBRARY
        OSGGA_RELEASE_LIBRARY
        OSGVIEWER_RELEASE_LIBRARY
        OSGUTIL_RELEASE_LIBRARY
)

find_package_handle_standard_args(QtitanRibbon
    REQUIRED_VARS QTITAN_INCLUDE_DIR QTITAN_RELEASE_LIBRARY
)

if(LAS_VIEWER_ENABLE_LASLIB)
    find_package_handle_standard_args(LASlib
        REQUIRED_VARS LASLIB_INCLUDE_DIR LASZIP_INCLUDE_DIR LASZIP_SRC_INCLUDE_DIR LASLIB_LIBRARY LASZIP_LIBRARY
    )
endif()

set(SOURCES
    src/main.cpp
    resources.qrc
    src/crs/CrsCatalogService.cpp
    src/crs/CrsAuthorityService.cpp
    src/crs/CrsProjRuntime.cpp
    src/crs/CrsSelectionDialog.cpp
    src/crs/CrsTransformService.cpp
    src/crs/CrsTypes.cpp
    src/crs/ProjectCoordinateSystemsDialog.cpp
    src/crs/RecentCrsStore.cpp
    src/domain/AnalysisData.cpp
    src/domain/ClassificationEditStore.cpp
    src/domain/ClearanceAnalysis.cpp
    src/domain/ClearanceReportExporter.cpp
    src/domain/DataManager.cpp
    src/domain/InspectionData.cpp
    src/domain/InspectionReportExporter.cpp
    src/domain/ProfileMarkerProjection.cpp
    src/domain/RuleBasedClearanceEngine.cpp
    src/domain/TowerFileInterop.cpp
    src/domain/VegetationRiskAnalysis.cpp
    src/gui/ApplicationLogDock.cpp
    src/gui/BackstageAboutWidget.cpp
    src/gui/BackstageAboutWidget.ui
    src/gui/BackstageApplicationSettingsWidget.cpp
    src/gui/BackstageApplicationSettingsWidget.ui
    src/gui/BackstageOpenActionsWidget.cpp
    src/gui/BackstageOpenActionsWidget.ui
    src/gui/BackstageOpenProjectWidget.cpp
    src/gui/BackstageOpenProjectWidget.ui
    src/gui/BackstagePageHeaderWidget.cpp
    src/gui/BackstagePageHeaderWidget.ui
    src/gui/BackstageProjectPropertiesWidget.cpp
    src/gui/BackstageProjectPropertiesWidget.ui
    src/gui/DatasetSummaryWidget.cpp
    src/gui/DatasetSummaryWidget.ui
    src/gui/IssueController.cpp
    src/gui/IssueEditorWidget.cpp
    src/gui/IssueEditorWidget.ui
    src/gui/MainWindow.cpp
    src/gui/MeasurementAnalysisController.cpp
    src/gui/NavigationSettingsWidget.cpp
    src/gui/NavigationSettingsWidget.ui
    src/gui/PointCloudViewer.cpp
    src/gui/ProfileClassificationController.cpp
    src/gui/ProfileClassificationDock.cpp
    src/gui/ProfileClassificationWidget.cpp
    src/gui/ProfileClassificationWidget.ui
    src/gui/ProjectExplorerController.cpp
    src/gui/ProjectExplorerDock.cpp
    src/gui/RouteController.cpp
    src/gui/RouteDetailsDock.cpp
    src/gui/SceneInspectorDock.cpp
    src/gui/SpanProfileDock.cpp
    src/gui/TowerController.cpp
    src/gui/TowerEditorWidget.cpp
    src/gui/TowerEditorWidget.ui
    src/gui/UiHistoryStore.cpp
    src/gui/VisualizationPanelController.cpp
    src/gui/support/RibbonIconFactory.cpp
    src/gui/support/UiHelpers.cpp
    src/gui/ProfilePlotWidget.cpp
    src/logging/ApplicationLogger.cpp
    src/pointcloud/LasReader.cpp
    src/pointcloud/PointCloudData.cpp
    src/pointcloud/PointCloudTileStore.cpp
    src/osg/OsgPointCloudNode.cpp
    src/route/InspectionRoutePlanning.cpp
    src/route/PowerlineRouteBridge.cpp
    src/route/PowerlineRouteJson.cpp
    src/route/RouteInterop.cpp
)

if(WIN32)
    list(APPEND SOURCES src/app_icon.rc)
endif()

set(HEADERS
    src/crs/CrsCatalogService.h
    src/crs/CrsAuthorityService.h
    src/crs/CrsProjRuntime.h
    src/crs/CrsSelectionDialog.h
    src/crs/CrsTransformService.h
    src/crs/CrsTypes.h
    src/crs/LASViewerCrsExport.h
    src/crs/ProjectCoordinateSystemsDialog.h
    src/crs/RecentCrsStore.h
    src/domain/AnalysisData.h
    src/domain/ClassificationEditStore.h
    src/domain/ClearanceAnalysis.h
    src/domain/ClearanceReportExporter.h
    src/domain/DataManager.h
    src/domain/InspectionData.h
    src/domain/InspectionReportExporter.h
    src/domain/ProjectMetadata.h
    src/domain/ProfileMarkerProjection.h
    src/domain/RuleBasedClearanceEngine.h
    src/domain/TowerFileInterop.h
    src/domain/VegetationRiskAnalysis.h
    src/gui/ApplicationLogDock.h
    src/gui/BackstageAboutWidget.h
    src/gui/BackstageApplicationSettingsWidget.h
    src/gui/BackstageOpenActionsWidget.h
    src/gui/BackstageOpenProjectWidget.h
    src/gui/BackstagePageHeaderWidget.h
    src/gui/BackstageProjectPropertiesWidget.h
    src/gui/DatasetSummaryWidget.h
    src/gui/IssueController.h
    src/gui/IssueEditorWidget.h
    src/gui/MainWindow.h
    src/gui/MeasurementAnalysisController.h
    src/gui/NavigationSettingsWidget.h
    src/gui/PointCloudViewer.h
    src/gui/ProfileClassificationController.h
    src/gui/ProfileClassificationDock.h
    src/gui/ProfileClassificationWidget.h
    src/gui/ProjectExplorerController.h
    src/gui/ProjectExplorerDock.h
    src/gui/RouteController.h
    src/gui/RouteDetailsDock.h
    src/gui/SceneInspectorDock.h
    src/gui/SpanProfileDock.h
    src/gui/TowerController.h
    src/gui/TowerEditorWidget.h
    src/gui/VisualizationPanelController.h
    src/gui/ProfilePlotWidget.h
    src/gui/support/RibbonIconFactory.h
    src/gui/support/SettingsKeys.h
    src/gui/support/UiHelpers.h
    src/gui/UiHistoryStore.h
    src/logging/ApplicationLogger.h
    src/pointcloud/LasReader.h
    src/pointcloud/PointCloudData.h
    src/pointcloud/PointCloudTileStore.h
    src/osg/PointCloudVisualization.h
    src/osg/OsgPointCloudNode.h
    src/route/InspectionRoutePlanning.h
    src/route/PowerlineRouteBridge.h
    src/route/PowerlineRouteJson.h
    src/route/PowerlineRouteTypes.h
    src/route/RouteInterop.h
)

set(APP_TRANSLATION_SOURCES
    translations/lasviewer_zh_CN.ts
)

find_program(QT_LRELEASE_EXECUTABLE
    NAMES lrelease lrelease.exe
    HINTS "${QT_ROOT}/bin"
)

set(GENERATED_APP_TRANSLATIONS)
if(QT_LRELEASE_EXECUTABLE)
    foreach(app_translation_source IN LISTS APP_TRANSLATION_SOURCES)
        get_filename_component(app_translation_name "${app_translation_source}" NAME_WE)
        set(app_translation_output "${CMAKE_CURRENT_BINARY_DIR}/translations/${app_translation_name}.qm")
        add_custom_command(
            OUTPUT "${app_translation_output}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/translations"
            COMMAND "${QT_LRELEASE_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/${app_translation_source}" -qm "${app_translation_output}"
            DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${app_translation_source}"
            COMMENT "Generating translation ${app_translation_name}.qm"
            VERBATIM
        )
        list(APPEND GENERATED_APP_TRANSLATIONS "${app_translation_output}")
    endforeach()

    add_custom_target(lasviewer_translations ALL DEPENDS ${GENERATED_APP_TRANSLATIONS})
else()
    message(WARNING "Qt lrelease was not found. Application translation files will not be generated.")
endif()

set(COMMON_SOURCES
    src/crs/CrsCatalogService.cpp
    src/crs/CrsAuthorityService.cpp
    src/crs/CrsProjRuntime.cpp
    src/crs/CrsSelectionDialog.cpp
    src/crs/CrsTransformService.cpp
    src/crs/CrsTypes.cpp
    src/crs/ProjectCoordinateSystemsDialog.cpp
    src/crs/RecentCrsStore.cpp
    src/domain/AnalysisData.cpp
    src/domain/ClassificationEditStore.cpp
    src/domain/DataManager.cpp
    src/domain/InspectionData.cpp
    src/domain/RuleBasedClearanceEngine.cpp
    src/domain/TowerFileInterop.cpp
    src/domain/VegetationRiskAnalysis.cpp
    src/gui/PointCloudViewer.cpp
    src/gui/UiHistoryStore.cpp
    src/pointcloud/LasReader.cpp
    src/pointcloud/PointCloudData.cpp
    src/pointcloud/PointCloudTileStore.cpp
    src/osg/OsgPointCloudNode.cpp
    src/route/InspectionRoutePlanning.cpp
    src/route/PowerlineRouteBridge.cpp
    src/route/PowerlineRouteJson.cpp
    src/route/RouteInterop.cpp
)

set(COMMON_HEADERS
    src/crs/CrsCatalogService.h
    src/crs/CrsAuthorityService.h
    src/crs/CrsProjRuntime.h
    src/crs/CrsSelectionDialog.h
    src/crs/CrsTransformService.h
    src/crs/CrsTypes.h
    src/crs/LASViewerCrsExport.h
    src/crs/ProjectCoordinateSystemsDialog.h
    src/crs/RecentCrsStore.h
    src/domain/AnalysisData.h
    src/domain/ClassificationEditStore.h
    src/domain/DataManager.h
    src/domain/InspectionData.h
    src/domain/RuleBasedClearanceEngine.h
    src/domain/TowerFileInterop.h
    src/domain/VegetationRiskAnalysis.h
    src/gui/PointCloudViewer.h
    src/gui/UiHistoryStore.h
    src/pointcloud/LasReader.h
    src/pointcloud/PointCloudData.h
    src/pointcloud/PointCloudTileStore.h
    src/osg/PointCloudVisualization.h
    src/osg/OsgPointCloudNode.h
    src/route/InspectionRoutePlanning.h
    src/route/PowerlineRouteBridge.h
    src/route/PowerlineRouteJson.h
    src/route/PowerlineRouteTypes.h
    src/route/RouteInterop.h
)

set(BACKSTAGE_SMOKE_SOURCES
    resources.qrc
    src/crs/CrsCatalogService.cpp
    src/crs/CrsAuthorityService.cpp
    src/crs/CrsProjRuntime.cpp
    src/crs/CrsSelectionDialog.cpp
    src/crs/CrsTransformService.cpp
    src/crs/CrsTypes.cpp
    src/crs/ProjectCoordinateSystemsDialog.cpp
    src/crs/RecentCrsStore.cpp
    src/domain/AnalysisData.cpp
    src/domain/ClassificationEditStore.cpp
    src/domain/ClearanceAnalysis.cpp
    src/domain/ClearanceReportExporter.cpp
    src/domain/DataManager.cpp
    src/domain/InspectionData.cpp
    src/domain/InspectionReportExporter.cpp
    src/domain/ProfileMarkerProjection.cpp
    src/domain/RuleBasedClearanceEngine.cpp
    src/domain/TowerFileInterop.cpp
    src/domain/VegetationRiskAnalysis.cpp
    src/gui/ApplicationLogDock.cpp
    src/gui/BackstageAboutWidget.cpp
    src/gui/BackstageAboutWidget.ui
    src/gui/BackstageApplicationSettingsWidget.cpp
    src/gui/BackstageApplicationSettingsWidget.ui
    src/gui/BackstageOpenActionsWidget.cpp
    src/gui/BackstageOpenActionsWidget.ui
    src/gui/BackstageOpenProjectWidget.cpp
    src/gui/BackstageOpenProjectWidget.ui
    src/gui/BackstagePageHeaderWidget.cpp
    src/gui/BackstagePageHeaderWidget.ui
    src/gui/BackstageProjectPropertiesWidget.cpp
    src/gui/BackstageProjectPropertiesWidget.ui
    src/gui/DatasetSummaryWidget.cpp
    src/gui/DatasetSummaryWidget.ui
    src/gui/IssueController.cpp
    src/gui/IssueEditorWidget.cpp
    src/gui/IssueEditorWidget.ui
    src/gui/MainWindow.cpp
    src/gui/MeasurementAnalysisController.cpp
    src/gui/NavigationSettingsWidget.cpp
    src/gui/NavigationSettingsWidget.ui
    src/gui/PointCloudViewer.cpp
    src/gui/ProfileClassificationController.cpp
    src/gui/ProfileClassificationDock.cpp
    src/gui/ProfileClassificationWidget.cpp
    src/gui/ProfileClassificationWidget.ui
    src/gui/ProjectExplorerController.cpp
    src/gui/ProjectExplorerDock.cpp
    src/gui/RouteController.cpp
    src/gui/RouteDetailsDock.cpp
    src/gui/SceneInspectorDock.cpp
    src/gui/SpanProfileDock.cpp
    src/gui/TowerController.cpp
    src/gui/TowerEditorWidget.cpp
    src/gui/TowerEditorWidget.ui
    src/gui/ProfilePlotWidget.cpp
    src/gui/UiHistoryStore.cpp
    src/gui/VisualizationPanelController.cpp
    src/gui/support/RibbonIconFactory.cpp
    src/gui/support/UiHelpers.cpp
    src/logging/ApplicationLogger.cpp
    src/pointcloud/LasReader.cpp
    src/pointcloud/PointCloudData.cpp
    src/pointcloud/PointCloudTileStore.cpp
    src/osg/OsgPointCloudNode.cpp
    src/route/InspectionRoutePlanning.cpp
    src/route/PowerlineRouteBridge.cpp
    src/route/PowerlineRouteJson.cpp
    src/route/RouteInterop.cpp
)

set(BACKSTAGE_SMOKE_HEADERS
    src/crs/CrsCatalogService.h
    src/crs/CrsAuthorityService.h
    src/crs/CrsProjRuntime.h
    src/crs/CrsSelectionDialog.h
    src/crs/CrsTransformService.h
    src/crs/CrsTypes.h
    src/crs/LASViewerCrsExport.h
    src/crs/ProjectCoordinateSystemsDialog.h
    src/crs/RecentCrsStore.h
    src/domain/AnalysisData.h
    src/domain/ClassificationEditStore.h
    src/domain/ClearanceAnalysis.h
    src/domain/ClearanceReportExporter.h
    src/domain/DataManager.h
    src/domain/InspectionData.h
    src/domain/InspectionReportExporter.h
    src/domain/ProjectMetadata.h
    src/domain/ProfileMarkerProjection.h
    src/domain/RuleBasedClearanceEngine.h
    src/domain/TowerFileInterop.h
    src/domain/VegetationRiskAnalysis.h
    src/gui/ApplicationLogDock.h
    src/gui/BackstageAboutWidget.h
    src/gui/BackstageApplicationSettingsWidget.h
    src/gui/BackstageOpenActionsWidget.h
    src/gui/BackstageOpenProjectWidget.h
    src/gui/BackstagePageHeaderWidget.h
    src/gui/BackstageProjectPropertiesWidget.h
    src/gui/DatasetSummaryWidget.h
    src/gui/IssueController.h
    src/gui/IssueEditorWidget.h
    src/gui/MainWindow.h
    src/gui/MeasurementAnalysisController.h
    src/gui/NavigationSettingsWidget.h
    src/gui/PointCloudViewer.h
    src/gui/ProfileClassificationController.h
    src/gui/ProfileClassificationDock.h
    src/gui/ProfileClassificationWidget.h
    src/gui/ProjectExplorerController.h
    src/gui/ProjectExplorerDock.h
    src/gui/RouteController.h
    src/gui/RouteDetailsDock.h
    src/gui/SceneInspectorDock.h
    src/gui/SpanProfileDock.h
    src/gui/TowerController.h
    src/gui/TowerEditorWidget.h
    src/gui/VisualizationPanelController.h
    src/gui/ProfilePlotWidget.h
    src/gui/support/RibbonIconFactory.h
    src/gui/support/SettingsKeys.h
    src/gui/support/UiHelpers.h
    src/gui/UiHistoryStore.h
    src/logging/ApplicationLogger.h
    src/pointcloud/LasReader.h
    src/pointcloud/PointCloudData.h
    src/pointcloud/PointCloudTileStore.h
    src/osg/PointCloudVisualization.h
    src/osg/OsgPointCloudNode.h
    src/route/InspectionRoutePlanning.h
    src/route/PowerlineRouteBridge.h
    src/route/PowerlineRouteJson.h
    src/route/PowerlineRouteTypes.h
    src/route/RouteInterop.h
)

add_executable(${PROJECT_NAME} WIN32 ${SOURCES} ${HEADERS})

function(configure_las_viewer_target target_name use_qtitan)
    target_include_directories(${target_name} PRIVATE
        "${CMAKE_SOURCE_DIR}/src"
        "${OSG_INCLUDE_DIR}"
    )

    target_link_libraries(${target_name} PRIVATE
        Qt5::Core
        Qt5::Gui
        Qt5::Widgets
        Qt5::OpenGL
        ${OSG_LIBRARIES}
    )

    if(use_qtitan)
        target_include_directories(${target_name} PRIVATE "${QTITAN_INCLUDE_DIR}")
        if(LAS_VIEWER_FORCE_RELEASE_THIRDPARTY_FOR_DEBUG)
            target_link_libraries(${target_name} PRIVATE "${QTITAN_RELEASE_LIBRARY}")
        else()
            target_link_libraries(${target_name} PRIVATE
                optimized "${QTITAN_RELEASE_LIBRARY}"
                debug "${QTITAN_DEBUG_LIBRARY}"
            )
        endif()
    endif()

    if(PCL_FOUND)
        target_compile_definitions(${target_name} PRIVATE LAS_VIEWER_HAS_PCL=1)
        target_include_directories(${target_name} PRIVATE ${PCL_INCLUDE_DIRS})
        target_link_libraries(${target_name} PRIVATE ${PCL_LIBRARIES})
    endif()

    if(LAS_VIEWER_ENABLE_LASLIB)
        target_compile_definitions(${target_name} PRIVATE LAS_VIEWER_HAS_LASLIB=1)
        target_include_directories(${target_name} PRIVATE
            "${LASLIB_INCLUDE_DIR}"
            "${LASZIP_INCLUDE_DIR}"
            "${LASZIP_SRC_INCLUDE_DIR}"
        )
        target_link_libraries(${target_name} PRIVATE
            "${LASLIB_LIBRARY}"
            "${LASZIP_LIBRARY}"
        )
    endif()

    if(LAS_VIEWER_HAS_PROJ)
        target_compile_definitions(${target_name} PRIVATE LAS_VIEWER_HAS_PROJ=1 LASVIEWERCRS_HAS_PROJ=1)
        target_include_directories(${target_name} PRIVATE "${PROJ_INCLUDE_DIR}")
        target_link_libraries(${target_name} PRIVATE "${PROJ_LIBRARY}")
    endif()

    if(MSVC)
        target_compile_definitions(${target_name} PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
        target_compile_options(${target_name} PRIVATE /MP /FS)
        if(LAS_VIEWER_FORCE_RELEASE_THIRDPARTY_FOR_DEBUG)
            target_compile_definitions(${target_name} PRIVATE $<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>)
            set_target_properties(${target_name} PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
        endif()
    endif()

    if(WIN32)
        target_link_libraries(${target_name} PRIVATE opengl32 glu32 ws2_32 winmm)

        if(use_qtitan)
            if(LAS_VIEWER_FORCE_RELEASE_THIRDPARTY_FOR_DEBUG)
                set(qtitan_runtime_dll "qtnribbon4.dll")
            else()
                set(qtitan_runtime_dll "$<IF:$<CONFIG:Debug>,qtnribbond4.dll,qtnribbon4.dll>")
            endif()
            add_custom_command(TARGET ${target_name} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${QTITAN_ROOT}/bin/${qtitan_runtime_dll}"
                "$<TARGET_FILE_DIR:${target_name}>"
            )
        endif()
    endif()

    set_target_properties(${target_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
endfunction()

function(deploy_osg_runtime target_name)
    if(NOT WIN32)
        return()
    endif()

    if(LAS_VIEWER_FORCE_RELEASE_THIRDPARTY_FOR_DEBUG)
        set(osg_runtime_dlls
            "OpenThreads.dll"
            "osg.dll"
            "osgDB.dll"
            "osgGA.dll"
            "osgViewer.dll"
            "osgUtil.dll"
        )
    else()
        set(osg_runtime_dlls
            "OpenThreads$<$<CONFIG:Debug>:d>.dll"
            "osg$<$<CONFIG:Debug>:d>.dll"
            "osgDB$<$<CONFIG:Debug>:d>.dll"
            "osgGA$<$<CONFIG:Debug>:d>.dll"
            "osgViewer$<$<CONFIG:Debug>:d>.dll"
            "osgUtil$<$<CONFIG:Debug>:d>.dll"
        )
    endif()

    foreach(osg_runtime_dll IN LISTS osg_runtime_dlls)
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${OSG_ROOT}/bin/${osg_runtime_dll}"
            "$<TARGET_FILE_DIR:${target_name}>"
        )
    endforeach()
endfunction()

function(deploy_runtime_dependencies target_name use_qtitan)
    if(NOT WIN32)
        return()
    endif()

    if(LAS_VIEWER_FORCE_RELEASE_THIRDPARTY_FOR_DEBUG)
        set(osg_runtime_probe_suffix "")
    else()
        set(osg_runtime_probe_suffix "$<$<CONFIG:Debug>:d>")
    endif()

    if(LAS_VIEWER_FORCE_RELEASE_THIRDPARTY_FOR_DEBUG)
        set(qtitan_runtime_probe "qtnribbon4.dll")
    else()
        set(qtitan_runtime_probe "$<IF:$<CONFIG:Debug>,qtnribbond4.dll,qtnribbon4.dll>")
    endif()

    set(runtime_dependency_script "${CMAKE_CURRENT_BINARY_DIR}/${target_name}_deploy_runtime_dependencies_$<CONFIG>.cmake")
    set(runtime_dependency_script_content [=[
set(target_file "$<TARGET_FILE:@target_name@>")
set(target_dir "$<TARGET_FILE_DIR:@target_name@>")

set(scan_libraries
    "@OSG_ROOT@/bin/OpenThreads@osg_runtime_probe_suffix@.dll"
    "@OSG_ROOT@/bin/osg@osg_runtime_probe_suffix@.dll"
    "@OSG_ROOT@/bin/osgDB@osg_runtime_probe_suffix@.dll"
    "@OSG_ROOT@/bin/osgGA@osg_runtime_probe_suffix@.dll"
    "@OSG_ROOT@/bin/osgViewer@osg_runtime_probe_suffix@.dll"
    "@OSG_ROOT@/bin/osgUtil@osg_runtime_probe_suffix@.dll"
)

if("@use_qtitan@" STREQUAL "TRUE")
    list(APPEND scan_libraries "@QTITAN_ROOT@/bin/@qtitan_runtime_probe@")
endif()

if(EXISTS "@LASLIB_ROOT@/bin")
    file(GLOB las_runtime_dlls LIST_DIRECTORIES FALSE "@LASLIB_ROOT@/bin/*.dll")
    list(APPEND scan_libraries ${las_runtime_dlls})
endif()

set(existing_scan_libraries)
foreach(scan_library IN LISTS scan_libraries)
    if(EXISTS "${scan_library}")
        list(APPEND existing_scan_libraries "${scan_library}")
    endif()
endforeach()

    set(search_directories
        "@QT_ROOT@/bin"
        "@OSG_ROOT@/bin"
        "@QTITAN_ROOT@/bin"
        "@GDAL_ROOT@/bin"
        "@LASLIB_ROOT@/bin"
        "@LASTOOLS_ROOT@/bin"
        "@PROJ_ROOT@/bin"
    )

if(existing_scan_libraries)
    file(GET_RUNTIME_DEPENDENCIES
        RESOLVED_DEPENDENCIES_VAR resolved_runtime_dependencies
        UNRESOLVED_DEPENDENCIES_VAR unresolved_runtime_dependencies
        CONFLICTING_DEPENDENCIES_PREFIX conflicting_runtime_dependencies
        EXECUTABLES "${target_file}"
        LIBRARIES ${existing_scan_libraries}
        DIRECTORIES ${search_directories}
        PRE_EXCLUDE_REGEXES
            "api-ms-win-.*"
            "ext-ms-.*"
            "HvsiFileTrust\\.dll"
        POST_EXCLUDE_REGEXES
            "^.*[\\\\/][Ww]indows[\\\\/].*"
            "^.*[\\\\/][Ss]ystem32[\\\\/].*"
            "^.*[\\\\/][Ss]ysWOW64[\\\\/].*"
    )
else()
    file(GET_RUNTIME_DEPENDENCIES
        RESOLVED_DEPENDENCIES_VAR resolved_runtime_dependencies
        UNRESOLVED_DEPENDENCIES_VAR unresolved_runtime_dependencies
        CONFLICTING_DEPENDENCIES_PREFIX conflicting_runtime_dependencies
        EXECUTABLES "${target_file}"
        DIRECTORIES ${search_directories}
        PRE_EXCLUDE_REGEXES
            "api-ms-win-.*"
            "ext-ms-.*"
            "HvsiFileTrust\\.dll"
        POST_EXCLUDE_REGEXES
            "^.*[\\\\/][Ww]indows[\\\\/].*"
            "^.*[\\\\/][Ss]ystem32[\\\\/].*"
            "^.*[\\\\/][Ss]ysWOW64[\\\\/].*"
    )
endif()

list(REMOVE_DUPLICATES resolved_runtime_dependencies)
foreach(resolved_dependency IN LISTS resolved_runtime_dependencies)
    if(EXISTS "${resolved_dependency}")
        file(COPY "${resolved_dependency}" DESTINATION "${target_dir}")
    endif()
endforeach()

if(unresolved_runtime_dependencies)
    list(REMOVE_DUPLICATES unresolved_runtime_dependencies)
    message(STATUS "Unresolved runtime dependencies for @target_name@: ${unresolved_runtime_dependencies}")
endif()
]=])
    string(CONFIGURE "${runtime_dependency_script_content}" runtime_dependency_script_content @ONLY)
    file(GENERATE OUTPUT "${runtime_dependency_script}" CONTENT "${runtime_dependency_script_content}")

    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -P "${runtime_dependency_script}"
        COMMENT "Collecting runtime dependencies for ${target_name}"
    )
endfunction()

function(deploy_qt_runtime target_name use_qtitan)
    if(NOT WIN32)
        return()
    endif()

    if(LAS_VIEWER_FORCE_RELEASE_THIRDPARTY_FOR_DEBUG)
        set(qt_runtime_suffix "")
    else()
        set(qt_runtime_suffix "$<$<CONFIG:Debug>:d>")
    endif()

    set(qt_runtime_files
        "Qt5Core${qt_runtime_suffix}.dll"
        "Qt5Gui${qt_runtime_suffix}.dll"
        "Qt5Widgets${qt_runtime_suffix}.dll"
        "Qt5OpenGL${qt_runtime_suffix}.dll"
        "libEGL.dll"
        "libGLESv2.dll"
        "opengl32sw.dll"
    )

    if(use_qtitan)
        list(APPEND qt_runtime_files
            "Qt5Svg${qt_runtime_suffix}.dll"
            "Qt5Xml${qt_runtime_suffix}.dll"
        )
    endif()

    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E rm -f
        "$<TARGET_FILE_DIR:${target_name}>/Qt5Core.dll"
        "$<TARGET_FILE_DIR:${target_name}>/Qt5Gui.dll"
        "$<TARGET_FILE_DIR:${target_name}>/Qt5Widgets.dll"
        "$<TARGET_FILE_DIR:${target_name}>/Qt5OpenGL.dll"
        "$<TARGET_FILE_DIR:${target_name}>/Qt5Svg.dll"
        "$<TARGET_FILE_DIR:${target_name}>/Qt5Xml.dll"
        "$<TARGET_FILE_DIR:${target_name}>/Qt5Cored.dll"
        "$<TARGET_FILE_DIR:${target_name}>/Qt5Guid.dll"
        "$<TARGET_FILE_DIR:${target_name}>/Qt5Widgetsd.dll"
        "$<TARGET_FILE_DIR:${target_name}>/Qt5OpenGLd.dll"
        "$<TARGET_FILE_DIR:${target_name}>/Qt5Svgd.dll"
        "$<TARGET_FILE_DIR:${target_name}>/Qt5Xmld.dll"
        "$<TARGET_FILE_DIR:${target_name}>/libEGL.dll"
        "$<TARGET_FILE_DIR:${target_name}>/libGLESv2.dll"
        "$<TARGET_FILE_DIR:${target_name}>/opengl32sw.dll"
        COMMAND ${CMAKE_COMMAND} -E remove_directory "$<TARGET_FILE_DIR:${target_name}>/platforms"
        COMMAND ${CMAKE_COMMAND} -E remove_directory "$<TARGET_FILE_DIR:${target_name}>/styles"
        COMMAND ${CMAKE_COMMAND} -E remove_directory "$<TARGET_FILE_DIR:${target_name}>/iconengines"
        COMMAND ${CMAKE_COMMAND} -E remove_directory "$<TARGET_FILE_DIR:${target_name}>/imageformats"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target_name}>/platforms"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target_name}>/styles"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target_name}>/iconengines"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target_name}>/imageformats"
        COMMENT "Deploying Qt runtime dependencies"
    )

    foreach(qt_runtime_file IN LISTS qt_runtime_files)
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT_ROOT}/bin/${qt_runtime_file}"
            "$<TARGET_FILE_DIR:${target_name}>"
        )
    endforeach()

    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${QT_ROOT}/plugins/platforms/qwindows${qt_runtime_suffix}.dll"
        "$<TARGET_FILE_DIR:${target_name}>/platforms"
    )

    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${QT_ROOT}/plugins/styles/qwindowsvistastyle${qt_runtime_suffix}.dll"
        "$<TARGET_FILE_DIR:${target_name}>/styles"
    )

    if(use_qtitan)
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT_ROOT}/plugins/iconengines/qsvgicon${qt_runtime_suffix}.dll"
            "$<TARGET_FILE_DIR:${target_name}>/iconengines"
        )

        set(qt_imageformat_plugins
            "qgif${qt_runtime_suffix}.dll"
            "qico${qt_runtime_suffix}.dll"
            "qjpeg${qt_runtime_suffix}.dll"
            "qsvg${qt_runtime_suffix}.dll"
        )
        foreach(qt_imageformat_plugin IN LISTS qt_imageformat_plugins)
            add_custom_command(TARGET ${target_name} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${QT_ROOT}/plugins/imageformats/${qt_imageformat_plugin}"
                "$<TARGET_FILE_DIR:${target_name}>/imageformats"
            )
        endforeach()
    endif()
endfunction()

function(deploy_translation_runtime target_name)
    if(NOT WIN32)
        return()
    endif()

    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target_name}>/translations"
        COMMENT "Deploying translation runtime files"
    )

    foreach(app_translation_file IN LISTS GENERATED_APP_TRANSLATIONS)
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${app_translation_file}"
            "$<TARGET_FILE_DIR:${target_name}>/translations"
        )
    endforeach()

    if(EXISTS "${QT_ROOT}/translations/qt_zh_CN.qm")
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT_ROOT}/translations/qt_zh_CN.qm"
            "$<TARGET_FILE_DIR:${target_name}>/translations"
        )
    endif()
endfunction()

function(deploy_proj_runtime_assets target_name)
    if(NOT WIN32)
        return()
    endif()

    set(proj_runtime_share_dir "")
    if(PROJ_ROOT AND EXISTS "${PROJ_ROOT}/bin/proj9/share/proj.db")
        set(proj_runtime_share_dir "${PROJ_ROOT}/bin/proj9/share")
    elseif(PROJ_ROOT AND EXISTS "${PROJ_ROOT}/share/proj/proj.db")
        set(proj_runtime_share_dir "${PROJ_ROOT}/share/proj")
    elseif(GDAL_ROOT AND EXISTS "${GDAL_ROOT}/bin/proj9/share/proj.db")
        set(proj_runtime_share_dir "${GDAL_ROOT}/bin/proj9/share")
    elseif(GDAL_ROOT AND EXISTS "${GDAL_ROOT}/share/proj/proj.db")
        set(proj_runtime_share_dir "${GDAL_ROOT}/share/proj")
    else()
        return()
    endif()

    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E remove_directory "$<TARGET_FILE_DIR:${target_name}>/gdal-data"
        COMMAND ${CMAKE_COMMAND} -E remove_directory "$<TARGET_FILE_DIR:${target_name}>/proj9"
        COMMAND ${CMAKE_COMMAND} -E remove_directory "$<TARGET_FILE_DIR:${target_name}>/gdal"
        COMMAND ${CMAKE_COMMAND} -E remove "$<TARGET_FILE_DIR:${target_name}>/gdal-env.bat"
        COMMENT "Refreshing PROJ runtime assets for ${target_name}"
    )

    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target_name}>/proj9"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${proj_runtime_share_dir}"
        "$<TARGET_FILE_DIR:${target_name}>/proj9/share"
    )
endfunction()

configure_las_viewer_target(${PROJECT_NAME} TRUE)
if(TARGET lasviewer_translations)
    add_dependencies(${PROJECT_NAME} lasviewer_translations)
endif()

add_executable(LASViewerSmokeTest
    examples/viewer_smoke_test.cpp
    ${BACKSTAGE_SMOKE_SOURCES}
    ${BACKSTAGE_SMOKE_HEADERS}
)
configure_las_viewer_target(LASViewerSmokeTest TRUE)
set_target_properties(LASViewerSmokeTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(WIN32)
    deploy_osg_runtime(${PROJECT_NAME})
    deploy_qt_runtime(${PROJECT_NAME} TRUE)
    deploy_runtime_dependencies(${PROJECT_NAME} TRUE)
    deploy_translation_runtime(${PROJECT_NAME})
    deploy_proj_runtime_assets(${PROJECT_NAME})
    deploy_osg_runtime(LASViewerSmokeTest)
    deploy_qt_runtime(LASViewerSmokeTest TRUE)
    deploy_runtime_dependencies(LASViewerSmokeTest TRUE)
    deploy_translation_runtime(LASViewerSmokeTest)
    deploy_proj_runtime_assets(LASViewerSmokeTest)
endif()


