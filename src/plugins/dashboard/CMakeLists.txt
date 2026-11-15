cmake_minimum_required(VERSION 3.24)

project(dashboard)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Svg Network WebSockets)
qt_standard_project_setup()

set(DASHBOARD_CORE_SRC
    config/layout_schema.cpp
    config/layout_schema.hpp
    providers/ha_client.cpp
    providers/ha_client.hpp
    providers/system_sensors.cpp
    providers/system_sensors.hpp
    ui/ninepatch.cpp
    ui/ninepatch.hpp
    ui/icon_helper.cpp
    ui/icon_helper.hpp
    ui/dashboard_view.cpp
    ui/dashboard_view.hpp
    ui/cards/card_widget.hpp
    ui/cards/btop_bar_card.cpp
    ui/cards/btop_bar_card.hpp
    ui/cards/clock_card.cpp
    ui/cards/clock_card.hpp
    ui/cards/ha_sensor_card.cpp
    ui/cards/ha_sensor_card.hpp
    ui/cards/net_card.cpp
    ui/cards/net_card.hpp
    ui/cards/placeholder_card.cpp
    ui/cards/placeholder_card.hpp
    ui/cards/sensor_dot_card.cpp
    ui/cards/sensor_dot_card.hpp
    ui/cards/weather_card.cpp
    ui/cards/weather_card.hpp
    dashboard_plugin_base.cpp
    dashboard_plugin_base.hpp
    ../../gscreen.cpp
    ../../gscreen.hpp
    ../../plugininterface.hpp
)

qt_add_resources(DASHBOARD_QRC dashboard.qrc)

add_library(dashboard_core OBJECT ${DASHBOARD_CORE_SRC})
set_target_properties(dashboard_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(dashboard_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)
target_link_libraries(dashboard_core PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Svg
    Qt6::Network
    Qt6::WebSockets
)

function(add_g19_dashboard_plugin TARGET_NAME PLUGIN_ID PLUGIN_NAME CONFIG_FILE)
    get_filename_component(ABS_CONFIG_FILE "${CONFIG_FILE}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

    set(DASH_TARGET "${TARGET_NAME}")
    set(DASH_PLUGIN_ID "${PLUGIN_ID}")
    set(DASH_PLUGIN_NAME "${PLUGIN_NAME}")
    set(DASH_CLASS_NAME "DashboardPlugin_${TARGET_NAME}")
    set(DASH_BASE_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/dashboard_plugin_base.hpp")
    set(DASH_ENTRY_HEADER "${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}_entry.hpp")
    set(DASH_ENTRY_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}_entry.cpp")
    set(DASH_CONFIG_QRC "${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}_config.qrc")

    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/plugin_entry.hpp.in"
        "${DASH_ENTRY_HEADER}"
        @ONLY
    )
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/plugin_entry.cpp.in"
        "${DASH_ENTRY_SOURCE}"
        @ONLY
    )

    file(WRITE "${DASH_CONFIG_QRC}"
"<RCC>
    <qresource prefix=\"/${TARGET_NAME}_config\">
        <file alias=\"layout.json\">${ABS_CONFIG_FILE}</file>
    </qresource>
</RCC>
")

    qt_add_resources(TARGET_CONFIG_QRC "${DASH_CONFIG_QRC}")

    qt_add_library(${TARGET_NAME} SHARED
        ${DASH_ENTRY_HEADER}
        ${DASH_ENTRY_SOURCE}
        ${TARGET_CONFIG_QRC}
        ${DASHBOARD_QRC}
    )
    target_link_libraries(${TARGET_NAME} PRIVATE
        dashboard_core
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Svg
        Qt6::Network
        Qt6::WebSockets
    )
    target_include_directories(${TARGET_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../..
        ${CMAKE_CURRENT_BINARY_DIR}
    )

    install(TARGETS ${TARGET_NAME} LIBRARY DESTINATION ${PLUGINS_DIR})
endfunction()

# Default dashboard plugin
add_g19_dashboard_plugin(dashboard "dashboard" "Dashboard" "configs/default_dashboard.json")

# Optional extra build-time dashboard variants:
# cmake -DDASHBOARD_EXTRA_CONFIGS="homelab:Homelab:/path/to/homelab.json" ..
set(DASHBOARD_EXTRA_CONFIGS "" CACHE STRING "Semicolon-separated list of id:DisplayName:config.json entries")
foreach(ENTRY IN LISTS DASHBOARD_EXTRA_CONFIGS)
    string(REPLACE ":" ";" PARTS "${ENTRY}")
    list(LENGTH PARTS PARTS_LEN)
    if(PARTS_LEN EQUAL 3)
        list(GET PARTS 0 EXTRA_ID)
        list(GET PARTS 1 EXTRA_NAME)
        list(GET PARTS 2 EXTRA_CFG)
        add_g19_dashboard_plugin(${EXTRA_ID} "${EXTRA_ID}" "${EXTRA_NAME}" "${EXTRA_CFG}")
    else()
        message(WARNING "Ignoring malformed DASHBOARD_EXTRA_CONFIGS entry: ${ENTRY}")
    endif()
endforeach()

# Standalone desktop preview tool for local development
add_executable(dashboard_preview
    preview/main.cpp
    preview/preview_window.cpp
    preview/preview_window.hpp
    ${DASHBOARD_QRC}
)
target_link_libraries(dashboard_preview PRIVATE
    dashboard_core
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Svg
    Qt6::Network
    Qt6::WebSockets
)
