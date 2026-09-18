@echo off
setlocal enabledelayedexpansion

set "SCRIPT_DIR=%~dp0"
if "%SCRIPT_DIR:~-1%"=="\" set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"
pushd "%SCRIPT_DIR%"

:: -----------------------------------------------------------------------------
:: Parse Arguments (Target selection or Toolchain path)
:: Syntax: build_host.bat [windows|posix|all] [path\to\gcc\bin]
:: -----------------------------------------------------------------------------
set "CHOSEN_TARGET="
set "HOST_TOOLCHAIN_BIN="

for %%A in ("%~1" "%~2") do (
    if not "%%~A"=="" (
        if /i "%%~A"=="windows" (
            set "CHOSEN_TARGET=windows"
        ) else if /i "%%~A"=="posix" (
            set "CHOSEN_TARGET=posix"
        ) else if /i "%%~A"=="all" (
            set "CHOSEN_TARGET=all"
        ) else if exist "%%~A\bin\gcc.exe" (
            set "HOST_TOOLCHAIN_BIN=%%~A\bin"
        ) else if exist "%%~A\gcc.exe" (
            set "HOST_TOOLCHAIN_BIN=%%~A"
        )
    )
)

:: -----------------------------------------------------------------------------
:: Locate GCC / MinGW Toolchain
:: -----------------------------------------------------------------------------
if not defined HOST_TOOLCHAIN_BIN (
    where gcc.exe >nul 2>nul
    if not errorlevel 1 (
        for /f "delims=" %%I in ('where gcc.exe') do (
            if not defined HOST_TOOLCHAIN_BIN set "HOST_TOOLCHAIN_BIN=%%~dpI"
        )
    )
)

if not defined HOST_TOOLCHAIN_BIN (
    if exist "C:\mingw64\bin\gcc.exe" (
        set "HOST_TOOLCHAIN_BIN=C:\mingw64\bin"
    ) else if exist "C:\msys64\mingw64\bin\gcc.exe" (
        set "HOST_TOOLCHAIN_BIN=C:\msys64\mingw64\bin"
    )
)

if not defined HOST_TOOLCHAIN_BIN (
    echo [ERROR] GCC toolchain not found!
    echo Please specify path, e.g.: build_host.bat windows C:\mingw64\bin
    popd
    exit /b 1
)

if "%HOST_TOOLCHAIN_BIN:~-1%"=="\" set "HOST_TOOLCHAIN_BIN=%HOST_TOOLCHAIN_BIN:~0,-1%"

set "CC=%HOST_TOOLCHAIN_BIN%\gcc.exe"
set "AR=%HOST_TOOLCHAIN_BIN%\ar.exe"
set "SIZE=%HOST_TOOLCHAIN_BIN%\size.exe"

echo ============================================================
echo [HOST] Using Toolchain: %HOST_TOOLCHAIN_BIN%
"%CC%" --version | findstr /C:"gcc"
echo ============================================================

:: -----------------------------------------------------------------------------
:: Setup Output and Build Directories
:: -----------------------------------------------------------------------------
set "BUILD_TMP=build\host_objs"
if not exist "%BUILD_TMP%" mkdir "%BUILD_TMP%"

:: Common Compiler Flags and Include Paths
set "COMMON_FLAGS=-O2 -ffunction-sections -fdata-sections -Wall -Wextra -pedantic -std=c99"
set "INCLUDES=-Iinc -Iport -Imodules\atomic -Imodules\ring_buffer -Imodules\memory_pool -Imodules\linked_list -Imodules\bitmap -Imodules\crc -Imodules\fsm"

:: Common Kernel Sources
set "CORE_SRCS=src\sertos_task.c src\sertos_scheduler.c src\sertos_sem.c src\sertos_mutex.c src\sertos_queue.c src\sertos_timer.c"
set "MODULE_SRCS=modules\bitmap\bitmap.c modules\crc\crc.c modules\fsm\fsm.c modules\linked_list\linked_list.c modules\memory_pool\memory_pool.c modules\ring_buffer\ring_buffer.c"

:: -----------------------------------------------------------------------------
:: Build Targets Configuration
:: -----------------------------------------------------------------------------
if defined CHOSEN_TARGET (
    if "%CHOSEN_TARGET%"=="all" (
        set "TARGETS=windows posix"
    ) else (
        set "TARGETS=%CHOSEN_TARGET%"
    )
) else (
    set "TARGETS=windows posix"
)

for %%T in (%TARGETS%) do (
    echo.
    echo ============================================================
    echo [BUILD] Compiling SertOS for %%T host architecture...
    echo ============================================================

    set "LIB_OUT_DIR=lib\%%T"
    if not exist "!LIB_OUT_DIR!" mkdir "!LIB_OUT_DIR!"

    set "OUT_OBJ_DIR=%BUILD_TMP%\%%T"
    if not exist "!OUT_OBJ_DIR!" mkdir "!OUT_OBJ_DIR!"

    if "%%T"=="windows" (
        set "PORT_SRCS=port\windows\port_windows.c"
        set "LIB_NAME=libsertos_windows.a"
        set "EXTRA_FLAGS=-D_WIN32"
    ) else if "%%T"=="posix" (
        set "PORT_SRCS=port\posix\port_posix.c"
        set "LIB_NAME=libsertos_posix.a"
        set "EXTRA_FLAGS="
    )

    set "ALL_SRCS=%CORE_SRCS% %MODULE_SRCS% !PORT_SRCS!"
    set "OBJS="

    for %%S in (!ALL_SRCS!) do (
        set "OBJ_FILE=!OUT_OBJ_DIR!\%%~nS.o"
        set "OBJS=!OBJS! "!OBJ_FILE!""

        "%CC%" %COMMON_FLAGS% !EXTRA_FLAGS! %INCLUDES% -c "%%S" -o "!OBJ_FILE!"
        if errorlevel 1 (
            echo [ERROR] Failed compiling %%S for %%T
            popd
            exit /b 1
        )
    )

    "%AR%" rcs "!LIB_OUT_DIR!\!LIB_NAME!" !OBJS!
    if errorlevel 1 (
        echo [ERROR] Failed creating archive !LIB_OUT_DIR!\!LIB_NAME!
        popd
        exit /b 1
    )

    echo [SUCCESS] Generated: !LIB_OUT_DIR!\!LIB_NAME!
    if exist "%SIZE%" (
        "%SIZE%" -t "!LIB_OUT_DIR!\!LIB_NAME!" | findstr /C:"TOTALS"
    )
)

echo.
echo ============================================================
echo [SUCCESS] Host libraries generated successfully!
echo           Targets: %TARGETS%
echo ============================================================
popd
endlocal
