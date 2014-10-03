project(ktexteditor_multicursor)

cmake_minimum_required(VERSION 2.8)

find_package(KDE4 REQUIRED)
include(KDE4Defaults)
include_directories(${KDE4_INCLUDES} ${QT_INCLUDES})
add_definitions(${QT_DEFINITIONS} ${KDE4_DEFINITIONS})

set(ktexteditor_multicursor_SRCS multicursorconfig.cpp multicursor.cpp)

kde4_add_plugin(ktexteditor_multicursor ${ktexteditor_multicursor_SRCS})

target_link_libraries(ktexteditor_multicursor ${KDE4_KDEUI_LIBS} ${KDE4_KTEXTEDITOR_LIBS})

install(TARGETS ktexteditor_multicursor DESTINATION ${PLUGIN_INSTALL_DIR})

install(FILES multicursorui.rc DESTINATION ${DATA_INSTALL_DIR}/ktexteditor_multicursor)

install(FILES ktexteditor_multicursor.desktop ktexteditor_multicursor_config.desktop DESTINATION ${SERVICES_INSTALL_DIR})

add_definitions(-std=c++11)
