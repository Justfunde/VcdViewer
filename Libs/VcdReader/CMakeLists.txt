set(TARGET_NAME VcdReader)
add_library(${TARGET_NAME} STATIC VcdReader.cpp)
target_include_directories(${TARGET_NAME} PUBLIC ${SHARED_DIRS})

add_subdirectory(Test)