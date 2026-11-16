cmake_minimum_required(VERSION 3.20)

project(yt8531c VERSION 1.0.0 LANGUAGES C CXX)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(YT8531C_BUILD_TESTS "Build the register-level unit tests" ON)
option(YT8531C_BUILD_LINUX_USER "Build the Linux userspace MDIO tool" ON)
option(YT8531C_BUILD_LINUX_KMOD "Build the Linux phylib kernel module" OFF)
option(YT8531C_WERROR "Treat compiler warnings as errors" ON)

set(YT8531C_KERNEL_DIR "" CACHE PATH
    "Kernel build directory used when YT8531C_BUILD_LINUX_KMOD is enabled")
set(YT8531C_KERNEL_ARCH "" CACHE STRING
    "Optional ARCH= value passed to the kernel build (for cross builds)")
set(YT8531C_KERNEL_CROSS_COMPILE "" CACHE STRING
    "Optional CROSS_COMPILE= prefix passed to the kernel build")

# ---------------------------------------------------------------------------
# Language standard and common settings
# ---------------------------------------------------------------------------
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ---------------------------------------------------------------------------
# Portable core
# ---------------------------------------------------------------------------
set(YT8531C_CORE_SOURCES
    src/core/yt8531c_reg.cpp
    src/core/yt8531c_dev.cpp
    src/core/yt8531c_link.cpp
    src/core/yt8531c_eee.cpp
    src/core/yt8531c_irq.cpp
    src/core/yt8531c_led.cpp
    src/core/yt8531c_wol.cpp)

add_library(yt8531c STATIC ${YT8531C_CORE_SOURCES})
add_library(yt8531c::yt8531c ALIAS yt8531c)

target_include_directories(yt8531c
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core)

# The core must never touch an OS or a C/C++ runtime.  These flags are part of
# the contract, not a style preference.
target_compile_definitions(yt8531c PRIVATE YT8531C_VERSION_STRING="1.0.0")

if(MSVC)
    target_compile_options(yt8531c PRIVATE /W4 /EHs-c- /GR-)
    if(YT8531C_WERROR)
        target_compile_options(yt8531c PRIVATE /WX)
    endif()
else()
    target_compile_options(yt8531c PRIVATE
        -Wall -Wextra -Wpedantic -Wshadow -Wundef -Wstrict-aliasing
        -fno-exceptions -fno-rtti -fno-threadsafe-statics
        -fno-use-cxa-atexit)
    if(YT8531C_WERROR)
        target_compile_options(yt8531c PRIVATE -Werror)
    endif()
endif()

# ---------------------------------------------------------------------------
# Linux userspace support (library + CLI)
# ---------------------------------------------------------------------------
if(YT8531C_BUILD_LINUX_USER AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_library(yt8531c_mdio STATIC src/platform/linux-user/yt8531c_mdio_ioctl.c)
    add_library(yt8531c::mdio ALIAS yt8531c_mdio)
    target_include_directories(yt8531c_mdio PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
    target_compile_options(yt8531c_mdio PRIVATE
        -Wall -Wextra -Wpedantic -Wshadow -Wundef)
    if(YT8531C_WERROR)
        target_compile_options(yt8531c_mdio PRIVATE -Werror)
    endif()

    add_executable(yt8531cctl src/platform/linux-user/yt8531c_cli.c)
    target_link_libraries(yt8531cctl PRIVATE yt8531c yt8531c_mdio)
    target_compile_options(yt8531cctl PRIVATE
        -Wall -Wextra -Wpedantic -Wshadow -Wundef)
    if(YT8531C_WERROR)
        target_compile_options(yt8531cctl PRIVATE -Werror)
    endif()
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(YT8531C_BUILD_TESTS)
    enable_testing()

    add_executable(yt8531c_tests
        tests/test_main.cpp
        tests/test_framework.cpp
        tests/mock_mdio.cpp
        tests/test_core.cpp
        tests/test_link.cpp
        tests/test_peripheral.cpp)
    target_link_libraries(yt8531c_tests PRIVATE yt8531c)
    target_compile_options(yt8531c_tests PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra -Wpedantic -Wshadow -Wundef>)
    if(YT8531C_WERROR)
        target_compile_options(yt8531c_tests PRIVATE
            $<$<COMPILE_LANGUAGE:CXX>:-Werror>)
    endif()

    add_test(NAME yt8531c_core COMMAND yt8531c_tests)

    # The ABI test is compiled and linked as C to prove there is no hidden
    # dependency on the C++ runtime.
    add_executable(yt8531c_abi_test tests/test_abi.c)
    target_link_libraries(yt8531c_abi_test PRIVATE yt8531c)
    set_target_properties(yt8531c_abi_test PROPERTIES LINKER_LANGUAGE C)
    target_compile_options(yt8531c_abi_test PRIVATE
        -Wall -Wextra -Wpedantic -Wshadow -Wundef)
    if(YT8531C_WERROR)
        target_compile_options(yt8531c_abi_test PRIVATE -Werror)
    endif()
    add_test(NAME yt8531c_abi COMMAND yt8531c_abi_test)
endif()

# ---------------------------------------------------------------------------
# Linux kernel module (built through the kernel's own Kbuild, driven by CMake)
# ---------------------------------------------------------------------------
if(YT8531C_BUILD_LINUX_KMOD)
    include(cmake/yt8531c-kmod.cmake)
endif()
