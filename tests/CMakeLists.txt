qt_add_executable(tst_watch tst_watch.cpp)
target_link_libraries(tst_watch PRIVATE watchcore Qt6::Test)
add_test(NAME tst_watch COMMAND tst_watch)
set_tests_properties(tst_watch PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND)
    add_test(NAME conformance
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/conformance.py)
endif()

qt_add_executable(tst_model tst_model.cpp)
target_link_libraries(tst_model PRIVATE watchcore Qt6::Test)
add_test(NAME tst_model COMMAND tst_model)
set_tests_properties(tst_model PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(tst_tree tst_tree.cpp)
target_link_libraries(tst_tree PRIVATE watchcore Qt6::Test)
add_test(NAME tst_tree COMMAND tst_tree)
set_tests_properties(tst_tree PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
