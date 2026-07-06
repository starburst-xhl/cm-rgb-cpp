cmake_minimum_required(VERSION 3.0)
project(cm-rgb LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 98)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Target Windows XP
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /D_WIN32_WINNT=0x0501 /DWINVER=0x0501")

# Source files
set(SOURCES
    src/main.cpp
    src/controller.cpp
    src/gui.cpp
    src/monitor.cpp
)

add_executable(cm-rgb ${SOURCES})

# Link libraries
target_link_libraries(cm-rgb
    setupapi
    hid
    comctl32
    wbemuuid
    ole32
    oleaut32
)
