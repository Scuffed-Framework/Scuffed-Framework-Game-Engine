cmake_minimum_required(VERSION 4.0)

project("SF_Editor"
    VERSION 1.0.0
    DESCRIPTION "Scufed Framework Editor Application"
    LANGUAGES C CXX
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(ARCH_SUFFIX "x64")
else()
    set(ARCH_SUFFIX "x86")
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

set(BASE_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/000-Build-${CMAKE_BUILD_TYPE}-${ARCH_SUFFIX}")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${BASE_OUTPUT_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${BASE_OUTPUT_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${BASE_OUTPUT_DIR}")
set(CMAKE_PDB_OUTPUT_DIRECTORY "${BASE_OUTPUT_DIR}")

if(CMAKE_CONFIGURATION_TYPES)
    foreach(CONFIG ${CMAKE_CONFIGURATION_TYPES})
        string(TOUPPER ${CONFIG} CONFIG_UPPER)
        set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CONFIG_UPPER} "${BASE_OUTPUT_DIR}")
        set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CONFIG_UPPER} "${BASE_OUTPUT_DIR}")
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER} "${BASE_OUTPUT_DIR}")
        set(CMAKE_PDB_OUTPUT_DIRECTORY_${CONFIG_UPPER} "${BASE_OUTPUT_DIR}")
    endforeach()
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
elseif(MSVC)
    add_compile_options(
        /W3
        /permissive-
        /Zc:__cplusplus
        /EHsc
        /utf-8
        $<$<CONFIG:Debug>:/Zi>
        $<$<CONFIG:Release>:/O2>
    )
endif()

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /ignore:4099,4098")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /ignore:4099,4098")
add_compile_options(/wd4005 /wd4224 /wd4267 /wd4244)

file(GLOB_RECURSE SF_Editor_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
)

file(GLOB_RECURSE SF_Editor_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hxx"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)

add_executable(SF_Editor
    ${SF_Editor_SOURCES}
    ${SF_Editor_HEADERS}
)

target_link_libraries(SF_Editor
    PRIVATE SF_Engine
)

target_include_directories(SF_Editor
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
    "../SF_Engine/"
)

set_target_properties(SF_Editor PROPERTIES
    OUTPUT_NAME "SF_Editor"
    DEBUG_POSTFIX "_d"
)
