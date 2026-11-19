cmake_minimum_required(VERSION 3.21)
project(Xfusion VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(SDL3 REQUIRED CONFIG)

set(LIBRETRO_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/external/picodrive/platform/libretro/libretro-common/include"
    CACHE PATH "Directory containing libretro.h")
if(NOT EXISTS "${LIBRETRO_INCLUDE_DIR}/libretro.h")
    message(FATAL_ERROR "libretro.h not found in ${LIBRETRO_INCLUDE_DIR}. Run scripts/setup-picodrive.ps1 first.")
endif()

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
    src/SystemInfo.cpp
    src/SystemInfo.h
    src/AppPaths.cpp
    src/AppPaths.h
    src/CoreProfile.cpp
    src/CoreProfile.h
)
target_include_directories(xfusion PRIVATE "${LIBRETRO_INCLUDE_DIR}")
target_link_libraries(xfusion PRIVATE Qt6::Widgets SDL3::SDL3)
