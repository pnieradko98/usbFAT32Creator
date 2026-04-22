cmake_minimum_required(VERSION 3.21)
project(USBFAT32Creator VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(APPLE)
    # Help CMake find Qt installed via Homebrew on Apple Silicon/Intel Macs.
    list(PREPEND CMAKE_PREFIX_PATH "/opt/homebrew/opt/qt" "/usr/local/opt/qt")
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets)

if(WIN32)
    set(WIN_RESOURCES src/app.rc)
endif()

add_executable(usb_fat32_creator
    src/main.cpp
    src/MainWindow.h
    src/MainWindow.cpp
    src/Models.h
    src/DiskManager.h
    src/DiskManager.cpp
    src/CommandBuilder.h
    src/CommandBuilder.cpp
    src/VirtualPenPlanner.h
    src/VirtualPenPlanner.cpp
    ${WIN_RESOURCES}
)

target_link_libraries(usb_fat32_creator PRIVATE Qt6::Widgets)

if(MSVC)
    target_compile_options(usb_fat32_creator PRIVATE /W4)
else()
    target_compile_options(usb_fat32_creator PRIVATE -Wall -Wextra -Wpedantic)
endif()
