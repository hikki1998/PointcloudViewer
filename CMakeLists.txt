cmake_minimum_required(VERSION 3.16)

project(LASPointCloudViewer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(LAS_VIEWER_ENABLE_PCL "Enable PCL integration" OFF)
option(LAS_VIEWER_ENABLE_LASLIB "Enable LASlib-based LAS/LAZ reader" ON)

set(QT_ROOT "E:/code/Qt5.15.2/5.15.2/msvc2019_64" CACHE PATH "Qt installation root")
set(OSG_ROOT "E:/code/thirdparty/osg3.6.5" CACHE PATH "OpenSceneGraph installation root")
set(QTITAN_ROOT "E:/code/thirdparty/QtitanRibbon" CACHE PATH "QtitanRibbon package root")
set(PCL_ROOT "E:/code/thirdparty/PCL 1.8.1" CACHE PATH "PCL installation root")
set(LASLIB_ROOT "E:/code/thirdparty/laslib-v2.0.4-win64" CACHE PATH "LASlib package root")
set(LASZIP_ROOT "E:/code/thirdparty/LAStools-2.0.4/LASzip" CACHE PATH "LASzip source root")

if(NOT LASLIB_ROOT)
    set(LASLIB_ROOT "E:/code/thirdparty/laslib-v2.0.4-win64" CACHE PATH "LASlib package root" FORCE)
endif()

if(NOT LASZIP_ROOT)
    set(LASZIP_ROOT "E:/code/thirdparty/LAStools-2.0.4/LASzip" CACHE PATH "LASzip source root" FORCE)
endif()

list(PREPEND CMAKE_PREFIX_PATH
    "${QT_ROOT}"
    "${QT_ROOT}/lib/cmake"
    "${PCL_ROOT}"
    "${PCL_ROOT}/cmake"
)

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

set(LAS_VIEWER_FORCE_RELEASE_THIRDPARTY_FOR_DEBUG OFF)
if(MSVC AND LAS_VIEWER_ENABLE_LASLIB AND (NOT LASLIB_DEBUG_LIBRARY OR NOT LASZIP_DEBUG_LIBRARY))
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
)

find_package_handle_standard_args(QtitanRibbon
    REQUIRED_VARS QTITAN_INCLUDE_DIR QTITAN_RELEASE_LIBRARY QTITAN_DEBUG_LIBRARY
)

if(LAS_VIEWER_ENABLE_LASLIB)
    find_package_handle_standard_args(LASlib
        REQUIRED_VARS LASLIB_INCLUDE_DIR LASZIP_INCLUDE_DIR LASZIP_SRC_INCLUDE_DIR LASLIB_LIBRARY LASZIP_LIBRARY
    )
endif()

set(SOURCES
    src/main.cpp
    src/gui/MainWindow.cpp
    src/gui/PointCloudViewer.cpp
    src/pointcloud/LasReader.cpp
    src/pointcloud/PointCloudData.cpp
    src/osg/OsgPointCloudNode.cpp
)

set(HEADERS
    src/gui/MainWindow.h
    src/gui/PointCloudViewer.h
    src/pointcloud/LasReader.h
    src/pointcloud/PointCloudData.h
    src/osg/PointCloudVisualization.h
    src/osg/OsgPointCloudNode.h
)

set(COMMON_SOURCES
    src/gui/PointCloudViewer.cpp
    src/pointcloud/LasReader.cpp
    src/pointcloud/PointCloudData.cpp
    src/osg/OsgPointCloudNode.cpp
)

set(COMMON_HEADERS
    src/gui/PointCloudViewer.h
    src/pointcloud/LasReader.h
    src/pointcloud/PointCloudData.h
    src/osg/PointCloudVisualization.h
    src/osg/OsgPointCloudNode.h
)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

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

    if(MSVC)
        target_compile_definitions(${target_name} PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
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

    set(osg_runtime_dlls
        "OpenThreads$<$<CONFIG:Debug>:d>.dll"
        "osg$<$<CONFIG:Debug>:d>.dll"
        "osgDB$<$<CONFIG:Debug>:d>.dll"
        "osgGA$<$<CONFIG:Debug>:d>.dll"
        "osgViewer$<$<CONFIG:Debug>:d>.dll"
        "osgUtil$<$<CONFIG:Debug>:d>.dll"
    )

    foreach(osg_runtime_dll IN LISTS osg_runtime_dlls)
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${OSG_ROOT}/bin/${osg_runtime_dll}"
            "$<TARGET_FILE_DIR:${target_name}>"
        )
    endforeach()
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

configure_las_viewer_target(${PROJECT_NAME} TRUE)

add_executable(LASViewerSmokeTest
    examples/viewer_smoke_test.cpp
    ${COMMON_SOURCES}
    ${COMMON_HEADERS}
)
configure_las_viewer_target(LASViewerSmokeTest FALSE)
set_target_properties(LASViewerSmokeTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/smoke"
)

if(WIN32)
    deploy_osg_runtime(${PROJECT_NAME})
    deploy_qt_runtime(${PROJECT_NAME} TRUE)
    deploy_osg_runtime(LASViewerSmokeTest)
    deploy_qt_runtime(LASViewerSmokeTest FALSE)
endif()
