cmake_minimum_required(VERSION 3.20)
project(WhereIsByLabelQt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

qt_standard_project_setup()

qt_add_executable(WhereIsByLabelQt
    src/main.cpp
    src/MainWindow.h
    src/MainWindow.cpp
    src/IndexerWorker.h
    src/IndexerWorker.cpp
    src/IndexFormat.h
)

target_link_libraries(WhereIsByLabelQt PRIVATE Qt6::Widgets)

if (WIN32)
  target_compile_definitions(WhereIsByLabelQt PRIVATE UNICODE _UNICODE NOMINMAX)
endif()
