cmake_minimum_required(VERSION 3.21)
project(eda_ui_prototype LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Try regular discovery first.
find_package(QT NAMES Qt6 Qt5 COMPONENTS Widgets QUIET)

# Fallback for local Windows installs (for example C:/Qt/*/*).
if(NOT QT_FOUND)
    set(_qt_hint_prefixes "")

    if(DEFINED ENV{CMAKE_PREFIX_PATH} AND NOT "$ENV{CMAKE_PREFIX_PATH}" STREQUAL "")
        list(APPEND _qt_hint_prefixes "$ENV{CMAKE_PREFIX_PATH}")
    endif()

    if(DEFINED ENV{Qt6_DIR} AND NOT "$ENV{Qt6_DIR}" STREQUAL "")
        get_filename_component(_qt6_prefix "$ENV{Qt6_DIR}/../../.." ABSOLUTE)
        list(APPEND _qt_hint_prefixes "${_qt6_prefix}")
    endif()

    file(GLOB _qt6_configs "C:/Qt/*/*/lib/cmake/Qt6/Qt6Config.cmake")
    foreach(_qt_cfg IN LISTS _qt6_configs)
        get_filename_component(_qt_prefix "${_qt_cfg}" DIRECTORY)
        get_filename_component(_qt_prefix "${_qt_prefix}" DIRECTORY)
        get_filename_component(_qt_prefix "${_qt_prefix}" DIRECTORY)
        get_filename_component(_qt_prefix "${_qt_prefix}" DIRECTORY)
        list(APPEND _qt_hint_prefixes "${_qt_prefix}")
    endforeach()

    list(REMOVE_DUPLICATES _qt_hint_prefixes)
    foreach(_qt_prefix IN LISTS _qt_hint_prefixes)
        if(EXISTS "${_qt_prefix}")
            list(APPEND CMAKE_PREFIX_PATH "${_qt_prefix}")
        endif()
    endforeach()

    if(_qt_hint_prefixes)
        message(STATUS "Qt hint prefixes: ${_qt_hint_prefixes}")
    endif()

    find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
endif()

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

add_executable(eda_ui_prototype
    src/app/main.cpp
    src/app/MainWindow.h
    src/app/MainWindow.cpp
    src/app/GraphView.h
    src/app/GraphView.cpp
    src/scene/EditorScene.h
    src/scene/EditorScene.cpp
    src/panels/ProjectTreePanel.h
    src/panels/ProjectTreePanel.cpp
    src/panels/PropertyPanel.h
    src/panels/PropertyPanel.cpp
    src/panels/PalettePanel.h
    src/panels/PalettePanel.cpp
    src/items/NodeItem.h
    src/items/NodeItem.cpp
    src/items/PortItem.h
    src/items/PortItem.cpp
    src/items/EdgeItem.h
    src/items/EdgeItem.cpp
    src/model/GraphDocument.h
    src/model/ComponentCatalog.h
    src/model/ComponentCatalog.cpp
    src/model/GraphSerializer.h
    src/model/GraphSerializer.cpp
    src/commands/DocumentStateCommand.h
    src/commands/DocumentStateCommand.cpp
    src/commands/NodeEditCommands.h
    src/commands/NodeEditCommands.cpp
)

target_include_directories(eda_ui_prototype PRIVATE src)
target_link_libraries(eda_ui_prototype PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)

include(CTest)

if(BUILD_TESTING)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Test)

    add_executable(eda_tests
        tests/test_suite.cpp
        src/model/GraphDocument.h
        src/model/ComponentCatalog.h
        src/model/ComponentCatalog.cpp
        src/model/GraphSerializer.h
        src/model/GraphSerializer.cpp
        src/app/MainWindow.h
        src/app/MainWindow.cpp
        src/app/GraphView.h
        src/app/GraphView.cpp
        src/panels/ProjectTreePanel.h
        src/panels/ProjectTreePanel.cpp
        src/panels/PropertyPanel.h
        src/panels/PropertyPanel.cpp
        src/panels/PalettePanel.h
        src/panels/PalettePanel.cpp
        src/scene/EditorScene.h
        src/scene/EditorScene.cpp
        src/items/NodeItem.h
        src/items/NodeItem.cpp
        src/items/PortItem.h
        src/items/PortItem.cpp
        src/items/EdgeItem.h
        src/items/EdgeItem.cpp
        src/commands/DocumentStateCommand.h
        src/commands/DocumentStateCommand.cpp
        src/commands/NodeEditCommands.h
        src/commands/NodeEditCommands.cpp
    )

    target_include_directories(eda_tests PRIVATE src)
    target_link_libraries(eda_tests PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Test)
    add_test(NAME eda_tests COMMAND eda_tests)

    if(WIN32 AND QT_VERSION_MAJOR EQUAL 6)
        get_target_property(_qt_qmake_exe_tests Qt6::qmake IMPORTED_LOCATION)
        get_filename_component(_qt_bin_dir_tests "${_qt_qmake_exe_tests}" DIRECTORY)
        find_program(WINDEPLOYQT_EXECUTABLE_TESTS windeployqt HINTS "${_qt_bin_dir_tests}")
        if(WINDEPLOYQT_EXECUTABLE_TESTS)
            add_custom_command(TARGET eda_tests POST_BUILD
                COMMAND "${WINDEPLOYQT_EXECUTABLE_TESTS}" --no-translations --no-compiler-runtime "$<TARGET_FILE:eda_tests>"
                COMMENT "Deploying Qt runtime for tests with windeployqt"
                VERBATIM
            )
        endif()
    endif()
endif()

if(WIN32 AND QT_VERSION_MAJOR EQUAL 6)
    get_target_property(_qt_qmake_exe Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin_dir "${_qt_qmake_exe}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}")

    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET eda_ui_prototype POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}" --no-translations --no-compiler-runtime "$<TARGET_FILE:eda_ui_prototype>"
            COMMENT "Deploying Qt runtime with windeployqt"
            VERBATIM
        )
    endif()
endif()
