include_guard(GLOBAL)

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

    if(LAS_VIEWER_HAS_WEBENGINE_DOCK)
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target_name}>/resources"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT_ROOT}/bin/QtWebEngineProcess.exe"
            "$<TARGET_FILE_DIR:${target_name}>"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${QT_ROOT}/resources"
            "$<TARGET_FILE_DIR:${target_name}>/resources"
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target_name}>/translations/qtwebengine_locales"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${QT_ROOT}/translations/qtwebengine_locales"
            "$<TARGET_FILE_DIR:${target_name}>/translations/qtwebengine_locales"
            COMMENT "Deploying Qt WebEngine runtime assets"
        )

        file(GLOB qt_webengine_translation_files
            LIST_DIRECTORIES FALSE
            "${QT_ROOT}/translations/qtwebengine_*.qm"
        )
        foreach(qt_webengine_translation_file IN LISTS qt_webengine_translation_files)
            add_custom_command(TARGET ${target_name} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${qt_webengine_translation_file}"
                "$<TARGET_FILE_DIR:${target_name}>/translations"
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

function(deploy_las_viewer_runtime target_name use_qtitan)
    if(NOT WIN32)
        return()
    endif()

    deploy_osg_runtime(${target_name})
    deploy_qt_runtime(${target_name} ${use_qtitan})
    deploy_runtime_dependencies(${target_name} ${use_qtitan})
    deploy_translation_runtime(${target_name})
    deploy_proj_runtime_assets(${target_name})
endfunction()
