cmake_minimum_required(VERSION 3.21)

project(Linamp VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC OFF)

if(APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version")
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS
    Core
    Gui
    Quick
    QuickControls2
    QuickDialogs2
    Multimedia
)

# ---- FFmpeg ----------------------------------------------------------------
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(FFMPEG QUIET IMPORTED_TARGET
        libavformat libavcodec libavutil libswresample)
endif()

if(NOT FFMPEG_FOUND)
    set(FFMPEG_ROOT "" CACHE PATH "Root of an FFmpeg installation (include/, lib/)")
    set(_ffmpeg_hints
        "${FFMPEG_ROOT}"
        "$ENV{FFMPEG_ROOT}"
        /opt/homebrew
        /usr/local
    )
    foreach(_lib avformat avcodec avutil swresample)
        find_path(${_lib}_INCLUDE_DIR
            NAMES lib${_lib}/${_lib}.h
            HINTS ${_ffmpeg_hints}
            PATH_SUFFIXES include)
        find_library(${_lib}_LIBRARY
            NAMES ${_lib}
            HINTS ${_ffmpeg_hints}
            PATH_SUFFIXES lib lib64)
        if(NOT ${_lib}_INCLUDE_DIR OR NOT ${_lib}_LIBRARY)
            message(FATAL_ERROR
                "FFmpeg development files not found (${_lib}). "
                "Install FFmpeg dev packages, or pass -DFFMPEG_ROOT=<prefix>.")
        endif()
        list(APPEND FFMPEG_INCLUDE_DIRS ${${_lib}_INCLUDE_DIR})
        list(APPEND FFMPEG_LIBRARIES ${${_lib}_LIBRARY})
    endforeach()
    set(FFMPEG_FOUND TRUE)
endif()

# ---- Application -----------------------------------------------------------
set(LINAMP_SOURCES
    src/main.cpp
    src/audioengine.cpp
    src/audioengine.h
    src/shell.cpp
    src/shell.h
    src/prober.cpp
    src/prober.h
    resources.qrc
)

if(APPLE)
    add_executable(linamp MACOSX_BUNDLE ${LINAMP_SOURCES})
else()
    add_executable(linamp ${LINAMP_SOURCES})
endif()

target_include_directories(linamp PRIVATE src)
target_compile_definitions(linamp PRIVATE LINAMP_EMBEDDED_QML)
target_link_libraries(linamp PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::QuickDialogs2
    Qt6::Multimedia
)

if(TARGET PkgConfig::FFMPEG)
    target_link_libraries(linamp PRIVATE PkgConfig::FFMPEG)
else()
    target_include_directories(linamp PRIVATE ${FFMPEG_INCLUDE_DIRS})
    target_link_libraries(linamp PRIVATE ${FFMPEG_LIBRARIES})
endif()

if(MSVC)
    target_compile_options(linamp PRIVATE /W3)
else()
    target_compile_options(linamp PRIVATE -Wall -Wextra)
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
        MACOSX_BUNDLE_BUNDLE_NAME "Linamp"
        MACOSX_BUNDLE_GUI_IDENTIFIER "local.linamp.app"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    )
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/linamp.icns")
        set(LINAMP_ICNS "${CMAKE_CURRENT_SOURCE_DIR}/assets/linamp.icns")
        set_source_files_properties("${LINAMP_ICNS}" PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources")
        target_sources(linamp PRIVATE "${LINAMP_ICNS}")
        set_target_properties(linamp PROPERTIES
            MACOSX_BUNDLE_ICON_FILE "linamp.icns")
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
