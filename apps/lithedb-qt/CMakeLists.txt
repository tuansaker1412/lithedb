cmake_minimum_required(VERSION 3.21)

project(LitheDBQt VERSION 0.1.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

qt_standard_project_setup()

qt_add_executable(lithedb-qt
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/resources.qrc
)

target_link_libraries(lithedb-qt PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)
