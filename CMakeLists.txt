cmake_minimum_required(VERSION 3.19)

project(VCD LANGUAGES CXX)

#enable_testing()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Debug)

#set(CMAKE_BUILD_TYPE RelWithDebInfo)   # -O2 -g
#add_compile_options(-fno-omit-frame-pointer)   # нужен perf’у

#Initialization of common build parameters
set(BUILD_TYPE_LIST "Debug" "Release")
set(OUTPUT_DIR "${CMAKE_CURRENT_LIST_DIR}/Output")

set(OUTPUT_DEBUG_DIR "{CMAKE_CURRENT_LIST_DIR}/Debug")
set(OUTPUT_RELEASE_DIR "{CMAKE_CURRENT_LIST_DIR}/Release")

set(CMAKE_MODULE_PATH "{CMAKE_CURRENT_LIST_DIR}/cmake")

add_definitions(-DQT_MESSAGELOGCONTEXT)
add_compile_definitions(QT_DEBUG)
add_compile_options(-g)



find_package(Qt5 COMPONENTS Core Gui Widgets Concurrent REQUIRED)
find_package(GTest REQUIRED)


set(SHARED_DIRS "${CMAKE_CURRENT_LIST_DIR}")

add_subdirectory(VcdViewer)
add_subdirectory(Libs)