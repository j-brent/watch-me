cmake_minimum_required(VERSION 3.21)
project(watch-me VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
qt_add_executable(watch-me src/main.cpp)
qt_add_qml_module(watch-me
    URI WatchMe
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/StatechartView.qml
        qml/StateBox.qml
)
target_link_libraries(watch-me PRIVATE watchcore Qt6::Quick)
if(TARGET Qt6::Multimedia)
    target_link_libraries(watch-me PRIVATE Qt6::Multimedia)
    target_compile_definitions(watch-me PRIVATE WATCHME_HAVE_MULTIMEDIA)
endif()

# ---------------------------------------------------------------------------
# tests
# ---------------------------------------------------------------------------
enable_testing()
add_subdirectory(tests)
