cmake_minimum_required(VERSION 3.16)

set(PROJECT_NAME ScaleToScreen)
set(PROJECT_VERSION 0.2.0)

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

add_library(${PROJECT_NAME} MODULE
    main.cpp
    scale_to_screen.h
    scale_to_screen.cpp
    scale_to_screen.qrc
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    KWin::kwin

    KF6::GlobalAccel
    KF6::I18n
)

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
