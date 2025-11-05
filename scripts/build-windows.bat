@echo off
setlocal enabledelayedexpansion
for %%i in ("%~dp0.") do set "ROOT_DIR=%%~dpi"

:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
:: Configuration
:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

set SPRITE_BACKER_EXECUTABLE=sprite_backer.exe

:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
:: Check if Visual Studio environment is set up
:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

where cl >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo Error: Visual Studio compiler not found!
    echo Please run this script from a Visual Studio Developer Command Prompt
    echo or run vcvarsall.bat first.
    exit /b 1
)

:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
:: Compiler settings
:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

set CC=cl
set CFLAGS_COMMON=/nologo /W3 /Oi /I%ROOT_DIR%\vendor /I%ROOT_DIR%\src /fp:fast /std:c17 /DPLATFORM_WIN32 /D_CRT_SECURE_NO_WARNINGS
set CFLAGS_DEBUG=/Od /Zi /D_DEBUG /DDEBUG /MTd /DSOKOL_DEBUG /DBUILD_INTERNAL /DBUILD_HOTRELOAD
set CFLAGS_INTERNAL=/O2 /MT /DNDEBUG /DBUILD_INTERNAL
set CFLAGS_RELEASE=/O2 /MT /DNDEBUG

:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
:: Create directories
:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

if not exist build mkdir build
pushd .\build

:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
:: Parse comand line arguments
:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

set BUILD_TYPE=debug
if "%1"=="release" set BUILD_TYPE=release
if "%1"=="internal" set BUILD_TYPE=internal

:: Set build-specific flags
if "%BUILD_TYPE%"=="debug" (
    set CFLAGS=%CFLAGS_COMMON% %CFLAGS_DEBUG%
) else if "%BUILD_TYPE%"=="release" (
    set CFLAGS=%CFLAGS_COMMON% %CFLAGS_RELEASE%
) else if "%BUILD_TYPE%"=="internal" (
    set CFLAGS=%CFLAGS_COMMON% %CFLAGS_INTERNAL%
) else (
    echo Error: Unknown build type!
    exit /b 1
)

:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
:: Compile sprite backer
:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

echo Building sprite backer (%BUILD_TYPE%)
%CC% %CFLAGS% ^
    %ROOT_DIR%\src\main.c ^
    /Fe:%SPRITE_BACKER_EXECUTABLE% ^
    /link ^
    /incremental:no ^
    /opt:ref

if %ERRORLEVEL% neq 0 (
    echo Error: Compilation failed!
    exit /b 1
)

popd
endlocal
