cmake_minimum_required(VERSION 4.0)

# Default build type for single-config generators (must be before project()).
if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
endif ()

# CMP0091 is NEW by default on CMake 4.0, so no cmake_policy() call needed.
if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL" CACHE STRING "MSVC runtime library")
endif ()

option(OrbisOS_build "Playstation" OFF)
option(HorizonOS_build "Nintendo Switch" OFF)

project(SF_Engine LANGUAGES CXX C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_VERBOSE_MAKEFILE OFF)
set(SLANG_VERSION "2026.14.1")

include(FetchContent)

#
# Dependencies (Conan)
#
find_package(OpenAL CONFIG REQUIRED)
find_package(assimp CONFIG REQUIRED)
find_package(OpenCLHeaders REQUIRED)
find_package(OpenCLHeadersCpp REQUIRED)
find_package(OpenCLICDLoader REQUIRED)
find_package(freetype CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(libxml2 CONFIG REQUIRED)
find_package(libsodium CONFIG REQUIRED)
find_package(spirv-cross CONFIG REQUIRED)
find_package(SPIRV-Tools CONFIG REQUIRED)
find_package(volk CONFIG REQUIRED)
find_package(VulkanHeaders CONFIG REQUIRED)
find_package(VulkanLoader CONFIG REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(PNG REQUIRED)
find_package(ZLIB REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(spirv-reflect-static CONFIG REQUIRED)
find_package(utf8cpp CONFIG QUIET) # header-only, may not generate a config file
find_package(libraw CONFIG REQUIRED)
find_package(plutosvg CONFIG REQUIRED)
find_package(tinyexr CONFIG REQUIRED)
find_package(openfbx REQUIRED)
find_package(WebP CONFIG REQUIRED) # Conan libwebp -> WebP-config.cmake, targets WebP::*
find_package(TinyOBJLoader REQUIRED)
find_package(opusfile REQUIRED)

if (WIN32)
    find_package(DirectX-Headers REQUIRED)
    find_package(D3D12MemoryAllocator CONFIG REQUIRED)
endif ()

#
# Sources
#
file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/*.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/*.cxx"
        "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/*.hxx"
)
# Keep build output and generated files out of the glob.
list(FILTER ENGINE_SOURCES EXCLUDE REGEX "/(build|cmake-build[^/]*|CMakeFiles)/")

add_library(SF_Engine STATIC ${ENGINE_SOURCES})

set_target_properties(SF_Engine PROPERTIES
        OUTPUT_NAME "SF_Engine"
        LINKER_LANGUAGE CXX
)
# NOTE: if SF_Engine ever becomes SHARED, re-add WINDOWS_EXPORT_ALL_SYMBOLS /
# visibility presets and the SF_Engine_EXPORTS define.

target_compile_features(SF_Engine PUBLIC cxx_std_26)

target_compile_definitions(SF_Engine PUBLIC
        GLM_FORCE_RADIANS
        GLM_FORCE_DEPTH_ZERO_TO_ONE
        GLM_ENABLE_EXPERIMENTAL
        SPDLOG_COMPILED_LIB
        VK_NO_PROTOTYPES
)

#
# Platform selection (console builds first, they may also set UNIX)
#
if (OrbisOS_build)
    target_compile_definitions(SF_Engine PUBLIC _PLATFORM_PLAYSTATION)
elseif (HorizonOS_build)
    target_compile_definitions(SF_Engine PUBLIC _PLATFORM_NINTENDO_SWITCH)
elseif (WIN32)
    target_compile_definitions(SF_Engine PUBLIC
            _PLATFORM_WINDOWS
            VK_USE_PLATFORM_WIN32_KHR
            NOMINMAX
            _CRT_SECURE_NO_WARNINGS
            WIN32_LEAN_AND_MEAN
    )
    if (MSVC)
        target_compile_options(SF_Engine PRIVATE /utf-8)
    endif ()
    target_link_libraries(SF_Engine PUBLIC
            kernel32 user32 gdi32 winspool shell32 ole32 oleaut32
            uuid comdlg32 advapi32
    )
elseif (APPLE)
    target_compile_definitions(SF_Engine PUBLIC
            _PLATFORM_MACOS
            VK_USE_PLATFORM_METAL_EXT
    )
elseif (UNIX)
    target_compile_definitions(SF_Engine PUBLIC
            _PLATFORM_LINUX
            VK_USE_PLATFORM_XLIB_KHR
    )
    target_link_libraries(SF_Engine PUBLIC X11 pthread dl)
endif ()

#
# box3d
#
# Disable extras before MakeAvailable (option names depend on box3d; harmless if absent).
set(BOX3D_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BOX3D_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(box3d
        GIT_REPOSITORY https://github.com/erincatto/box3d.git
        GIT_TAG v0.1.0)
FetchContent_MakeAvailable(box3d)

#
# Slang (prebuilt)
#
if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|ARM64|aarch64)$")
    set(SLANG_ARCH "aarch64")
else ()
    set(SLANG_ARCH "x86_64")
endif ()

if (WIN32)
    if (SLANG_ARCH STREQUAL "aarch64")
        set(SLANG_ARCHIVE_NAME "slang-${SLANG_VERSION}-windows-aarch64.zip")
        set(SLANG_ARCHIVE_HASH "SHA256=5067047bb35ae5675b06a3467d0b302d9816727450a803cb3770660bef684f37")
    else ()
        set(SLANG_ARCHIVE_NAME "slang-${SLANG_VERSION}-windows-x86_64.zip")
        set(SLANG_ARCHIVE_HASH "SHA256=5ed0a59d650a0af0aca45d5db4e083b3d8fb5cea05748747dd95dfbe9c580658")
    endif ()
elseif (APPLE)
    if (SLANG_ARCH STREQUAL "aarch64")
        set(SLANG_ARCHIVE_NAME "slang-${SLANG_VERSION}-macos-aarch64.zip")
        set(SLANG_ARCHIVE_HASH "SHA256=2976c3a9a6f4d77b5734d00b5d841d1ff087d9965d9006b9b4d73edd0062cb7d")
    else ()
        set(SLANG_ARCHIVE_NAME "slang-${SLANG_VERSION}-macos-x86_64.zip")
        set(SLANG_ARCHIVE_HASH "SHA256=a3da109bfb732ab3f09beb6999f9831d9be440fa9a1843006b7ea56498f545d6")
    endif ()
elseif (UNIX)
    if (SLANG_ARCH STREQUAL "aarch64")
        set(SLANG_ARCHIVE_NAME "slang-${SLANG_VERSION}-linux-aarch64.tar.gz")
        set(SLANG_ARCHIVE_HASH "SHA256=438325c6529ba658f571614d9d551cdff09e730836d29c26c290afc8ed996985")
    else ()
        set(SLANG_ARCHIVE_NAME "slang-${SLANG_VERSION}-linux-x86_64.tar.gz")
        set(SLANG_ARCHIVE_HASH "SHA256=21f2d7847385a770e569fb61b1507a7794d742d97850bce0432bff0032ca005f")
    endif ()
endif ()

set(SF_HAS_SLANG OFF)
if (DEFINED SLANG_ARCHIVE_NAME AND NOT OrbisOS_build AND NOT HorizonOS_build)
    set(SF_HAS_SLANG ON)
    message(STATUS "Slang: fetching ${SLANG_ARCHIVE_NAME}")

    FetchContent_Declare(slang_prebuilt
            URL "https://github.com/shader-slang/slang/releases/download/v${SLANG_VERSION}/${SLANG_ARCHIVE_NAME}"
            URL_HASH ${SLANG_ARCHIVE_HASH}
    )
    FetchContent_MakeAvailable(slang_prebuilt)

    add_library(slang::slang SHARED IMPORTED GLOBAL)
    if (WIN32)
        set_target_properties(slang::slang PROPERTIES
                IMPORTED_IMPLIB "${slang_prebuilt_SOURCE_DIR}/lib/slang.lib"
                IMPORTED_LOCATION "${slang_prebuilt_SOURCE_DIR}/bin/slang.dll"
                INTERFACE_INCLUDE_DIRECTORIES "${slang_prebuilt_SOURCE_DIR}/include"
        )
    elseif (APPLE)
        set_target_properties(slang::slang PROPERTIES
                IMPORTED_LOCATION "${slang_prebuilt_SOURCE_DIR}/lib/libslang.dylib"
                INTERFACE_INCLUDE_DIRECTORIES "${slang_prebuilt_SOURCE_DIR}/include"
        )
    else ()
        set_target_properties(slang::slang PROPERTIES
                IMPORTED_LOCATION "${slang_prebuilt_SOURCE_DIR}/lib/libslang.so"
                INTERFACE_INCLUDE_DIRECTORIES "${slang_prebuilt_SOURCE_DIR}/include"
        )
    endif ()
endif ()

#
# Linking (imported targets propagate their own include dirs)
#
target_link_libraries(SF_Engine PUBLIC
        OpenAL::OpenAL
        assimp::assimp
        OpenCL::Headers
        OpenCL::HeadersCpp
        OpenCL::OpenCL
        Freetype::Freetype
        glfw
        glm::glm
        LibXml2::LibXml2
        libsodium::libsodium
        spirv-cross::spirv-cross
        SPIRV-Tools
        volk::volk
        Vulkan::Loader
        GPUOpen::VulkanMemoryAllocator
        fmt::fmt
        PNG::PNG
        ZLIB::ZLIB
        spdlog::spdlog
        spirv-reflect-static
        box3d::box3d
        libraw::libraw
        openfbx::openfbx
        tinyobjloader::tinyobjloader
        plutosvg::plutosvg
        tinyexr::tinyexr
        WebP::webp
        WebP::webpdemux
        opusfile::opusfile
)

if (TARGET utf8cpp::utf8cpp)
    target_link_libraries(SF_Engine PUBLIC utf8cpp::utf8cpp)
endif ()

if (SF_HAS_SLANG)
    target_link_libraries(SF_Engine PUBLIC slang::slang)
endif ()

if (WIN32)
    target_link_libraries(SF_Engine PUBLIC Microsoft::DirectX-Headers)
    target_link_libraries(SF_Engine PRIVATE GPUOpen::D3D12MemoryAllocator)
elseif (UNIX AND NOT APPLE)
    target_link_libraries(SF_Engine PRIVATE evdev udev)
endif ()

target_include_directories(SF_Engine PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

