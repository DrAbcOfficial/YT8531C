# SPDX-License-Identifier: GPL-3.0-only
#
# Linux kernel module integration.
#
# The module is still built by the kernel's own Kbuild machinery; CMake only
# generates the Kbuild fragment and invokes the kernel build.  This keeps the
# project to a single build system while remaining compatible with the way
# Linux modules must be compiled.

if(NOT YT8531C_KERNEL_DIR)
    message(FATAL_ERROR
        "YT8531C_BUILD_LINUX_KMOD requires -DYT8531C_KERNEL_DIR=<kernel build directory>")
endif()

if(NOT EXISTS "${YT8531C_KERNEL_DIR}/Makefile")
    message(FATAL_ERROR
        "YT8531C_KERNEL_DIR='${YT8531C_KERNEL_DIR}' does not look like a kernel build directory")
endif()

find_program(YT8531C_KMAKE NAMES make gmake REQUIRED)

set(YT8531C_KMOD_DIR "${CMAKE_CURRENT_BINARY_DIR}/kmod")
file(MAKE_DIRECTORY "${YT8531C_KMOD_DIR}")

set(YT8531C_KMOD_OBJS "yt8531c_phylib.o")
set(YT8531C_KMOD_TARGETS "yt8531c_phylib.o")
set(YT8531C_KMOD_RULES "")

foreach(src IN LISTS YT8531C_CORE_SOURCES)
    get_filename_component(name "${src}" NAME_WE)
    list(APPEND YT8531C_KMOD_OBJS "${name}.o")
    list(APPEND YT8531C_KMOD_TARGETS "${name}.o")
    string(APPEND YT8531C_KMOD_RULES
        "$(obj)/${name}.o: ${CMAKE_CURRENT_SOURCE_DIR}/${src} FORCE\n"
        "\t$(call if_changed_dep,cc_o_cpp)\n\n")
endforeach()

string(REPLACE ";" " " YT8531C_KMOD_OBJS "${YT8531C_KMOD_OBJS}")
string(REPLACE ";" " " YT8531C_KMOD_TARGETS "${YT8531C_KMOD_TARGETS}")

set(YT8531C_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(YT8531C_CORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/core")
set(YT8531C_PHYLIB_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/linux/yt8531c_phylib.c")

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/yt8531c-kmod.Kbuild.in"
    "${YT8531C_KMOD_DIR}/Kbuild"
    @ONLY)

set(YT8531C_KMOD_ENV "")
if(YT8531C_KERNEL_ARCH)
    list(APPEND YT8531C_KMOD_ENV "ARCH=${YT8531C_KERNEL_ARCH}")
endif()
if(YT8531C_KERNEL_CROSS_COMPILE)
    list(APPEND YT8531C_KMOD_ENV "CROSS_COMPILE=${YT8531C_KERNEL_CROSS_COMPILE}")
endif()
if(YT8531C_KMOD_MODPOST_WARN)
    list(APPEND YT8531C_KMOD_ENV "KBUILD_MODPOST_WARN=1")
endif()

add_custom_target(yt8531c_kmod ALL
    COMMAND ${CMAKE_COMMAND} -E env ${YT8531C_KMOD_ENV}
            "${YT8531C_KMAKE}" -C "${YT8531C_KERNEL_DIR}"
            "M=${YT8531C_KMOD_DIR}" modules
    BYPRODUCTS "${YT8531C_KMOD_DIR}/yt8531c.ko"
    COMMENT "Building yt8531c.ko against ${YT8531C_KERNEL_DIR}"
    VERBATIM)

add_custom_target(yt8531c_kmod_clean
    COMMAND ${CMAKE_COMMAND} -E env ${YT8531C_KMOD_ENV}
            "${YT8531C_KMAKE}" -C "${YT8531C_KERNEL_DIR}"
            "M=${YT8531C_KMOD_DIR}" clean
    COMMENT "Cleaning yt8531c.ko"
    VERBATIM)
