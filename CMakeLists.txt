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

list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(LASViewerDependencies)
include(LASViewerTranslations)
include(LASViewerTargetConfig)
include(LASViewerRuntimeDeploy)

add_library(LASViewerCoreObj OBJECT)
configure_las_viewer_common_target(LASViewerCoreObj TRUE)

add_executable(${PROJECT_NAME} WIN32)
configure_las_viewer_target(${PROJECT_NAME} TRUE)
target_link_libraries(${PROJECT_NAME} PRIVATE LASViewerCoreObj)
if(TARGET lasviewer_translations)
    add_dependencies(${PROJECT_NAME} lasviewer_translations)
endif()

add_executable(LASViewerSmokeTest)
configure_las_viewer_target(LASViewerSmokeTest TRUE)
target_link_libraries(LASViewerSmokeTest PRIVATE LASViewerCoreObj)

las_viewer_add_shared_sources(resources.qrc)

add_subdirectory(src)
add_subdirectory(examples)

deploy_las_viewer_runtime(${PROJECT_NAME} TRUE)
deploy_las_viewer_runtime(LASViewerSmokeTest TRUE)
