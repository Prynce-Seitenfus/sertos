cmake_minimum_required(VERSION 3.10)
project(SertOS C ASM)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Supported port architectures:
# "windows", "posix", "cortex-m33", "cortex-m4", "cortex-m3", "cortex-m0", "rv32i"
if(NOT DEFINED SERTOS_PORT)
    if(WIN32)
        set(SERTOS_PORT "windows")
    elseif(UNIX)
        set(SERTOS_PORT "posix")
    else()
        set(SERTOS_PORT "cortex-m4")
    endif()
endif()

message(STATUS "Configuring SertOS for port architecture: ${SERTOS_PORT}")

# Kernel source files
set(SERTOS_KERNEL_SOURCES
    src/sertos_task.c
    src/sertos_scheduler.c
    src/sertos_sem.c
    src/sertos_mutex.c
    src/sertos_queue.c
    src/sertos_timer.c
)

# Port source selection
if(SERTOS_PORT STREQUAL "windows")
    list(APPEND SERTOS_KERNEL_SOURCES port/windows/port_windows.c)
elseif(SERTOS_PORT STREQUAL "posix")
    list(APPEND SERTOS_KERNEL_SOURCES port/posix/port_posix.c)
elseif(SERTOS_PORT STREQUAL "cortex-m33")
    list(APPEND SERTOS_KERNEL_SOURCES port/arm/cortex-m33/port_cpu.c port/arm/cortex-m33/port_context.s)
elseif(SERTOS_PORT STREQUAL "cortex-m4")
    list(APPEND SERTOS_KERNEL_SOURCES port/arm/cortex-m4/port_cpu.c port/arm/cortex-m4/port_context.s)
elseif(SERTOS_PORT STREQUAL "cortex-m3")
    list(APPEND SERTOS_KERNEL_SOURCES port/arm/cortex-m3/port_cpu.c port/arm/cortex-m3/port_context.s)
elseif(SERTOS_PORT STREQUAL "cortex-m0")
    list(APPEND SERTOS_KERNEL_SOURCES port/arm/cortex-m0/port_cpu.c port/arm/cortex-m0/port_context.s)
elseif(SERTOS_PORT STREQUAL "rv32i")
    list(APPEND SERTOS_KERNEL_SOURCES port/riscv/rv32i/port_cpu.c port/riscv/rv32i/port_context.s)
else()
    message(FATAL_ERROR "Unsupported SERTOS_PORT architecture: ${SERTOS_PORT}")
endif()

# Resolve submodule dependency directories (check modules/ first, fallback to parent ../)
set(SERTOS_DEPENDENCIES atomic ring_buffer memory_pool linked_list bitmap crc fsm)
set(SERTOS_DEP_INCLUDES)
set(SERTOS_DEP_SOURCES)

foreach(DEP ${SERTOS_DEPENDENCIES})
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/modules/${DEP}")
        set(DEP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/modules/${DEP}")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../${DEP}")
        set(DEP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../${DEP}")
    else()
        message(FATAL_ERROR "Submodule dependency '${DEP}' not found in modules/ or ../")
    endif()

    list(APPEND SERTOS_DEP_INCLUDES "${DEP_DIR}")

    if(EXISTS "${DEP_DIR}/${DEP}.c")
        list(APPEND SERTOS_DEP_SOURCES "${DEP_DIR}/${DEP}.c")
    endif()
endforeach()

add_library(sertos_kernel STATIC ${SERTOS_KERNEL_SOURCES} ${SERTOS_DEP_SOURCES})

target_include_directories(sertos_kernel PUBLIC
    inc
    port
    ${SERTOS_DEP_INCLUDES}
)

set_target_properties(sertos_kernel PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/lib/${SERTOS_PORT}"
    OUTPUT_NAME "sertos_${SERTOS_PORT}"
)
