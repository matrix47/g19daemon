cmake_minimum_required(VERSION 3.24)

project(yad_plugin_wrap LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

get_filename_component(YAD_DEFAULT_DIR "${CMAKE_SOURCE_DIR}/../yad" ABSOLUTE)
set(YAD_DIR "${YAD_DEFAULT_DIR}" CACHE PATH "Path to external yad repository")

if(NOT EXISTS "${YAD_DIR}/CMakeLists.txt")
    message(WARNING "yad repository not found at '${YAD_DIR}'; skipping yad plugin")
    return()
endif()

set(YAD_BUILD_POSIX ON CACHE BOOL "" FORCE)
set(YAD_BUILD_PREVIEW OFF CACHE BOOL "" FORCE)
add_subdirectory("${YAD_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/yad_ext" EXCLUDE_FROM_ALL)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Svg Network WebSockets)

qt_add_resources(YAD_PLUGIN_QRC yad.qrc)

qt_add_library(yad_plugin SHARED
    yad_plugin.cpp
    yad_plugin.hpp
    ../../gscreen.cpp
    ../../gscreen.hpp
    ../../plugininterface.hpp
    ${YAD_PLUGIN_QRC}
)
set_target_properties(yad_plugin PROPERTIES OUTPUT_NAME "yad")
target_compile_definitions(yad_plugin PRIVATE
    YAD_INSTALLED_CONFIG_PATH="${CMAKE_INSTALL_PREFIX}/share/g19daemon/yad/default_dashboard.json"
    YAD_FALLBACK_CONFIG_PATH="${YAD_DIR}/assets/configs/default_dashboard.json"
)
target_include_directories(yad_plugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)
target_link_libraries(yad_plugin PRIVATE
    yad_posix
    yad
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

install(TARGETS yad_plugin LIBRARY DESTINATION ${PLUGINS_DIR})
install(FILES "${YAD_DIR}/assets/configs/default_dashboard.json"
    DESTINATION share/g19daemon/yad)

if(TARGET dashboard_core)
    qt_add_resources(YAD_COMPARE_DASH_QRC ../dashboard/dashboard.qrc)
    add_executable(yad_compare yad_compare.cpp ${YAD_COMPARE_DASH_QRC})
    target_compile_definitions(yad_compare PRIVATE
        YAD_COMPARE_DEFAULT_CONFIG="${YAD_DIR}/assets/configs/default_dashboard.json"
    )
    target_include_directories(yad_compare PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_CURRENT_SOURCE_DIR}/../..
    )
    target_link_libraries(yad_compare PRIVATE
        dashboard_core
        yad
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Svg
        Qt6::Network
        Qt6::WebSockets
    )
endif()
