set(TEST_NAME VcdReaderTest)
set(LIBRARY_LIST VcdReader)

add_executable(${TEST_NAME} Test.cpp)
target_link_libraries(${TEST_NAME} ${GTEST_LIBRARIES} ${LIBRARY_LIST})
target_include_directories(${TEST_NAME} PRIVATE ${SHARED_DIRS})
gtest_discover_tests(${TEST_NAME})