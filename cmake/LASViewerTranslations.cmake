include_guard(GLOBAL)

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
