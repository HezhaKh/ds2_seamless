cmake_minimum_required(VERSION 3.20)
project(ds2sc LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "ds2sc must be built as x64 (DarkSoulsII.exe is x64).")
endif()

if(MSVC)
    add_compile_options(/W4 /permissive- /Zc:__cplusplus /utf-8 /MP)
    add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
endif()

set(DS2SC_GAME_DIR "C:/Program Files (x86)/Steam/steamapps/common/Dark Souls II Scholar of the First Sin/Game"
    CACHE PATH "DS2 SOTFS Game/ directory — the folder containing DarkSoulsII.exe. Build artifacts stage HERE (not the parent install root), because DS2 keeps its exe inside Game/ unlike DS3/ER/DS1R.")

add_subdirectory(dll)
add_subdirectory(launcher)
