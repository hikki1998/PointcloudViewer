include_guard(GLOBAL)

function(las_viewer_add_app_sources)
    target_sources(${PROJECT_NAME} PRIVATE ${ARGN})
endfunction()

function(las_viewer_add_smoke_sources)
    target_sources(LASViewerSmokeTest PRIVATE ${ARGN})
endfunction()

function(las_viewer_add_shared_sources)
    target_sources(LASViewerCoreObj PRIVATE ${ARGN})
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

    if(use_qtitan)
        target_include_directories(${target_name} PRIVATE "${QTITAN_INCLUDE_DIR}")
    endif()

    if(PCL_FOUND)
        target_compile_definitions(${target_name} PRIVATE LAS_VIEWER_HAS_PCL=1)
        target_include_directories(${target_name} PRIVATE ${PCL_INCLUDE_DIRS})
    endif()

    if(LAS_VIEWER_ENABLE_LASLIB)
        target_compile_definitions(${target_name} PRIVATE LAS_VIEWER_HAS_LASLIB=1)
        target_include_directories(${target_name} PRIVATE
            "${LASLIB_INCLUDE_DIR}"
            "${LASZIP_INCLUDE_DIR}"
            "${LASZIP_SRC_INCLUDE_DIR}"
        )
    endif()

    if(LAS_VIEWER_HAS_PROJ)
        target_compile_definitions(${target_name} PRIVATE LAS_VIEWER_HAS_PROJ=1 LASVIEWERCRS_HAS_PROJ=1)
        target_include_directories(${target_name} PRIVATE "${PROJ_INCLUDE_DIR}")
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
        target_link_libraries(${target_name} PRIVATE ${PCL_LIBRARIES})
    endif()

    if(LAS_VIEWER_ENABLE_LASLIB)
        target_link_libraries(${target_name} PRIVATE
            "${LASLIB_LIBRARY}"
            "${LASZIP_LIBRARY}"
        )
    endif()

    if(LAS_VIEWER_HAS_PROJ)
        target_link_libraries(${target_name} PRIVATE "${PROJ_LIBRARY}")
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

function(configure_las_viewer_target target_name use_qtitan)
    configure_las_viewer_common_target(${target_name} ${use_qtitan})
    configure_las_viewer_executable_target(${target_name} ${use_qtitan})
endfunction()
