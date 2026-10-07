add_library(ds2sc SHARED
    dllmain.cpp
    core.cpp
    log.cpp
    settings.cpp
    version_gate.cpp
    hooks/iat.cpp
    hooks/getaddrinfo_hook.cpp
    hooks/savefile_hook.cpp
    hooks/server_redirect_hook.cpp
    player_count.cpp
    scaling.cpp
)

set_target_properties(ds2sc PROPERTIES
    OUTPUT_NAME "ds2sc"
    PREFIX ""
)

target_link_libraries(ds2sc PRIVATE bcrypt ws2_32 psapi)
