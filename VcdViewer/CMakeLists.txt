set(TARGET_NAME VcdViewer)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(SOURCES_FILES
SnapScrollBar.hpp
   VcdAsyncReader.hpp
   VcdAsyncReader.cpp
   TreeModulesModel.hpp
   PinTableModel.hpp
   SignalsTreeModel.hpp
   SignalsTreeModel.cpp
   WaveformView.hpp
   WaveformView.cpp
   VcdViewerWidget.hpp
   VcdViewerWidget.cpp
   TreeModulesModel.cpp
   MainWindow.hpp
   MainWindow.cpp
   WaveformItem/ParamWave.cpp
   WaveformItem/SimpleWave.cpp
   WaveformItem/BusWaveItem.cpp
   WaveformItem/WaveItems.hpp
   WaveformItem/Dumpoff.cpp
   main.cpp)

source_group("Sources Files" FILES ${SOURCES_FILES})
source_group("Headers Files" FILES ${HEADERS_FILES})
source_group("Resources Files" FILES ${RESOURCES_FILES})

qt5_add_resources(RESOURCES ${CMAKE_CURRENT_LIST_DIR}/Resources/resources.qrc)


add_executable(${TARGET_NAME} ${SOURCES_FILES})
target_link_libraries(${TARGET_NAME}  VcdReader Qt5::Core  Qt5::Gui Qt5::Widgets Qt5::Concurrent)
target_sources(${TARGET_NAME} PRIVATE ${RESOURCES})

target_include_directories(${TARGET_NAME}
   PUBLIC ${SHARED_DIRS}
           ${CMAKE_CURRENT_LIST_DIR})