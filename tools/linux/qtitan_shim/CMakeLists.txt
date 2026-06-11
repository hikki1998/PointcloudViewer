cmake_minimum_required(VERSION 3.16)

project(QtitanRibbonShim VERSION 2026.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

find_package(Qt5 5.15 REQUIRED COMPONENTS Core Gui Widgets)

add_library(qtnribbon4 SHARED
    QtitanRibbonShim.cpp
    QtitanRibbonShim.h
)

target_link_libraries(qtnribbon4 PRIVATE Qt5::Core Qt5::Gui Qt5::Widgets)
target_compile_definitions(qtnribbon4 PRIVATE QTITAN_SHIM_LIBRARY=1)
target_include_directories(qtnribbon4 PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<INSTALL_INTERFACE:include>"
)

set_target_properties(qtnribbon4 PROPERTIES
    OUTPUT_NAME qtnribbon4
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

set(qtitan_shim_headers
    QtitanDef.h
    QtitanRibbon.h
    QtnRibbonBackstageView.h
    QtnRibbonBar.h
    QtnRibbonGroup.h
    QtnRibbonMainWindow.h
    QtnRibbonPage.h
    QtnRibbonQuickAccessBar.h
    QtnRibbonStyle.h
    QtnRibbonSystemPopupBar.h
    QtnRibbonToolTip.h
    QtitanRibbonShim.h
)

install(TARGETS qtnribbon4 LIBRARY DESTINATION bin)
install(FILES ${qtitan_shim_headers} DESTINATION include)
install(CODE "file(WRITE \"${CMAKE_INSTALL_PREFIX}/.version\" \"NAME=QtitanRibbon\\nVERSION=2026.1.0-linux-shim\\n\")")
