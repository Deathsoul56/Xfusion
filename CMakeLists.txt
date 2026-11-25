cmake_minimum_required(VERSION 3.21)
project(Xfusion VERSION 0.1.0 LANGUAGES C CXX)
if(WIN32)
    enable_language(RC) # the exe icon
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# On Windows the default build is one executable: Qt (MSYS2 package mingw-w64-clang-x86_64-qt6-static), SDL3 and
# the C++ runtime are linked statically. -DXFUSION_STATIC=OFF uses the shared libraries and deploys their DLLs.
option(XFUSION_STATIC "Link Qt, SDL3 and the C++ runtime statically" ${WIN32})
if(XFUSION_STATIC)
    get_filename_component(toolchainPrefix "${CMAKE_CXX_COMPILER}/../.." ABSOLUTE)
    list(PREPEND CMAKE_PREFIX_PATH "${toolchainPrefix}/qt6-static")
    # Qt's own dependencies (freetype, harfbuzz, png, ...) are found as .a files, not as DLL import libraries.
    set(CMAKE_FIND_LIBRARY_SUFFIXES .a)
    set(CMAKE_IMPORT_LIBRARY_SUFFIX .a) # harfbuzz's CMake package builds its path from this
    set(XFUSION_SDL3 SDL3::SDL3-static)
    # FreeType and HarfBuzz, used by static Qt, need these but their CMake packages do not list them.
    find_library(BROTLI_DEC_LIB brotlidec REQUIRED)
    find_library(BROTLI_COMMON_LIB brotlicommon REQUIRED)
    find_library(BZIP2_LIB bz2 REQUIRED)
    find_library(GRAPHITE2_LIB graphite2 REQUIRED)
    set(XFUSION_STATIC_LIBS ${BROTLI_DEC_LIB} ${BROTLI_COMMON_LIB} ${BZIP2_LIB} ${GRAPHITE2_LIB} rpcrt4 usp10)
else()
    set(XFUSION_SDL3 SDL3::SDL3)
endif()

# The renderer uses the RHI of Qt and its shader baker, which are private APIs: the version of Qt is fixed on purpose (ADR-0032).
# The baker (Qt Shader Tools with glslang and SPIRV-Cross) is large: only xfusion-shaderc and the tests carry it, xfusion.exe does not.
set(QT_NO_PRIVATE_MODULE_WARNING ON)
find_package(Qt6 REQUIRED COMPONENTS Widgets Svg Network Test ShaderTools ShaderToolsPrivate)
find_package(SDL3 REQUIRED CONFIG)
find_package(Threads REQUIRED)

set(LIBRETRO_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/external/picodrive/platform/libretro/libretro-common/include"
    CACHE PATH "Directory containing libretro.h")
if(NOT EXISTS "${LIBRETRO_INCLUDE_DIR}/libretro.h")
    message(FATAL_ERROR "libretro.h not found in ${LIBRETRO_INCLUDE_DIR}. Run scripts/setup-picodrive.ps1 first.")
endif()

# Systems, input bindings and joystick reading: no dependency on the window or on any core.
# RetroArch shader presets (ADR-0032, phase B): reading them, and getting their compiled shaders (from the table baked into the program, the
# cache, or xfusion-shaderc).
add_library(xfusion_slang STATIC src/SlangPreset.cpp src/SlangPreset.h src/SlangCompiler.cpp src/SlangCompiler.h src/PresetProgram.cpp src/PresetProgram.h
    src/GpuFilters.cpp src/GpuFilters.h)
# The filters that run on the GPU instead of the CPU (xBRZ, HQx, EPX and the plain scalers): small presets of ours, and the shaders
# of third_party/slang-shaders, are inside the program, as resources under :/gpu-filters/.
file(GLOB XFUSION_GPU_FILTER_OWN RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/src/gpu-filters" "${CMAKE_CURRENT_SOURCE_DIR}/src/gpu-filters/*")
file(GLOB_RECURSE XFUSION_GPU_FILTER_THIRD RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/slang-shaders" "${CMAKE_CURRENT_SOURCE_DIR}/third_party/slang-shaders/*.slang"
     "${CMAKE_CURRENT_SOURCE_DIR}/third_party/slang-shaders/*.inc" "${CMAKE_CURRENT_SOURCE_DIR}/third_party/slang-shaders/*.png")
set(XFUSION_GPU_FILTER_FILES)
foreach(name ${XFUSION_GPU_FILTER_OWN})
    set(path "${CMAKE_CURRENT_SOURCE_DIR}/src/gpu-filters/${name}")
    set_source_files_properties("${path}" PROPERTIES QT_RESOURCE_ALIAS "${name}")
    list(APPEND XFUSION_GPU_FILTER_FILES "${path}")
endforeach()
foreach(name ${XFUSION_GPU_FILTER_THIRD})
    set(path "${CMAKE_CURRENT_SOURCE_DIR}/third_party/slang-shaders/${name}")
    set_source_files_properties("${path}" PROPERTIES QT_RESOURCE_ALIAS "${name}")
    list(APPEND XFUSION_GPU_FILTER_FILES "${path}")
endforeach()
qt_add_resources(xfusion_slang "gpu_filters" PREFIX "/gpu-filters" FILES ${XFUSION_GPU_FILTER_FILES})
target_link_libraries(xfusion_slang PUBLIC Qt6::Core Qt6::Gui Qt6::GuiPrivate ${XFUSION_STATIC_LIBS})
target_include_directories(xfusion_slang PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
# The baker itself, for the programs that compile shaders.
add_library(xfusion_slang_baker STATIC src/SlangBaker.cpp src/SlangBaker.h)
target_link_libraries(xfusion_slang_baker PUBLIC xfusion_slang Qt6::ShaderTools Qt6::ShaderToolsPrivate)

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
    src/AppSettings.cpp
    src/AppSettings.h
    src/Shortcuts.cpp
    src/Shortcuts.h
    src/RewindBuffer.cpp
    src/RewindBuffer.h
    src/VideoFilters.cpp
    src/VideoFilters.h
    third_party/xbrz/xbrz.cpp
    third_party/hqx/hq2x.cpp
    third_party/snes_ntsc/snes_ntsc.c
    # rcheevos, the RetroAchievements runtime (MIT, ADR-0028): without the editor toolkit (raintegration) and the
    # external client, which a player does not need
    third_party/rcheevos/src/rc_client.c
    third_party/rcheevos/src/rc_compat.c
    third_party/rcheevos/src/rc_util.c
    third_party/rcheevos/src/rc_version.c
    third_party/rcheevos/src/rc_libretro.c
    third_party/rcheevos/src/rapi/rc_api_common.c
    third_party/rcheevos/src/rapi/rc_api_info.c
    third_party/rcheevos/src/rapi/rc_api_runtime.c
    third_party/rcheevos/src/rapi/rc_api_user.c
    third_party/rcheevos/src/rcheevos/alloc.c
    third_party/rcheevos/src/rcheevos/condition.c
    third_party/rcheevos/src/rcheevos/condset.c
    third_party/rcheevos/src/rcheevos/consoleinfo.c
    third_party/rcheevos/src/rcheevos/format.c
    third_party/rcheevos/src/rcheevos/lboard.c
    third_party/rcheevos/src/rcheevos/memref.c
    third_party/rcheevos/src/rcheevos/operand.c
    third_party/rcheevos/src/rcheevos/richpresence.c
    third_party/rcheevos/src/rcheevos/runtime.c
    third_party/rcheevos/src/rcheevos/runtime_progress.c
    third_party/rcheevos/src/rcheevos/trigger.c
    third_party/rcheevos/src/rcheevos/value.c
    third_party/rcheevos/src/rhash/aes.c
    third_party/rcheevos/src/rhash/cdreader.c
    third_party/rcheevos/src/rhash/hash.c
    third_party/rcheevos/src/rhash/hash_disc.c
    third_party/rcheevos/src/rhash/hash_encrypted.c
    third_party/rcheevos/src/rhash/hash_rom.c
    third_party/rcheevos/src/rhash/hash_zip.c
    third_party/rcheevos/src/rhash/md5.c
    # libchdr (BSD-3), to read the CHD disc images: the same files as upstream, without the micro-flac backend and with
    # the system zlib and zstd (ADR-0029)
    third_party/libchdr/src/libchdr_bitstream.c
    third_party/libchdr/src/libchdr_cdrom.c
    third_party/libchdr/src/libchdr_chd.c
    third_party/libchdr/src/libchdr_codec_avhuff.c
    third_party/libchdr/src/libchdr_codec_cdfl.c
    third_party/libchdr/src/libchdr_codec_cdlz.c
    third_party/libchdr/src/libchdr_codec_cdzl.c
    third_party/libchdr/src/libchdr_codec_cdzs.c
    third_party/libchdr/src/libchdr_codec_flac.c
    third_party/libchdr/src/libchdr_codec_huff.c
    third_party/libchdr/src/libchdr_codec_lzma.c
    third_party/libchdr/src/libchdr_codec_zlib.c
    third_party/libchdr/src/libchdr_codec_zstd.c
    third_party/libchdr/src/libchdr_flac.c
    third_party/libchdr/src/libchdr_huffman.c
    third_party/libchdr/deps/lzma-26.02/src/LzmaDec.c
    src/GameSaves.cpp
    src/Library.cpp
    src/Library.h
    src/CoreRules.cpp
    src/CoreRules.h
    src/CoreProfile.cpp
    src/CoreProfile.h
    src/CoreManifest.cpp
    src/CoreManifest.h
    src/CoreProbe.cpp
    src/CoreProbe.h
    src/AchievementsToken.cpp
    src/AchievementsToken.h
    src/AchievementsConsole.cpp
    src/AchievementsConsole.h
    src/ChdDisc.cpp
    src/ChdDisc.h
    src/AchievementsDisc.cpp
    src/AchievementsDisc.h
    src/Patch.cpp
    src/Patch.h
    src/RomInfo.cpp
    src/RomInfo.h
    src/ZipRead.cpp
    src/ZipRead.h
    src/GameHeader.cpp
    src/GameHeader.h
    src/DiscInfo.cpp
    src/DiscInfo.h
    src/Bios.cpp
    src/Bios.h
    src/GameSaves.h
    src/StateFile.cpp
    src/StateFile.h
    src/DisplayLayout.cpp
    src/DisplayLayout.h
)
target_include_directories(xfusion_input PUBLIC src third_party third_party/rcheevos/include third_party/rcheevos/src third_party/libchdr/include)
find_library(XFUSION_ZSTD_LIB zstd REQUIRED)
target_link_libraries(xfusion_input PUBLIC ${XFUSION_ZSTD_LIB})
file(GLOB XFUSION_CHDR_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/third_party/libchdr/src/*.c)
set_source_files_properties(${XFUSION_CHDR_SOURCES} PROPERTIES COMPILE_DEFINITIONS "CHDR_SYSTEM_ZLIB;CHDR_SYSTEM_ZSTD")
target_include_directories(xfusion_input PRIVATE "${LIBRETRO_INCLUDE_DIR}")
target_compile_definitions(xfusion_input PUBLIC RC_CLIENT_SUPPORTS_HASH)
target_link_libraries(xfusion_input PUBLIC Qt6::Gui ${XFUSION_SDL3} ${XFUSION_STATIC_LIBS})

add_executable(xfusion
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindowGames.cpp
    src/MainWindowStates.cpp
    src/MainWindowView.cpp
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
    src/LibraryPage.cpp
    src/LibraryPage.h
    src/SettingsDialog.cpp
    src/SettingsDialog.h
    src/DisplayPage.cpp
    src/DisplayPage.h
    src/SoundPage.cpp
    src/SoundPage.h
    src/GeneralPage.cpp
    src/GeneralPage.h
    src/EmulationPage.cpp
    src/EmulationPage.h
    src/FilesPage.cpp
    src/FilesPage.h
    src/ShortcutsPage.cpp
    src/ShortcutsPage.h
    src/StatePreviewDialog.cpp
    src/StatePreviewDialog.h
    src/OsdOverlay.cpp
    src/OsdOverlay.h
    src/BindingIcons.cpp
    src/BindingIcons.h
    src/Icons.cpp
    src/Icons.h
    src/Theme.cpp
    src/Theme.h
    src/EmuThread.cpp
    src/EmuThread.h
    src/CoreInstaller.cpp
    src/CoreInstaller.h
    src/Achievements.cpp
    src/Achievements.h
    src/AchievementsPage.cpp
    src/AchievementsPage.h
    src/AchievementsDialogs.cpp
    src/AchievementsDialogs.h
    src/PasswordDigestor.cpp
    src/ScopedPage.cpp
    src/ScopedPage.h
    src/GpuCanvas.cpp
    src/GpuCanvas.h
    src/PresetRenderer.cpp
    src/PresetRenderer.h
    src/ShaderParametersDialog.cpp
    src/ShaderParametersDialog.h
    src/CoresDialog.cpp
    src/CoresDialog.h
)
target_include_directories(xfusion PRIVATE "${LIBRETRO_INCLUDE_DIR}")
target_compile_definitions(xfusion PRIVATE XFUSION_VERSION="${PROJECT_VERSION}")
if(WIN32)
    target_link_libraries(xfusion PRIVATE winmm) # the chime of an unlocked achievement
    configure_file(src/resources/xfusion.rc.in "${CMAKE_CURRENT_BINARY_DIR}/xfusion.rc" @ONLY)
    target_sources(xfusion PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/xfusion.rc")
    set_target_properties(xfusion PROPERTIES WIN32_EXECUTABLE ON) # no console window
endif()
target_link_libraries(xfusion PRIVATE xfusion_input xfusion_slang Qt6::Widgets Qt6::GuiPrivate Qt6::Svg Qt6::Network ${XFUSION_SDL3} Threads::Threads)
# xfusion-shaderc compiles shaders: the ones of the filters that come with the program, now (they go into xfusion.exe as a resource),
# and the ones of presets of the user, when xfusion.exe asks it to, from the folder it is in.
add_executable(xfusion-shaderc tools/shaderc.cpp)
target_link_libraries(xfusion-shaderc PRIVATE xfusion_slang_baker xfusion_input)
set(XFUSION_BAKED_SHADERS "${CMAKE_CURRENT_BINARY_DIR}/baked-shaders.bin")
add_custom_command(OUTPUT "${XFUSION_BAKED_SHADERS}" COMMAND xfusion-shaderc bake "${XFUSION_BAKED_SHADERS}"
    DEPENDS xfusion-shaderc ${XFUSION_GPU_FILTER_FILES} COMMENT "Compiling the shaders of the GPU filters")
set_source_files_properties("${XFUSION_BAKED_SHADERS}" PROPERTIES GENERATED TRUE QT_RESOURCE_ALIAS "shaders.bin")
qt_add_resources(xfusion "baked_shaders" PREFIX "/baked" FILES "${XFUSION_BAKED_SHADERS}")
add_dependencies(xfusion xfusion-shaderc)

# The shaders of the GPU renderer (ADR-0032), compiled for every backend when the program is built.
qt_add_shaders(xfusion "video_shaders" PREFIX "/shaders" BASE "${CMAKE_CURRENT_SOURCE_DIR}/src/shaders"
    FILES src/shaders/video.vert src/shaders/video_plain.frag src/shaders/video_scanlines.frag src/shaders/video_crt.frag
          src/shaders/video_sharp.frag src/shaders/video_lcd.frag)

# `cmake --install build --prefix build` leaves a runnable build\xfusion.exe; another prefix makes a portable folder.
if(WIN32)
    install(TARGETS xfusion xfusion-shaderc RUNTIME DESTINATION .)
endif()

# The shared build also needs the Qt plugins and every DLL the exe depends on next to it.
if(WIN32 AND NOT XFUSION_STATIC)
    install(CODE [[
        execute_process(
            COMMAND "$<TARGET_FILE:Qt6::windeployqt>" --release --no-translations --no-system-d3d-compiler
                    --no-opengl-sw --no-compiler-runtime --skip-plugin-types generic,imageformats,networkinformation,tls
                    --dir "${CMAKE_INSTALL_PREFIX}" "$<TARGET_FILE:xfusion>"
            RESULT_VARIABLE deployResult ERROR_VARIABLE deployError OUTPUT_QUIET)
        if(deployResult)
            message(FATAL_ERROR "windeployqt failed: ${deployError}")
        endif()

        # windeployqt copies Qt only; the MSYS2 libraries Qt and the exe depend on are resolved here.
        if(POLICY CMP0207)
            cmake_policy(SET CMP0207 NEW)
        endif()
        file(GET_RUNTIME_DEPENDENCIES
            EXECUTABLES "$<TARGET_FILE:xfusion>"
            RESOLVED_DEPENDENCIES_VAR resolved
            UNRESOLVED_DEPENDENCIES_VAR unresolved
            DIRECTORIES "$<TARGET_FILE_DIR:SDL3::SDL3>"
            PRE_EXCLUDE_REGEXES "api-ms-.*" "ext-ms-.*"
            POST_EXCLUDE_REGEXES ".*/[Ss]ystem32/.*")
        file(INSTALL ${resolved} DESTINATION "${CMAKE_INSTALL_PREFIX}")
    ]])
endif()

enable_testing()
add_executable(input_tests tests/InputTests.cpp)
add_executable(settings_tests tests/SettingsTests.cpp)
add_executable(save_tests tests/SaveTests.cpp)
add_executable(library_tests tests/LibraryTests.cpp)
add_executable(disc_tests tests/DiscTests.cpp)
add_executable(patch_tests tests/PatchTests.cpp)
add_executable(core_tests tests/CoreTests.cpp src/PasswordDigestor.cpp)
target_link_libraries(core_tests PRIVATE Qt6::Network)
add_executable(preset_scan tests/PresetScan.cpp)
target_link_libraries(preset_scan PRIVATE xfusion_slang_baker)
if(XFUSION_STATIC)
    target_link_options(preset_scan PRIVATE -static)
endif()
add_executable(shader_tests tests/ShaderTests.cpp)
target_link_libraries(shader_tests PRIVATE xfusion_slang_baker)
foreach(testTarget input_tests settings_tests save_tests library_tests disc_tests patch_tests core_tests shader_tests)
    target_link_libraries(${testTarget} PRIVATE xfusion_input Qt6::Test)
    add_test(NAME ${testTarget} COMMAND ${testTarget})
    set_tests_properties(${testTarget} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endforeach()
if(XFUSION_STATIC)
    target_link_options(xfusion PRIVATE -static $<$<CONFIG:Release>:-s>)
    target_link_options(input_tests PRIVATE -static)
    target_link_options(settings_tests PRIVATE -static)
    target_link_options(save_tests PRIVATE -static)
    target_link_options(library_tests PRIVATE -static)
    target_link_options(disc_tests PRIVATE -static)
    target_link_options(patch_tests PRIVATE -static)
    target_link_options(core_tests PRIVATE -static)
    target_link_options(shader_tests PRIVATE -static)
    target_link_options(xfusion-shaderc PRIVATE -static $<$<CONFIG:Release>:-s>)
    # Static Qt links its plugins into the exe; take only the ones used instead of every default one
    # (image formats, TLS, ... would pull in more libraries).
    qt_import_plugins(xfusion NO_DEFAULT INCLUDE Qt6::QWindowsIntegrationPlugin Qt6::QModernWindowsStylePlugin Qt6::QSvgPlugin Qt6::QSchannelBackendPlugin Qt6::QJpegPlugin)
    qt_import_plugins(input_tests NO_DEFAULT INCLUDE Qt6::QOffscreenIntegrationPlugin)
    qt_import_plugins(settings_tests NO_DEFAULT INCLUDE Qt6::QOffscreenIntegrationPlugin)
    qt_import_plugins(save_tests NO_DEFAULT INCLUDE Qt6::QOffscreenIntegrationPlugin)
    qt_import_plugins(library_tests NO_DEFAULT INCLUDE Qt6::QOffscreenIntegrationPlugin)
    qt_import_plugins(disc_tests NO_DEFAULT INCLUDE Qt6::QOffscreenIntegrationPlugin)
    qt_import_plugins(patch_tests NO_DEFAULT INCLUDE Qt6::QOffscreenIntegrationPlugin)
    qt_import_plugins(core_tests NO_DEFAULT INCLUDE Qt6::QOffscreenIntegrationPlugin)
    qt_import_plugins(shader_tests NO_DEFAULT INCLUDE Qt6::QOffscreenIntegrationPlugin)
    qt_import_plugins(preset_scan NO_DEFAULT INCLUDE Qt6::QJpegPlugin)
    qt_import_plugins(xfusion-shaderc NO_DEFAULT)
endif()
