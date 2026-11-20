# Test executables are console programs: qt_add_executable would otherwise make them
# Windows GUI-subsystem executables (no captured output) and macOS bundles.
qt_add_executable(tst_watch tst_watch.cpp)
set_target_properties(tst_watch PROPERTIES WIN32_EXECUTABLE FALSE MACOSX_BUNDLE FALSE)
target_link_libraries(tst_watch PRIVATE watchcore Qt6::Test)
add_test(NAME tst_watch COMMAND tst_watch -o -,txt -o ${CMAKE_CURRENT_BINARY_DIR}/tst_watch.log,txt)
set_tests_properties(tst_watch PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND)
    add_test(NAME conformance
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/conformance.py)
    # Every spec row walked in the built app, through its real input path and event
    # log; also fails if the committed docs/VERIFICATION.md is not current.
    add_test(NAME verify
             COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/verify.py
                     --check --bin $<TARGET_FILE:watch-me>)
    set_tests_properties(verify PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 600)
else()
    message(WARNING "Python 3 not found: the conformance and verify tests are not registered")
endif()

qt_add_executable(tst_model tst_model.cpp)
set_target_properties(tst_model PROPERTIES WIN32_EXECUTABLE FALSE MACOSX_BUNDLE FALSE)
target_link_libraries(tst_model PRIVATE watchcore Qt6::Test)
add_test(NAME tst_model COMMAND tst_model -o -,txt -o ${CMAKE_CURRENT_BINARY_DIR}/tst_model.log,txt)
set_tests_properties(tst_model PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(tst_tree tst_tree.cpp)
set_target_properties(tst_tree PROPERTIES WIN32_EXECUTABLE FALSE MACOSX_BUNDLE FALSE)
target_link_libraries(tst_tree PRIVATE watchcore Qt6::Test)
add_test(NAME tst_tree COMMAND tst_tree -o -,txt -o ${CMAKE_CURRENT_BINARY_DIR}/tst_tree.log,txt)
set_tests_properties(tst_tree PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(tst_input tst_input.cpp)
set_target_properties(tst_input PROPERTIES WIN32_EXECUTABLE FALSE MACOSX_BUNDLE FALSE)
target_link_libraries(tst_input PRIVATE watchcore Qt6::Test)
add_test(NAME tst_input COMMAND tst_input -o -,txt -o ${CMAKE_CURRENT_BINARY_DIR}/tst_input.log,txt)
set_tests_properties(tst_input PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(tst_lcd tst_lcd.cpp)
set_target_properties(tst_lcd PROPERTIES WIN32_EXECUTABLE FALSE MACOSX_BUNDLE FALSE)
target_link_libraries(tst_lcd PRIVATE Qt6::Qml Qt6::Test)
target_compile_definitions(tst_lcd PRIVATE LCDVIEW_PATH="${PROJECT_SOURCE_DIR}/qml/LcdView.mjs")
add_test(NAME tst_lcd COMMAND tst_lcd -o -,txt -o ${CMAKE_CURRENT_BINARY_DIR}/tst_lcd.log,txt)
set_tests_properties(tst_lcd PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
