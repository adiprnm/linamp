cmake_minimum_required(VERSION 3.21)

project(Linamp VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ---- SDL3 ------------------------------------------------------------------
# Video, input, drag & drop, file dialogs, audio output and timers.
find_package(SDL3 3.2 QUIET)
if(TARGET SDL3::SDL3-static)
    # static SDL3 build (packaging/build-sdl3.sh) -> fully self-contained binary
    set(LINAMP_SDL_TARGET SDL3::SDL3-static)
elseif(SDL3_FOUND)
    set(LINAMP_SDL_TARGET SDL3::SDL3)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL3Pkg REQUIRED IMPORTED_TARGET sdl3)
    set(LINAMP_SDL_TARGET PkgConfig::SDL3Pkg)
endif()

# ---- FFmpeg (decode only) --------------------------------------------------
# A slim static build (packaging/build-ffmpeg.sh) is strongly recommended:
# linking the distro build pulls in every video codec, which multiplies the
# package size. Pass -DFFMPEG_ROOT=<prefix> to use a static build.
set(FFMPEG_ROOT "" CACHE PATH "Prefix of a static FFmpeg build (include/, lib/)")

macro(linamp_find_ffmpeg_static root)
    set(_ok TRUE)
    foreach(_lib avformat avcodec avutil swresample)
        find_path(${_lib}_INCLUDE_DIR NAMES lib${_lib}/${_lib}.h
            HINTS "${root}" PATH_SUFFIXES include NO_DEFAULT_PATH)
        find_library(${_lib}_LIBRARY NAMES ${_lib}
            HINTS "${root}" PATH_SUFFIXES lib lib64 NO_DEFAULT_PATH)
        if(NOT ${_lib}_INCLUDE_DIR OR NOT ${_lib}_LIBRARY)
            set(_ok FALSE)
        endif()
        list(APPEND FFMPEG_INCLUDE_DIRS ${${_lib}_INCLUDE_DIR})
        list(APPEND FFMPEG_LIBRARIES ${${_lib}_LIBRARY})
    endforeach()
    set(FFMPEG_FOUND ${_ok})
endmacro()

if(FFMPEG_ROOT)
    linamp_find_ffmpeg_static("${FFMPEG_ROOT}")
    if(NOT FFMPEG_FOUND)
        message(FATAL_ERROR "No FFmpeg found under FFMPEG_ROOT=${FFMPEG_ROOT}")
    endif()
else()
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(FFMPEG QUIET IMPORTED_TARGET
            libavformat libavcodec libavutil libswresample)
    endif()
    if(NOT FFMPEG_FOUND)
        linamp_find_ffmpeg_static("$ENV{FFMPEG_ROOT}")
    endif()
    if(NOT FFMPEG_FOUND)
        message(FATAL_ERROR
            "FFmpeg development files not found. Install the distro -dev packages "
            "or build the slim static one with packaging/build-ffmpeg.sh and pass "
            "-DFFMPEG_ROOT=<prefix>.")
    endif()
endif()

# ---- Application -----------------------------------------------------------
add_executable(linamp
    src/main.cpp
    src/app.cpp
    src/app.h
    src/engine.cpp
    src/engine.h
    src/prober.cpp
    src/prober.h
    src/ui.cpp
    src/ui.h
    src/gfx.cpp
    src/gfx.h
    src/font.cpp
    src/font.h
    src/text.cpp
    src/text.h
    src/third_party/stb_truetype.h
    src/json.cpp
    src/json.h
    src/util.cpp
    src/util.h
    src/theme.h
)

target_include_directories(linamp PRIVATE src src/third_party)
target_compile_definitions(linamp PRIVATE LINAMP_VERSION="${PROJECT_VERSION}")
target_link_libraries(linamp PRIVATE ${LINAMP_SDL_TARGET})

if(TARGET PkgConfig::FFMPEG)
    target_link_libraries(linamp PRIVATE PkgConfig::FFMPEG)
else()
    target_include_directories(linamp PRIVATE ${FFMPEG_INCLUDE_DIRS})
    target_link_libraries(linamp PRIVATE ${FFMPEG_LIBRARIES})
    if(UNIX)
        target_link_libraries(linamp PRIVATE m pthread)
    endif()
    if(WIN32)
        target_link_libraries(linamp PRIVATE ws2_32 bcrypt)
    endif()
endif()

if(MSVC)
    target_compile_options(linamp PRIVATE /W3)
else()
    target_compile_options(linamp PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

if(WIN32)
    set_target_properties(linamp PROPERTIES WIN32_EXECUTABLE TRUE)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/linamp.ico")
        file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/assets/linamp.ico" LINAMP_ICON_PATH)
        configure_file(packaging/linamp.rc.in "${CMAKE_CURRENT_BINARY_DIR}/linamp.rc" @ONLY)
        target_sources(linamp PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/linamp.rc")
    endif()
endif()

if(APPLE)
    set_target_properties(linamp PROPERTIES
        OUTPUT_NAME "Linamp"
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "Linamp"
        MACOSX_BUNDLE_GUI_IDENTIFIER "local.linamp.app"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    )
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/linamp.icns")
        set(LINAMP_ICNS "${CMAKE_CURRENT_SOURCE_DIR}/assets/linamp.icns")
        set_source_files_properties("${LINAMP_ICNS}" PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
        target_sources(linamp PRIVATE "${LINAMP_ICNS}")
        set_target_properties(linamp PROPERTIES MACOSX_BUNDLE_ICON_FILE "linamp.icns")
    endif()
endif()

# ---- Install ---------------------------------------------------------------
include(GNUInstallDirs)
install(TARGETS linamp
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(UNIX AND NOT APPLE)
    install(FILES packaging/linamp.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES assets/icon.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/512x512/apps
        RENAME linamp.png)
endif()
