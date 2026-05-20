add_executable(ds2sc_launcher WIN32
    main.cpp
)

set_target_properties(ds2sc_launcher PROPERTIES
    OUTPUT_NAME "ds2sc_launcher"
)

add_custom_command(TARGET ds2sc_launcher POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:ds2sc_launcher>"
            "${DS2SC_GAME_DIR}/ds2sc_launcher.exe"
    COMMENT "Staging ds2sc_launcher.exe next to DarkSoulsII.exe in Game/"
)
