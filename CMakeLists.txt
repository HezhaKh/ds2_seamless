cmake_minimum_required(VERSION 3.20)
project(ds2sc LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "ds2sc requires Windows and a Windows C++ toolchain.")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8 OR
   CMAKE_SYSTEM_PROCESSOR MATCHES "[Aa][Rr][Mm]" OR
   CMAKE_GENERATOR_PLATFORM MATCHES "[Aa][Rr][Mm]")
    message(FATAL_ERROR "ds2sc requires Windows x64 (not Win32 or ARM64). Configure Visual Studio with -A x64.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /permissive- /Zc:__cplusplus /utf-8 /MP)
endif()
add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)

option(DS2SC_STAGE_TO_GAME "Also copy the package into an existing game directory" OFF)
set(DS2SC_GAME_DIR "" CACHE PATH "Game directory containing DarkSoulsII.exe; required only when DS2SC_STAGE_TO_GAME=ON")
if(DS2SC_STAGE_TO_GAME AND NOT EXISTS "${DS2SC_GAME_DIR}/DarkSoulsII.exe")
    message(FATAL_ERROR "Set DS2SC_GAME_DIR to an existing directory containing DarkSoulsII.exe to enable game staging.")
endif()

add_subdirectory(dll)
add_subdirectory(launcher)

# Runs on every build, including when only settings change. Configurations have
# separate packages so Debug and Release cannot overwrite each other.
set(DS2SC_PACKAGE_DIR "${CMAKE_BINARY_DIR}/package/$<CONFIG>")
add_custom_target(ds2sc_package ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "${DS2SC_PACKAGE_DIR}/SeamlessCoop"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:ds2sc_launcher>" "${DS2SC_PACKAGE_DIR}/ds2sc_launcher.exe"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:ds2sc>" "${DS2SC_PACKAGE_DIR}/SeamlessCoop/ds2sc.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/dist/SeamlessCoop/ds2sc_settings.ini"
            "${DS2SC_PACKAGE_DIR}/SeamlessCoop/ds2sc_settings.ini"
    DEPENDS ds2sc ds2sc_launcher
    COMMENT "Staging compiled launcher, DLL and settings into ${DS2SC_PACKAGE_DIR}"
    VERBATIM
)
if(DS2SC_STAGE_TO_GAME)
    add_custom_target(ds2sc_stage_game ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${DS2SC_PACKAGE_DIR}" "${DS2SC_GAME_DIR}"
        DEPENDS ds2sc_package
        COMMENT "Copying package into ${DS2SC_GAME_DIR}"
        VERBATIM
    )
endif()
