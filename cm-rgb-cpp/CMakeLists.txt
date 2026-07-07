cmake_minimum_required(VERSION 3.0)
project(cm-rgb LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 98)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Target Windows XP
# _USING_V110_SDK71_ selects the XP-compatible CRT from the v141_xp toolset
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /DNOMINMAX /D_WIN32_WINNT=0x0501 /DWINVER=0x0501 /D_USING_V110_SDK71_")

# Source files
set(SOURCES
    src/main.cpp
    src/controller.cpp
    src/gui.cpp
    src/monitor.cpp
)

add_executable(cm-rgb ${SOURCES})

# Set XP-compatible subsystem (x86 only; for x64 use 5.02)
if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS,5.01")
endif()

# Link libraries
target_link_libraries(cm-rgb
    setupapi
    hid
    comctl32
    gdi32
    advapi32
    shell32
    wbemuuid
    ole32
    oleaut32
)
