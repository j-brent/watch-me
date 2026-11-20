cmake_minimum_required(VERSION 3.21)
project(watch-me VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Sources contain non-ASCII string literals (▶ ◀ → ·).
if(MSVC)
    add_compile_options(/utf-8)
endif()

# Release packaging: `cmake --install` also deploys the Qt runtime next to the app
# (windeployqt / macdeployqt / Linux deploy). Needs a non-system Qt >= 6.5 on Linux.
option(WATCHME_DEPLOY "Deploy the Qt runtime on install (for release packages)" OFF)

# Qt >= 6.4 (6.4.2 is what the build environment provides, see DESIGN-NOTES D-1).
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Qml Quick Scxml Test)
find_package(Qt6 6.4 QUIET COMPONENTS Multimedia)
qt_standard_project_setup()

# ---------------------------------------------------------------------------
# watchcore: generated state machine + value models (no QML)
# ---------------------------------------------------------------------------
add_library(watchcore STATIC
    src/WatchModel.h src/WatchModel.cpp
    src/ButtonRouter.h src/ButtonRouter.cpp
    src/ScxmlTreeModel.h src/ScxmlTreeModel.cpp
    src/TransitionTracer.h src/TransitionTracer.cpp
    src/SpecTags.h src/SpecTags.cpp
)
target_include_directories(watchcore PUBLIC src ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(watchcore PUBLIC Qt6::Core Qt6::Gui Qt6::Qml Qt6::Scxml)

# Compiles statechart/watch.scxml into class WatchMachine (see the <scxml name=...>).
# https://doc.qt.io/qt-6/qtscxml-cmake-qt-add-statecharts.html
qt_add_statecharts(watchcore statechart/watch.scxml)

# The SCXML (for the live view), layout hints and the spec (for confidence badges)
# are embedded so the view is generated from the same document that was compiled.
qt_add_resources(watchcore "watchdata"
    PREFIX "/"
    FILES statechart/watch.scxml statechart/layout.json docs/STATECHART-SPEC.md
)

# ---------------------------------------------------------------------------
# watch-me application
# ---------------------------------------------------------------------------
qt_add_executable(watch-me src/main.cpp src/Beeper.h src/Beeper.cpp)
qt_add_qml_module(watch-me
    URI WatchMe
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/WatchFace.qml
        qml/Lcd.qml
        qml/SevenSegment.qml
        qml/LcdDigits.qml
        qml/WatchButton.qml
        qml/StatechartView.qml
        qml/StateBox.qml
        qml/TransitionOverlay.qml
        qml/Legend.qml
        qml/EventLog.qml
        qml/SimPanel.qml
)
target_link_libraries(watch-me PRIVATE watchcore Qt6::Quick)
set_target_properties(watch-me PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "watch-me"
    MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.j-brent.watch-me"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
)
if(TARGET Qt6::Multimedia)
    target_link_libraries(watch-me PRIVATE Qt6::Multimedia)
    target_compile_definitions(watch-me PRIVATE WATCHME_HAVE_MULTIMEDIA)
endif()

# ---------------------------------------------------------------------------
# install / deploy
# https://doc.qt.io/qt-6/qt-generate-deploy-qml-app-script.html
# ---------------------------------------------------------------------------
include(GNUInstallDirs)
install(TARGETS watch-me
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
if(WATCHME_DEPLOY)
    if(QT_VERSION VERSION_LESS 6.5)
        message(FATAL_ERROR "WATCHME_DEPLOY needs Qt >= 6.5 (found ${QT_VERSION})")
    endif()
    # (qt_standard_project_setup already gives Linux installs an $ORIGIN/../lib RPATH)
    qt_generate_deploy_qml_app_script(
        TARGET watch-me
        OUTPUT_SCRIPT deploy_script
    )
    install(SCRIPT ${deploy_script})
endif()

# ---------------------------------------------------------------------------
# tests
# ---------------------------------------------------------------------------
enable_testing()
add_subdirectory(tests)
