include_guard(GLOBAL)

function(las_viewer_set_source_routes)
    set(options)
    set(one_value_args APP SHARED SMOKE)
    set(multi_value_args)
    cmake_parse_arguments(LAS_VIEWER "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})

    if(LAS_VIEWER_APP)
        set_property(GLOBAL PROPERTY LAS_VIEWER_SOURCE_ROUTE_APP "${LAS_VIEWER_APP}")
    endif()

    if(LAS_VIEWER_SHARED)
        set_property(GLOBAL PROPERTY LAS_VIEWER_SOURCE_ROUTE_SHARED "${LAS_VIEWER_SHARED}")
    endif()

    if(LAS_VIEWER_SMOKE)
        set_property(GLOBAL PROPERTY LAS_VIEWER_SOURCE_ROUTE_SMOKE "${LAS_VIEWER_SMOKE}")
    endif()
endfunction()

function(las_viewer_resolve_source_route scope out_target)
    string(TOUPPER "${scope}" scope_upper)
    get_property(configured_target GLOBAL PROPERTY "LAS_VIEWER_SOURCE_ROUTE_${scope_upper}")

    if(NOT configured_target)
        if(scope_upper STREQUAL "APP")
            set(configured_target "${PROJECT_NAME}")
        elseif(scope_upper STREQUAL "SHARED")
            set(configured_target "LASViewerCoreObj")
        elseif(scope_upper STREQUAL "SMOKE")
            set(configured_target "LASViewerSmokeTest")
        else()
            message(FATAL_ERROR "Unknown source route scope: ${scope}")
        endif()
    endif()

    if(NOT TARGET ${configured_target})
        message(FATAL_ERROR
            "Source route '${scope}' points to target '${configured_target}', but that target does not exist yet."
        )
    endif()

    set(${out_target} "${configured_target}" PARENT_SCOPE)
endfunction()

function(las_viewer_add_app_sources)
    las_viewer_resolve_source_route(APP app_target)
    target_sources(${app_target} PRIVATE ${ARGN})
endfunction()

function(las_viewer_add_smoke_sources)
    las_viewer_resolve_source_route(SMOKE smoke_target)
    target_sources(${smoke_target} PRIVATE ${ARGN})
endfunction()

function(las_viewer_add_shared_sources)
    las_viewer_resolve_source_route(SHARED shared_target)
    target_sources(${shared_target} PRIVATE ${ARGN})
endfunction()

function(configure_las_viewer_common_target target_name use_qtitan)
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

    if(LAS_VIEWER_HAS_WEBENGINE_DOCK AND TARGET Qt5::WebEngineWidgets)
        target_compile_definitions(${target_name} PRIVATE LAS_VIEWER_HAS_WEBENGINE_DOCK=1)
        target_link_libraries(${target_name} PRIVATE Qt5::WebEngineWidgets)
    endif()

    if(use_qtitan)
        target_include_directories(${target_name} PRIVATE "${QTITAN_INCLUDE_DIR}")
    endif()

    if(PCL_FOUND)
        target_compile_definitions(${target_name} PRIVATE LAS_VIEWER_HAS_PCL=1)
        target_include_directories(${target_name} PRIVATE ${PCL_INCLUDE_DIRS})
    endif()

    if(LAS_VIEWER_ENABLE_LASLIB)
        target_compile_definitions(${target_name} PRIVATE LAS_VIEWER_HAS_LAS_READER=1 LAS_VIEWER_HAS_LASLIB=1)
        target_include_directories(${target_name} PRIVATE
            "${LASLIB_INCLUDE_DIR}"
            "${LASZIP_INCLUDE_DIR}"
            "${LASZIP_SRC_INCLUDE_DIR}"
        )
    endif()

    if(LAS_VIEWER_ENABLE_LASZIP_API)
        target_compile_definitions(${target_name} PRIVATE LAS_VIEWER_HAS_LAS_READER=1 LAS_VIEWER_HAS_LASZIP_API=1)
        target_include_directories(${target_name} PRIVATE "${LASZIP_API_INCLUDE_DIR}")
    endif()

    if(LAS_VIEWER_HAS_PROJ)
        target_compile_definitions(${target_name} PRIVATE LAS_VIEWER_HAS_PROJ=1 LASVIEWERCRS_HAS_PROJ=1)
        target_include_directories(${target_name} PRIVATE "${PROJ_INCLUDE_DIR}")
    endif()

    if(LAS_VIEWER_ENABLE_WINDOWS_CAPTURE)
        target_compile_definitions(${target_name} PRIVATE LAS_VIEWER_ENABLE_WINDOWS_CAPTURE=1)
    endif()

    if(MSVC)
        target_compile_definitions(${target_name} PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
        target_compile_options(${target_name} PRIVATE /MP /FS)
        if(LAS_VIEWER_FORCE_RELEASE_THIRDPARTY_FOR_DEBUG)
            target_compile_definitions(${target_name} PRIVATE $<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>)
            set_target_properties(${target_name} PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
        endif()
    endif()
endfunction()

function(configure_las_viewer_executable_target target_name use_qtitan)
    if(use_qtitan)
        if(LAS_VIEWER_USE_QTITAN_SHIM_TARGET)
            target_link_libraries(${target_name} PRIVATE qtnribbon4)
        elseif(LAS_VIEWER_FORCE_RELEASE_THIRDPARTY_FOR_DEBUG)
            target_link_libraries(${target_name} PRIVATE "${QTITAN_RELEASE_LIBRARY}")
        else()
            target_link_libraries(${target_name} PRIVATE
                optimized "${QTITAN_RELEASE_LIBRARY}"
                debug "${QTITAN_DEBUG_LIBRARY}"
            )
        endif()
    endif()

    if(PCL_FOUND)
        target_link_libraries(${target_name} PRIVATE ${PCL_LIBRARIES})
    endif()

    if(LAS_VIEWER_ENABLE_LASLIB)
        target_link_libraries(${target_name} PRIVATE
            "${LASLIB_LIBRARY}"
            "${LASZIP_LIBRARY}"
        )
    endif()

    if(LAS_VIEWER_ENABLE_LASZIP_API)
        target_link_libraries(${target_name} PRIVATE "${LASZIP_API_LIBRARY}")
    endif()

    if(LAS_VIEWER_HAS_PROJ)
        target_link_libraries(${target_name} PRIVATE "${PROJ_LIBRARY}")
    endif()

    if(WIN32)
        target_link_libraries(${target_name} PRIVATE opengl32 glu32 ws2_32 winmm)

        if(LAS_VIEWER_ENABLE_WINDOWS_CAPTURE)
            target_link_libraries(${target_name} PRIVATE ${LAS_VIEWER_WINDOWS_CAPTURE_LIBRARIES})
        endif()

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
    if(NOT WIN32)
        set_target_properties(${target_name} PROPERTIES
            BUILD_RPATH "$ORIGIN"
            INSTALL_RPATH "$ORIGIN"
        )
    endif()
endfunction()

function(configure_las_viewer_target target_name use_qtitan)
    configure_las_viewer_common_target(${target_name} ${use_qtitan})
    configure_las_viewer_executable_target(${target_name} ${use_qtitan})
endfunction()
