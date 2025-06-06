cmake_minimum_required(VERSION 3.5...3.30)
set(CMAKE_ALLOW_LOOSE_LOOP_CONSTRUCTS ON)

# CMake 3.0 changed the project command, setting policy CMP0048 reverts to the old behaviour.
# See http://www.cmake.org/cmake/help/v3.0/policy/CMP0048.html
cmake_policy(PUSH)
if (CMAKE_MAJOR_VERSION GREATER 2)
    cmake_policy(SET CMP0048 NEW)
endif ()
project(zlib C)
SET(ZLIB_VERSION_MAJOR 1)
SET(ZLIB_VERSION_MINOR 2)
SET(ZLIB_VERSION_PATCH 11)
SET(ZLIB_VERSION ${ZLIB_VERSION_MAJOR}.${ZLIB_VERSION_MINOR}.${ZLIB_VERSION_PATCH})
SET(ZLIB_SOVERSION 1)
SET(PROJECT_VERSION "${ZLIB_VERSION}")
cmake_policy(POP)


option(ASM686 "Enable building i686 assembly implementation")
option(AMD64 "Enable building amd64 assembly implementation")

#set(INSTALL_BIN_DIR "${CMAKE_INSTALL_PREFIX}/bin" CACHE PATH "Installation directory for executables")
#set(INSTALL_LIB_DIR "${CMAKE_INSTALL_PREFIX}/lib" CACHE PATH "Installation directory for libraries")
#set(INSTALL_INC_DIR "${CMAKE_INSTALL_PREFIX}/include" CACHE PATH "Installation directory for headers")
#set(INSTALL_MAN_DIR "${CMAKE_INSTALL_PREFIX}/share/man" CACHE PATH "Installation directory for manual pages")
#set(INSTALL_PKGCONFIG_DIR "${CMAKE_INSTALL_PREFIX}/share/pkgconfig" CACHE PATH "Installation directory for pkgconfig (.pc) files")

include(CheckTypeSize)
include(CheckFunctionExists)
include(CheckIncludeFile)
include(CheckCSourceCompiles)
enable_testing()

check_include_file(sys/types.h HAVE_SYS_TYPES_H)
check_include_file(stdint.h HAVE_STDINT_H)
check_include_file(stddef.h HAVE_STDDEF_H)

#
# Check to see if we have large file support
#
set(CMAKE_REQUIRED_DEFINITIONS -D_LARGEFILE64_SOURCE=1)
# We add these other definitions here because CheckTypeSize.cmake
# in CMake 2.4.x does not automatically do so and we want
# compatibility with CMake 2.4.x.
if (HAVE_SYS_TYPES_H)
    list(APPEND CMAKE_REQUIRED_DEFINITIONS -DHAVE_SYS_TYPES_H)
endif ()
if (HAVE_STDINT_H)
    list(APPEND CMAKE_REQUIRED_DEFINITIONS -DHAVE_STDINT_H)
endif ()
if (HAVE_STDDEF_H)
    list(APPEND CMAKE_REQUIRED_DEFINITIONS -DHAVE_STDDEF_H)
endif ()
check_type_size(off64_t OFF64_T)
if (HAVE_OFF64_T)
    add_definitions(-D_LARGEFILE64_SOURCE=1)
endif ()
set(CMAKE_REQUIRED_DEFINITIONS) # clear variable

#
# Check for fseeko
#
check_function_exists(fseeko HAVE_FSEEKO)
if (NOT HAVE_FSEEKO)
    add_definitions(-DNO_FSEEKO)
endif ()

#
# Check for unistd.h
#
check_include_file(unistd.h Z_HAVE_UNISTD_H)

if (MSVC)
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /wd4131 /wd4127 /wd4244")
    set(CMAKE_DEBUG_POSTFIX "d")
    add_definitions(-D_CRT_SECURE_NO_DEPRECATE)
    add_definitions(-D_CRT_NONSTDC_NO_DEPRECATE)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR})
endif ()

if (NOT CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_CURRENT_BINARY_DIR)
    # If we're doing an out of source build and the user has a zconf.h
    # in their source tree...
    if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/zconf.h)
        message(STATUS "Renaming")
        message(STATUS "    ${CMAKE_CURRENT_SOURCE_DIR}/zconf.h")
        message(STATUS "to 'zconf.h.included' because this file is included with zlib")
        message(STATUS "but CMake generates it automatically in the build directory.")
        file(RENAME ${CMAKE_CURRENT_SOURCE_DIR}/zconf.h ${CMAKE_CURRENT_SOURCE_DIR}/zconf.h.included)
    endif ()
endif ()

set(ZLIB_PC ${CMAKE_CURRENT_BINARY_DIR}/zlib.pc)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/zlib.pc.cmakein
        ${ZLIB_PC} @ONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/zconf.h.cmakein
        ${CMAKE_CURRENT_BINARY_DIR}/zconf.h @ONLY)
include_directories(${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_SOURCE_DIR})


#============================================================================
# zlib
#============================================================================

set(ZLIB_PUBLIC_HDRS
        ${CMAKE_CURRENT_BINARY_DIR}/zconf.h
        zlib.h
)
set(ZLIB_PRIVATE_HDRS
        crc32.h
        deflate.h
        gzguts.h
        inffast.h
        inffixed.h
        inflate.h
        inftrees.h
        trees.h
        zutil.h
)
set(ZLIB_SRCS
        adler32.c
        compress.c
        crc32.c
        deflate.c
        gzclose.c
        gzlib.c
        gzread.c
        gzwrite.c
        inflate.c
        infback.c
        inftrees.c
        inffast.c
        trees.c
        uncompr.c
        zutil.c
)

if (NOT MINGW)
    set(ZLIB_DLL_SRCS
            win32/zlib1.rc # If present will override custom build rule below.
    )
endif ()

if (CMAKE_COMPILER_IS_GNUCC)
    if (ASM686)
        set(ZLIB_ASMS contrib/asm686/match.S)
    elseif (AMD64)
        set(ZLIB_ASMS contrib/amd64/amd64-match.S)
    endif ()

    if (ZLIB_ASMS)
        add_definitions(-DASMV)
        set_source_files_properties(${ZLIB_ASMS} PROPERTIES LANGUAGE C COMPILE_FLAGS -DNO_UNDERLINE)
    endif ()
endif ()

if (MSVC)
    if (ASM686)
        ENABLE_LANGUAGE(ASM_MASM)
        set(ZLIB_ASMS
                contrib/masmx86/inffas32.asm
                contrib/masmx86/match686.asm
        )
    elseif (AMD64)
        ENABLE_LANGUAGE(ASM_MASM)
        set(ZLIB_ASMS
                contrib/masmx64/gvmat64.asm
                contrib/masmx64/inffasx64.asm
        )
    endif ()

    if (ZLIB_ASMS)
        add_definitions(-DASMV -DASMINF)
    endif ()
endif ()

# parse the full version number from zlib.h and include in ZLIB_FULL_VERSION
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/zlib.h _zlib_h_contents)
string(REGEX REPLACE ".*#define[ \t]+ZLIB_VERSION[ \t]+\"([-0-9A-Za-z.]+)\".*"
        "\\1" ZLIB_FULL_VERSION ${_zlib_h_contents})

if (MINGW)
    # This gets us DLL resource information when compiling on MinGW.
    if (NOT CMAKE_RC_COMPILER)
        set(CMAKE_RC_COMPILER windres.exe)
    endif ()

    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/zlib1rc.obj
            COMMAND ${CMAKE_RC_COMPILER}
            -D GCC_WINDRES
            -I ${CMAKE_CURRENT_SOURCE_DIR}
            -I ${CMAKE_CURRENT_BINARY_DIR}
            -o ${CMAKE_CURRENT_BINARY_DIR}/zlib1rc.obj
            -i ${CMAKE_CURRENT_SOURCE_DIR}/win32/zlib1.rc)
    set(ZLIB_DLL_SRCS ${CMAKE_CURRENT_BINARY_DIR}/zlib1rc.obj)
endif (MINGW)

add_library(zlib SHARED ${ZLIB_SRCS} ${ZLIB_ASMS} ${ZLIB_DLL_SRCS} ${ZLIB_PUBLIC_HDRS} ${ZLIB_PRIVATE_HDRS})
add_library(zlibstatic STATIC ${ZLIB_SRCS} ${ZLIB_ASMS} ${ZLIB_PUBLIC_HDRS} ${ZLIB_PRIVATE_HDRS})
set_target_properties(zlib PROPERTIES DEFINE_SYMBOL ZLIB_DLL)
set_target_properties(zlib PROPERTIES SOVERSION 1)

INSTALL(TARGETS zlibstatic
        LIBRARY DESTINATION ${ASSIMP_LIB_INSTALL_DIR}
        ARCHIVE DESTINATION ${ASSIMP_LIB_INSTALL_DIR}
        RUNTIME DESTINATION ${ASSIMP_BIN_INSTALL_DIR}
        COMPONENT ${LIBASSIMP_COMPONENT})
