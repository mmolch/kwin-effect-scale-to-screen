configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/metadata.json.in
    ${CMAKE_CURRENT_BINARY_DIR}/metadata.json
    @ONLY
)

add_library(scaletoscreen MODULE
    main.cpp
    scale_to_screen.h scale_to_screen.cpp
    scaler.h scaler.cpp
    util.h util.cpp
    globals.h
)

#kconfig_target_kcfg_file(scaletoscreen
#  FILE scale_to_screen.kcfg
#  CLASS_NAME Config
#  MUTATORS
#  DEFAULT_VALUE_GETTERS
#)

target_link_libraries(scaletoscreen PRIVATE
    KWin::kwin

    KF6::ConfigGui
    KF6::GlobalAccel
    KF6::I18n
)

install(TARGETS scaletoscreen
    LIBRARY DESTINATION ${KWIN_EFFECTS_DIR}
)
