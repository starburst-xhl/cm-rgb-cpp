@echo off
REM Build script for cm-rgb (Windows XP compatible)
REM Requires: Visual Studio C++ compiler (cl.exe)
REM
REM Usage:
REM   build              - Build release
REM   build debug        - Build debug
REM   build clean        - Clean build artifacts

setlocal

set "BUILD_DIR=%~dp0build"
set "SRC_DIR=%~dp0src"

if /I "%1"=="clean" goto :clean
if /I "%1"=="debug" set "DEBUG=1"

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

set "CXXFLAGS=/nologo /EHsc /W3 /D_WIN32_WINNT=0x0501 /DWINVER=0x0501"
set "LDFLAGS=/nologo"
set "LIBS=setupapi.lib hid.lib comctl32.lib wbemuuid.lib ole32.lib oleaut32.lib"

if defined DEBUG (
    set "CXXFLAGS=%CXXFLAGS% /Od /Zi /D_DEBUG"
    set "LDFLAGS=%LDFLAGS% /DEBUG"
    set "OUTDIR=%BUILD_DIR%\debug"
) else (
    set "CXXFLAGS=%CXXFLAGS% /O2 /DNDEBUG"
    set "OUTDIR=%BUILD_DIR%\release"
)

if not exist "%OUTDIR%" mkdir "%OUTDIR%"

echo Compiling...
cl %CXXFLAGS% /Fo"%OUTDIR%\\" /Fe"%OUTDIR%\\cm-rgb.exe" ^
    "%SRC_DIR%\main.cpp" ^
    "%SRC_DIR%\controller.cpp" ^
    "%SRC_DIR%\gui.cpp" ^
    "%SRC_DIR%\monitor.cpp" ^
    /link %LDFLAGS% %LIBS%

if %ERRORLEVEL% equ 0 (
    echo.
    echo Build succeeded: %OUTDIR%\cm-rgb.exe
) else (
    echo.
    echo Build failed.
    exit /b 1
)

goto :eof

:clean
    if exist "%BUILD_DIR%" (
        rmdir /s /q "%BUILD_DIR%"
        echo Cleaned.
    ) else (
        echo Nothing to clean.
    )
    goto :eof
