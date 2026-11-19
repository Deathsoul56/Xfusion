cmake_minimum_required(VERSION 3.21)
project(Xfusion VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Test)
find_package(SDL3 REQUIRED CONFIG)
find_package(Threads REQUIRED)

set(LIBRETRO_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/external/picodrive/platform/libretro/libretro-common/include"
    CACHE PATH "Directory containing libretro.h")
if(NOT EXISTS "${LIBRETRO_INCLUDE_DIR}/libretro.h")
    message(FATAL_ERROR "libretro.h not found in ${LIBRETRO_INCLUDE_DIR}. Run scripts/setup-picodrive.ps1 first.")
endif()

# Systems, input bindings and joystick reading: no dependency on the window or on any core.
add_library(xfusion_input STATIC
    src/PadButton.h
    src/SystemInfo.cpp
    src/SystemInfo.h
    src/InputBinding.cpp
    src/InputBinding.h
    src/InputConfig.cpp
    src/InputConfig.h
    src/InputManager.cpp
    src/InputManager.h
)
target_include_directories(xfusion_input PUBLIC src)
target_link_libraries(xfusion_input PUBLIC Qt6::Gui SDL3::SDL3)

add_executable(xfusion
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/LibretroCore.cpp
    src/LibretroCore.h
    src/AudioOutput.cpp
    src/AudioOutput.h
    src/VideoWidget.cpp
    src/VideoWidget.h
    src/AppPaths.cpp
    src/AppPaths.h
    src/ControllerDiagram.cpp
    src/ControllerDiagram.h
    src/resources/resources.qrc
    src/ControllersPage.cpp
    src/ControllersPage.h
    src/SettingsDialog.cpp
    src/SettingsDialog.h
    src/CoreProfile.cpp
    src/CoreProfile.h
    src/EmuThread.cpp
    src/EmuThread.h
)
target_include_directories(xfusion PRIVATE "${LIBRETRO_INCLUDE_DIR}")
target_link_libraries(xfusion PRIVATE xfusion_input Qt6::Widgets SDL3::SDL3 Threads::Threads)

enable_testing()
add_executable(input_tests tests/InputTests.cpp)
target_link_libraries(input_tests PRIVATE xfusion_input Qt6::Test)
add_test(NAME input_tests COMMAND input_tests)
set_tests_properties(input_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
