@echo off
setlocal enabledelayedexpansion

set "SCRIPT_DIR=%~dp0"
if "%SCRIPT_DIR:~-1%"=="\" set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"
pushd "%SCRIPT_DIR%"

:: -----------------------------------------------------------------------------
:: Locate GNU Arm Embedded Toolchain
:: -----------------------------------------------------------------------------
set "ARM_TOOLCHAIN_BIN="

if not "%~1"=="" (
    if exist "%~1\bin\arm-none-eabi-gcc.exe" (
        set "ARM_TOOLCHAIN_BIN=%~1\bin"
    ) else if exist "%~1\arm-none-eabi-gcc.exe" (
        set "ARM_TOOLCHAIN_BIN=%~1"
    )
)

if not defined ARM_TOOLCHAIN_BIN (
    if exist "C:\arm\13.2.1\bin\arm-none-eabi-gcc.exe" (
        set "ARM_TOOLCHAIN_BIN=C:\arm\13.2.1\bin"
    )
)

if not defined ARM_TOOLCHAIN_BIN (
    where arm-none-eabi-gcc.exe >nul 2>nul
    if not errorlevel 1 (
        for /f "delims=" %%I in ('where arm-none-eabi-gcc.exe') do (
            if not defined ARM_TOOLCHAIN_BIN set "ARM_TOOLCHAIN_BIN=%%~dpI"
        )
    )
)

if not defined ARM_TOOLCHAIN_BIN (
    echo [ERROR] GNU Arm Embedded Toolchain not found!
    echo Please specify path, e.g.: build_arm.bat C:\arm\13.2.1
    popd
    exit /b 1
)

set "CC=%ARM_TOOLCHAIN_BIN%\arm-none-eabi-gcc.exe"
set "AR=%ARM_TOOLCHAIN_BIN%\arm-none-eabi-ar.exe"
set "SIZE=%ARM_TOOLCHAIN_BIN%\arm-none-eabi-size.exe"

echo ============================================================
echo [ARM] Using Toolchain: %ARM_TOOLCHAIN_BIN%
"%CC%" --version | findstr /C:"arm-none-eabi-gcc"
echo ============================================================

:: -----------------------------------------------------------------------------
:: Setup Output and Build Directories
:: -----------------------------------------------------------------------------
set "LIB_OUT_DIR=lib\arm"
if not exist "%LIB_OUT_DIR%" mkdir "%LIB_OUT_DIR%"

set "BUILD_TMP=build\arm_objs"
if not exist "%BUILD_TMP%" mkdir "%BUILD_TMP%"

:: Common Compiler Flags and Include Paths
set "COMMON_FLAGS=-Os -ffreestanding -ffunction-sections -fdata-sections -Wall -Wextra -pedantic -std=c99"
set "INCLUDES=-Iinc -Iport -Imodules\atomic -Imodules\ring_buffer -Imodules\memory_pool -Imodules\linked_list -Imodules\bitmap -Imodules\crc -Imodules\fsm"

:: Common Kernel Sources
set "CORE_SRCS=src\sertos_task.c src\sertos_scheduler.c src\sertos_sem.c src\sertos_mutex.c src\sertos_queue.c src\sertos_timer.c"
set "MODULE_SRCS=modules\bitmap\bitmap.c modules\crc\crc.c modules\fsm\fsm.c modules\linked_list\linked_list.c modules\memory_pool\memory_pool.c modules\ring_buffer\ring_buffer.c"

:: -----------------------------------------------------------------------------
:: Build Targets Configuration: Name | CPU | Extra Flags
:: -----------------------------------------------------------------------------
set "TARGETS=cortex-m0 cortex-m3 cortex-m4 cortex-m33"

for %%T in (%TARGETS%) do (
    echo.
    echo ============================================================
    echo [BUILD] Compiling SertOS for %%T...
    echo ============================================================

    set "PORT_DIR=port\arm\%%T"
    set "OUT_OBJ_DIR=%BUILD_TMP%\%%T"
    if not exist "!OUT_OBJ_DIR!" mkdir "!OUT_OBJ_DIR!"

    if "%%T"=="cortex-m0" (
        set "ARCH_FLAGS=-mcpu=cortex-m0 -mthumb"
        set "LIB_NAME=libsertos_cortex_m0.a"
    ) else if "%%T"=="cortex-m3" (
        set "ARCH_FLAGS=-mcpu=cortex-m3 -mthumb"
        set "LIB_NAME=libsertos_cortex_m3.a"
    ) else if "%%T"=="cortex-m4" (
        set "ARCH_FLAGS=-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard"
        set "LIB_NAME=libsertos_cortex_m4.a"
    ) else if "%%T"=="cortex-m33" (
        set "ARCH_FLAGS=-mcpu=cortex-m33 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard"
        set "LIB_NAME=libsertos_cortex_m33.a"
    )

    set "PORT_SRCS=!PORT_DIR!\port_cpu.c !PORT_DIR!\port_context.s"
    set "ALL_SRCS=%CORE_SRCS% %MODULE_SRCS% !PORT_SRCS!"
    set "OBJS="

    for %%S in (!ALL_SRCS!) do (
        set "OBJ_FILE=!OUT_OBJ_DIR!\%%~nS.o"
        set "OBJS=!OBJS! "!OBJ_FILE!""

        "%CC%" !ARCH_FLAGS! %COMMON_FLAGS% %INCLUDES% -c "%%S" -o "!OBJ_FILE!"
        if errorlevel 1 (
            echo [ERROR] Failed compiling %%S for %%T
            popd
            exit /b 1
        )
    )

    "%AR%" rcs "%LIB_OUT_DIR%\!LIB_NAME!" !OBJS!
    if errorlevel 1 (
        echo [ERROR] Failed creating archive %LIB_OUT_DIR%\!LIB_NAME!
        popd
        exit /b 1
    )

    echo [SUCCESS] Generated: %LIB_OUT_DIR%\!LIB_NAME!
    "%SIZE%" -t "%LIB_OUT_DIR%\!LIB_NAME!" | findstr /C:"TOTALS"
)

echo.
echo ============================================================
echo [SUCCESS] All 4 ARM Cortex libraries generated in:
echo           %LIB_OUT_DIR%
echo ============================================================
popd
endlocal
