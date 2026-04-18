include_guard(GLOBAL)

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

set(LAS_VIEWER_WINDOWS_CAPTURE_LIBRARIES "")
if(LAS_VIEWER_ENABLE_WINDOWS_CAPTURE)
    if(NOT WIN32)
        message(FATAL_ERROR
            "LAS_VIEWER_ENABLE_WINDOWS_CAPTURE requires a Windows host toolchain."
        )
    endif()

    include(CheckIncludeFileCXX)
    include(CheckCXXSourceCompiles)

    set(_las_viewer_windows_capture_headers
        d3d11.h
        dxgi1_2.h
        mfapi.h
        mfidl.h
        windows.graphics.capture.interop.h
        winrt/Windows.Graphics.Capture.h
    )

    set(_las_viewer_windows_capture_missing_headers)
    foreach(_header_name IN LISTS _las_viewer_windows_capture_headers)
        string(MAKE_C_IDENTIFIER "${_header_name}" _header_identifier)
        set(_header_var "LAS_VIEWER_HAS_${_header_identifier}")
        check_include_file_cxx("${_header_name}" ${_header_var})
        if(NOT ${_header_var})
            list(APPEND _las_viewer_windows_capture_missing_headers ${_header_name})
        endif()
    endforeach()

    # mfreadwrite.h depends on mfapi.h/mfidl.h declarations.
    check_cxx_source_compiles(
        "#include <mfapi.h>\n#include <mfidl.h>\n#include <mfreadwrite.h>\nint main() { return 0; }"
        LAS_VIEWER_HAS_MEDIAFOUNDATION_READWRITE_HEADERS
    )
    if(NOT LAS_VIEWER_HAS_MEDIAFOUNDATION_READWRITE_HEADERS)
        list(APPEND _las_viewer_windows_capture_missing_headers mfreadwrite.h)
    endif()

    if(_las_viewer_windows_capture_missing_headers)
        list(JOIN _las_viewer_windows_capture_missing_headers ", " _missing_headers_text)
        message(FATAL_ERROR
            "Windows capture backend requires missing headers: ${_missing_headers_text}.\n"
            "Install Windows 10/11 SDK with Media Foundation and C++/WinRT components, "
            "or configure with -DLAS_VIEWER_ENABLE_WINDOWS_CAPTURE=OFF."
        )
    endif()

    set(LAS_VIEWER_WINDOWS_CAPTURE_LIBRARIES
        d3d11
        dxgi
        windowsapp
        mfplat
        mfreadwrite
        mfuuid
    )
    message(STATUS
        "Windows capture backend enabled. System libraries: ${LAS_VIEWER_WINDOWS_CAPTURE_LIBRARIES}"
    )
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
