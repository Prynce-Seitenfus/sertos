@echo off
setlocal enabledelayedexpansion

set "SCRIPT_DIR=%~dp0"
if "%SCRIPT_DIR:~-1%"=="\" set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"
pushd "%SCRIPT_DIR%"

:: -----------------------------------------------------------------------------
:: Parse Arguments
:: Syntax: build.bat [host|arm|all|windows|posix|m0|m3|m4|m33] [toolchain_path]
:: -----------------------------------------------------------------------------
set "CHOSEN_TARGET="
set "CUSTOM_TOOLCHAIN="

for %%A in ("%~1" "%~2") do (
    if not "%%~A"=="" (
        if /i "%%~A"=="-h" goto :show_help
        if /i "%%~A"=="--help" goto :show_help
        if /i "%%~A"=="/?" goto :show_help

        if /i "%%~A"=="host" (
            set "CHOSEN_TARGET=host"
        ) else if /i "%%~A"=="windows" (
            set "CHOSEN_TARGET=windows"
        ) else if /i "%%~A"=="posix" (
            set "CHOSEN_TARGET=posix"
        ) else if /i "%%~A"=="arm" (
            set "CHOSEN_TARGET=arm"
        ) else if /i "%%~A"=="all" (
            set "CHOSEN_TARGET=all"
        ) else if /i "%%~A"=="cortex-m0" (
            set "CHOSEN_TARGET=cortex-m0"
        ) else if /i "%%~A"=="m0" (
            set "CHOSEN_TARGET=cortex-m0"
        ) else if /i "%%~A"=="cortex-m3" (
            set "CHOSEN_TARGET=cortex-m3"
        ) else if /i "%%~A"=="m3" (
            set "CHOSEN_TARGET=cortex-m3"
        ) else if /i "%%~A"=="cortex-m4" (
            set "CHOSEN_TARGET=cortex-m4"
        ) else if /i "%%~A"=="m4" (
            set "CHOSEN_TARGET=cortex-m4"
        ) else if /i "%%~A"=="cortex-m33" (
            set "CHOSEN_TARGET=cortex-m33"
        ) else if /i "%%~A"=="m33" (
            set "CHOSEN_TARGET=cortex-m33"
        ) else if exist "%%~A\bin\gcc.exe" (
            set "CUSTOM_TOOLCHAIN=%%~A\bin"
        ) else if exist "%%~A\gcc.exe" (
            set "CUSTOM_TOOLCHAIN=%%~A"
        ) else if exist "%%~A\bin\arm-none-eabi-gcc.exe" (
            set "CUSTOM_TOOLCHAIN=%%~A\bin"
        ) else if exist "%%~A\arm-none-eabi-gcc.exe" (
            set "CUSTOM_TOOLCHAIN=%%~A"
        )
    )
)

if not defined CHOSEN_TARGET (
    set "CHOSEN_TARGET=all"
)

:: -----------------------------------------------------------------------------
:: Common Paths, Directories, and Source Sets
:: -----------------------------------------------------------------------------
set "CORE_SRCS=src\sertos_task.c src\sertos_scheduler.c src\sertos_sem.c src\sertos_mutex.c src\sertos_queue.c src\sertos_timer.c"
set "MODULE_SRCS=modules\bitmap\bitmap.c modules\crc\crc.c modules\fsm\fsm.c modules\linked_list\linked_list.c modules\memory_pool\memory_pool.c modules\ring_buffer\ring_buffer.c"
set "INCLUDES=-Iinc -Iport -Imodules\atomic -Imodules\ring_buffer -Imodules\memory_pool -Imodules\linked_list -Imodules\bitmap -Imodules\crc -Imodules\fsm"

set "BUILD_FAIL=0"

:: -----------------------------------------------------------------------------
:: Execute Target Builds
:: -----------------------------------------------------------------------------
if "%CHOSEN_TARGET%"=="host" (
    call :build_host_target windows
    call :build_host_target posix
) else if "%CHOSEN_TARGET%"=="windows" (
    call :build_host_target windows
) else if "%CHOSEN_TARGET%"=="posix" (
    call :build_host_target posix
) else if "%CHOSEN_TARGET%"=="arm" (
    call :build_arm_all
) else if "%CHOSEN_TARGET%"=="cortex-m0" (
    call :build_arm_single cortex-m0
) else if "%CHOSEN_TARGET%"=="cortex-m3" (
    call :build_arm_single cortex-m3
) else if "%CHOSEN_TARGET%"=="cortex-m4" (
    call :build_arm_single cortex-m4
) else if "%CHOSEN_TARGET%"=="cortex-m33" (
    call :build_arm_single cortex-m33
) else if "%CHOSEN_TARGET%"=="all" (
    call :build_host_target windows
    call :build_host_target posix
    call :build_arm_all
)

echo.
echo ============================================================
if "!BUILD_FAIL!"=="0" (
    echo [SUCCESS] Build completed successfully
) else (
    echo [ERROR] Build completed with errors
)
echo ============================================================

if "!BUILD_FAIL!"=="0" (
    popd
    endlocal
    exit /b 0
) else (
    popd
    endlocal
    exit /b 1
)

:: -----------------------------------------------------------------------------
:: Subroutine: Build Host Target (Windows or POSIX)
:: -----------------------------------------------------------------------------
:build_host_target
set "HOST_TARGET=%~1"
set "HOST_TOOLCHAIN="

if defined CUSTOM_TOOLCHAIN (
    if exist "%CUSTOM_TOOLCHAIN%\gcc.exe" set "HOST_TOOLCHAIN=%CUSTOM_TOOLCHAIN%"
)

if not defined HOST_TOOLCHAIN (
    if exist "C:\mingw64\gcc-13.2.0\mingw64\bin\gcc.exe" (
        set "HOST_TOOLCHAIN=C:\mingw64\gcc-13.2.0\mingw64\bin"
    ) else if exist "C:\mingw64\bin\gcc.exe" (
        set "HOST_TOOLCHAIN=C:\mingw64\bin"
    ) else if exist "C:\msys64\mingw64\bin\gcc.exe" (
        set "HOST_TOOLCHAIN=C:\msys64\mingw64\bin"
    )
)

if not defined HOST_TOOLCHAIN (
    where gcc.exe >nul 2>nul
    if not errorlevel 1 (
        for /f "delims=" %%I in ('where gcc.exe') do (
            if not defined HOST_TOOLCHAIN set "HOST_TOOLCHAIN=%%~dpI"
        )
    )
)

if not defined HOST_TOOLCHAIN (
    echo [ERROR] MinGW / Host GCC toolchain not found!
    set "BUILD_FAIL=1"
    goto :eof
)

if "%HOST_TOOLCHAIN:~-1%"=="\" set "HOST_TOOLCHAIN=%HOST_TOOLCHAIN:~0,-1%"

set "HOST_CC=%HOST_TOOLCHAIN%\gcc.exe"
set "HOST_AR=%HOST_TOOLCHAIN%\ar.exe"
set "HOST_SIZE=%HOST_TOOLCHAIN%\size.exe"

set "LIB_OUT=lib\%HOST_TARGET%"
if not exist "%LIB_OUT%" mkdir "%LIB_OUT%"
set "OBJ_DIR=build\host_objs\%HOST_TARGET%"
if not exist "%OBJ_DIR%" mkdir "%OBJ_DIR%"

set "HOST_LIB=%LIB_OUT%\libsertos_%HOST_TARGET%.a"

echo.
echo ============================================================
echo [BUILD] Compiling SertOS for %HOST_TARGET% host architecture...
echo [TOOLCHAIN] %HOST_TOOLCHAIN%
echo ============================================================

set "PORT_SRC=port\%HOST_TARGET%\port_%HOST_TARGET%.c"
set "SRCS_TO_BUILD=%CORE_SRCS% %MODULE_SRCS% %PORT_SRC%"
set "OBJS="

for %%S in (%SRCS_TO_BUILD%) do (
    set "OBJ_FILE=%OBJ_DIR%\%%~nS.o"
    set "OBJS=!OBJS! "!OBJ_FILE!""

    "%HOST_CC%" -O2 -Wall -Wextra -pedantic -std=c99 %INCLUDES% -c "%%S" -o "!OBJ_FILE!"
    if !ERRORLEVEL! neq 0 (
        echo [ERROR] Failed compiling %%S
        set "BUILD_FAIL=1"
        goto :eof
    )
)

"%HOST_AR%" rcs "%HOST_LIB%" %OBJS%
if !ERRORLEVEL! neq 0 (
    echo [ERROR] Failed creating archive %HOST_LIB%
    set "BUILD_FAIL=1"
    goto :eof
)

echo [SUCCESS] Generated: %HOST_LIB%
if exist "%HOST_SIZE%" (
    "%HOST_SIZE%" -t "%HOST_LIB%" | findstr /C:"TOTALS"
)
goto :eof

:: -----------------------------------------------------------------------------
:: Subroutine: Build All ARM Targets
:: -----------------------------------------------------------------------------
:build_arm_all
for %%T in (cortex-m0 cortex-m3 cortex-m4 cortex-m33) do (
    call :build_arm_single %%T
)
goto :eof

:: -----------------------------------------------------------------------------
:: Subroutine: Build Single ARM Target
:: -----------------------------------------------------------------------------
:build_arm_single
set "ARM_TARGET=%~1"
set "ARM_TOOLCHAIN="

if defined CUSTOM_TOOLCHAIN (
    if exist "%CUSTOM_TOOLCHAIN%\arm-none-eabi-gcc.exe" set "ARM_TOOLCHAIN=%CUSTOM_TOOLCHAIN%"
)

if not defined ARM_TOOLCHAIN (
    if exist "C:\arm\13.2.1\bin\arm-none-eabi-gcc.exe" (
        set "ARM_TOOLCHAIN=C:\arm\13.2.1\bin"
    )
)

if not defined ARM_TOOLCHAIN (
    where arm-none-eabi-gcc.exe >nul 2>nul
    if not errorlevel 1 (
        for /f "delims=" %%I in ('where arm-none-eabi-gcc.exe') do (
            if not defined ARM_TOOLCHAIN set "ARM_TOOLCHAIN=%%~dpI"
        )
    )
)

if not defined ARM_TOOLCHAIN (
    echo [ERROR] GNU Arm Embedded Toolchain not found!
    set "BUILD_FAIL=1"
    goto :eof
)

if "%ARM_TOOLCHAIN:~-1%"=="\" set "ARM_TOOLCHAIN=%ARM_TOOLCHAIN:~0,-1%"

set "ARM_CC=%ARM_TOOLCHAIN%\arm-none-eabi-gcc.exe"
set "ARM_AR=%ARM_TOOLCHAIN%\arm-none-eabi-ar.exe"
set "ARM_SIZE=%ARM_TOOLCHAIN%\arm-none-eabi-size.exe"

set "LIB_OUT=lib\arm"
if not exist "%LIB_OUT%" mkdir "%LIB_OUT%"
set "OBJ_DIR=build\arm_objs\%ARM_TARGET%"
if not exist "%OBJ_DIR%" mkdir "%OBJ_DIR%"

if "%ARM_TARGET%"=="cortex-m0" (
    set "ARCH_FLAGS=-mcpu=cortex-m0 -mthumb"
    set "LIB_NAME=libsertos_cortex_m0.a"
) else if "%ARM_TARGET%"=="cortex-m3" (
    set "ARCH_FLAGS=-mcpu=cortex-m3 -mthumb"
    set "LIB_NAME=libsertos_cortex_m3.a"
) else if "%ARM_TARGET%"=="cortex-m4" (
    set "ARCH_FLAGS=-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard"
    set "LIB_NAME=libsertos_cortex_m4.a"
) else if "%ARM_TARGET%"=="cortex-m33" (
    set "ARCH_FLAGS=-mcpu=cortex-m33 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard"
    set "LIB_NAME=libsertos_cortex_m33.a"
)

set "ARM_LIB=%LIB_OUT%\%LIB_NAME%"

echo.
echo ============================================================
echo [BUILD] Compiling SertOS for %ARM_TARGET%...
echo [TOOLCHAIN] %ARM_TOOLCHAIN%
echo ============================================================

set "PORT_DIR=port\arm\%ARM_TARGET%"
set "PORT_SRCS=%PORT_DIR%\port_cpu.c %PORT_DIR%\port_context.s"
set "ALL_ARM_SRCS=%CORE_SRCS% %MODULE_SRCS% %PORT_SRCS%"
set "OBJS="

for %%S in (%ALL_ARM_SRCS%) do (
    set "OBJ_FILE=%OBJ_DIR%\%%~nS.o"
    set "OBJS=!OBJS! "!OBJ_FILE!""

    "%ARM_CC%" !ARCH_FLAGS! -Os -ffreestanding -ffunction-sections -fdata-sections -Wall -Wextra -pedantic -std=c99 %INCLUDES% -c "%%S" -o "!OBJ_FILE!"
    if !ERRORLEVEL! neq 0 (
        echo [ERROR] Failed compiling %%S for %ARM_TARGET%
        set "BUILD_FAIL=1"
        goto :eof
    )
)

"%ARM_AR%" rcs "%ARM_LIB%" %OBJS%
if !ERRORLEVEL! neq 0 (
    echo [ERROR] Failed creating archive %ARM_LIB%
    set "BUILD_FAIL=1"
    goto :eof
)

echo [SUCCESS] Generated: %ARM_LIB%
if exist "%ARM_SIZE%" (
    "%ARM_SIZE%" -t "%ARM_LIB%" | findstr /C:"TOTALS"
)
goto :eof

:: -----------------------------------------------------------------------------
:: Help Menu
:: -----------------------------------------------------------------------------
:show_help
echo.
echo Usage: build.bat [TARGET] [TOOLCHAIN_PATH]
echo.
echo Targets:
echo   all         Build host (windows) and all ARM Cortex libraries (default)
echo   host        Build Windows host static library (lib\windows\libsertos_windows.a)
echo   windows     Build Windows host static library (lib\windows\libsertos_windows.a)
echo   posix       Build POSIX host static library   (lib\posix\libsertos_posix.a)
echo   arm         Build all 4 ARM Cortex libraries  (lib\arm\libsertos_cortex_*.a)
echo   m0          Build Cortex-M0 library           (lib\arm\libsertos_cortex_m0.a)
echo   m3          Build Cortex-M3 library           (lib\arm\libsertos_cortex_m3.a)
echo   m4          Build Cortex-M4 library           (lib\arm\libsertos_cortex_m4.a)
echo   m33         Build Cortex-M33 library          (lib\arm\libsertos_cortex_m33.a)
echo.
echo Examples:
echo   build.bat
echo   build.bat host
echo   build.bat arm
echo   build.bat m33
echo   build.bat host C:\mingw64\gcc-13.2.0\mingw64\bin
echo   build.bat arm  C:\arm\13.2.1\bin
echo.
popd
endlocal
