qt_add_executable(tst_watch tst_watch.cpp)
target_link_libraries(tst_watch PRIVATE watchcore Qt6::Test)
add_test(NAME tst_watch COMMAND tst_watch)
set_tests_properties(tst_watch PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
