cmake_minimum_required(VERSION 3.21)
project(watch-me VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Sources contain non-ASCII string literals (▶ ◀ → ·).
if(MSVC)
    add_compile_options(/utf-8)
endif()

# Release packaging: `cmake --install` also deploys the Qt runtime next to the app
# (windeployqt / macdeployqt / Linux deploy). On Linux it needs a non-system Qt.
option(WATCHME_DEPLOY "Deploy the Qt runtime on install (for release packages)" OFF)

# Qt 6.8 LTS or newer (DESIGN-NOTES D-1).
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick Scxml Test)
find_package(Qt6 6.8 QUIET COMPONENTS Multimedia)
# REQUIRES 6.8 opts into every Qt policy up to 6.8, e.g. QTP0001 (QML modules
# under qrc:/qt/qml/, hence loadFromModule in main.cpp; DESIGN-NOTES D-25).
# https://doc.qt.io/qt-6/qt-standard-project-setup.html
# https://doc.qt.io/qt-6/qt-cmake-policy-qtp0001.html
qt_standard_project_setup(REQUIRES 6.8)

# ---------------------------------------------------------------------------
# watchcore: generated state machine + value models (no QML)
# ---------------------------------------------------------------------------
add_library(watchcore STATIC
    cpp/src/WatchModel.h cpp/src/WatchModel.cpp
    cpp/src/TimeoutService.h cpp/src/TimeoutService.cpp
    cpp/src/ButtonRouter.h cpp/src/ButtonRouter.cpp
    cpp/src/ScxmlTreeModel.h cpp/src/ScxmlTreeModel.cpp
    cpp/src/TransitionTracer.h cpp/src/TransitionTracer.cpp
    cpp/src/SpecTags.h cpp/src/SpecTags.cpp
)
target_include_directories(watchcore PUBLIC cpp/src ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(watchcore PUBLIC Qt6::Core Qt6::Gui Qt6::Qml Qt6::Scxml)

# Compiles shared/statechart/watch.scxml into class WatchMachine (see the <scxml name=...>).
# https://doc.qt.io/qt-6/qtscxml-cmake-qt-add-statecharts.html
qt_add_statecharts(watchcore shared/statechart/watch.scxml)

# The SCXML (for the live view), layout hints and the spec (for confidence badges)
# are embedded so the view is generated from the same document that was compiled.
# Keep the resource paths short and host-independent (":/statechart/watch.scxml").
# https://doc.qt.io/qt-6/cmake-source-file-property-qt-resource-alias.html
set_source_files_properties(shared/statechart/watch.scxml PROPERTIES QT_RESOURCE_ALIAS statechart/watch.scxml)
set_source_files_properties(shared/statechart/layout.json PROPERTIES QT_RESOURCE_ALIAS statechart/layout.json)
qt_add_resources(watchcore "watchdata"
    PREFIX "/"
    FILES shared/statechart/watch.scxml shared/statechart/layout.json docs/STATECHART-SPEC.md
)

# ---------------------------------------------------------------------------
# watch-me application
# ---------------------------------------------------------------------------
qt_add_executable(watch-me cpp/src/main.cpp cpp/src/Beeper.h cpp/src/Beeper.cpp)
qt_add_qml_module(watch-me
    URI WatchMe
    VERSION 1.0
    QML_FILES
        shared/qml/Main.qml
        shared/qml/WatchFace.qml
        shared/qml/Lcd.qml
        shared/qml/LcdView.mjs
        shared/qml/SevenSegment.qml
        shared/qml/LcdDigits.qml
        shared/qml/WatchButton.qml
        shared/qml/StatechartView.qml
        shared/qml/StateBox.qml
        shared/qml/TransitionOverlay.qml
        shared/qml/Legend.qml
        shared/qml/EventLog.qml
        shared/qml/SimPanel.qml
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
    # (qt_standard_project_setup already gives Linux installs an $ORIGIN/../lib RPATH)
    qt_generate_deploy_qml_app_script(
        TARGET watch-me
        OUTPUT_SCRIPT deploy_script
    )
    install(SCRIPT ${deploy_script})

    # The ECMAScript datamodel of Qt SCXML is a run-time plugin
    # (plugins/scxmldatamodel) that Qt's deploy tooling does not pick up: without
    # it the state machine has no datamodel. Install it explicitly.
    if(TARGET Qt6::QScxmlEcmaScriptDataModelPlugin)
        if(APPLE)
            set(_dm_dir "watch-me.app/Contents/PlugIns/scxmldatamodel")
        elseif(WIN32)
            set(_dm_dir "${CMAKE_INSTALL_BINDIR}/scxmldatamodel")      # windeployqt layout
        else()
            set(_dm_dir "plugins/scxmldatamodel")                      # Qt's Linux deploy layout
        endif()
        install(FILES $<TARGET_FILE:Qt6::QScxmlEcmaScriptDataModelPlugin> DESTINATION "${_dm_dir}")
        if(APPLE)
            # Resolve @rpath/Qt*.framework inside the bundle, then re-sign (ad hoc):
            # arm64 macOS refuses to load a modified, unsigned dylib.
            install(CODE "set(_dm_rel \"${_dm_dir}/$<TARGET_FILE_NAME:Qt6::QScxmlEcmaScriptDataModelPlugin>\")")
            install(CODE [[
                set(_dm "${CMAKE_INSTALL_PREFIX}/${_dm_rel}")
                execute_process(COMMAND install_name_tool -add_rpath @loader_path/../../Frameworks "${_dm}")
                execute_process(COMMAND codesign --force --sign - "${_dm}" COMMAND_ERROR_IS_FATAL ANY)
            ]])
        endif()
    else()
        message(WARNING "Qt6::QScxmlEcmaScriptDataModelPlugin not found; the package will lack it")
    endif()
endif()

# ---------------------------------------------------------------------------
# tests
# ---------------------------------------------------------------------------
enable_testing()
add_subdirectory(cpp/tests)
