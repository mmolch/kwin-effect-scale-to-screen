cmake_minimum_required(VERSION 3.16)

set(PROJECT_NAME kwin-effect-scale-to-screen)
set(PROJECT_VERSION 1.0.1)

set(PROJECT_VERSION_SUFFIX "" CACHE STRING "Optional version suffix")

if(PROJECT_VERSION_SUFFIX)
    set(PROJECT_VERSION_FULL "${PROJECT_VERSION}-${PROJECT_VERSION_SUFFIX}")
else()
    set(PROJECT_VERSION_FULL "${PROJECT_VERSION}")
endif()

project(${PROJECT_NAME}
    VERSION ${PROJECT_VERSION}
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# extra cmake modules for KDE
find_package(ECM REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(FeatureSummary)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)
find_package(KF6 6.0 REQUIRED COMPONENTS Config GlobalAccel I18n CoreAddons)
find_package(KWin 6.0 REQUIRED NO_MODULE)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/metadata.json.in
    ${CMAKE_CURRENT_BINARY_DIR}/metadata.json
    @ONLY
)

add_library(ScaleToScreen MODULE
    main.cpp
    scale_to_screen.h
    scale_to_screen.cpp
    scale_to_screen.qrc
)

target_link_libraries(ScaleToScreen PRIVATE
    KWin::kwin

    KF6::GlobalAccel
    KF6::I18n
)

# Debian/Ubuntu multiarch Qt6 plugin path override
set(QT6_PLUGIN_DIR "${CMAKE_INSTALL_LIBDIR}/qt6/plugins")

# KWin effects live inside the Qt6 plugin tree
set(KWIN_EFFECTS_DIR "${QT6_PLUGIN_DIR}/kwin/effects")

install(TARGETS ScaleToScreen
    LIBRARY DESTINATION ${KWIN_EFFECTS_DIR}
)

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)

include(cmake/packaging.cmake)
