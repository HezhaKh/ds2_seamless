add_library(ds2sc SHARED
    dllmain.cpp
    core.cpp
    log.cpp
    settings.cpp
    version_gate.cpp
    hooks/iat.cpp
    hooks/getaddrinfo_hook.cpp
)

set_target_properties(ds2sc PROPERTIES
    OUTPUT_NAME "ds2sc"
    PREFIX ""
)

target_link_libraries(ds2sc PRIVATE bcrypt ws2_32)

add_custom_command(TARGET ds2sc POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${DS2SC_GAME_DIR}/SeamlessCoop"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:ds2sc>"
            "${DS2SC_GAME_DIR}/SeamlessCoop/ds2sc.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/dist/SeamlessCoop/ds2sc_settings.ini"
            "${DS2SC_GAME_DIR}/SeamlessCoop/ds2sc_settings.ini"
    COMMENT "Staging ds2sc.dll + settings.ini into Game/SeamlessCoop/"
)
