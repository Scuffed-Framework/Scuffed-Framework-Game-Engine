cmake_minimum_required(VERSION 4.0)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

get_directory_property(_parent_prefix PARENT_DIRECTORY CMAKE_PREFIX_PATH)
if(_parent_prefix)
    list(APPEND CMAKE_PREFIX_PATH ${_parent_prefix})
endif()

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL" CACHE STRING "MSVC runtime library")
    cmake_policy(SET CMP0091 NEW)
endif()

project(SF_Engine LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_VERBOSE_MAKEFILE OFF)

find_package(OpenAL CONFIG REQUIRED)                # OpenALConfig.cmake
find_package(assimp CONFIG REQUIRED)                # assimp-config.cmake
find_package(OpenCLHeaders REQUIRED)                # OpenCLHeadersConfig.cmake
find_package(OpenCLHeadersCpp REQUIRED)             # OpenCLHeadersCppConfig.cmake
find_package(OpenCLICDLoader REQUIRED)              # OpenCLICDLoaderConfig.cmake
find_package(freetype CONFIG REQUIRED)              # freetype-config.cmake
find_package(glfw3 CONFIG REQUIRED)                 # glfw3-config.cmake
find_package(glm CONFIG REQUIRED)                   # glm-config.cmake
find_package(glslang CONFIG REQUIRED)               # glslang-config.cmake
find_package(imgui CONFIG REQUIRED)                 # imgui-config.cmake
find_package(implot CONFIG REQUIRED)                # implot-config.cmake
find_package(imguizmo CONFIG REQUIRED)              # imguizmo-config.cmake
find_package(libxml2 CONFIG REQUIRED)               # libxml2-config.cmake
find_package(libsodium CONFIG REQUIRED)             # libsodium-config.cmake
find_package(spirv-cross CONFIG REQUIRED)           # spirv-cross-config.cmake
find_package(SPIRV-Tools CONFIG REQUIRED)           # SPIRV-ToolsConfig.cmake
find_package(volk CONFIG REQUIRED)                  # volk-config.cmake
find_package(VulkanHeaders CONFIG REQUIRED)         # VulkanHeadersConfig.cmake
find_package(VulkanLoader CONFIG REQUIRED)          # VulkanLoaderConfig.cmake
find_package(VulkanMemoryAllocator CONFIG REQUIRED) # VulkanMemoryAllocatorConfig.cmake
find_package(fmt CONFIG REQUIRED)                   # fmt-config.cmake
find_package(PNG REQUIRED)                          # PNGConfig.cmake
find_package(ZLIB REQUIRED)                         # ZLIBConfig.cmake
find_package(spdlog CONFIG REQUIRED)                # spdlog-config.cmake
find_package(spirv-reflect-static CONFIG REQUIRED)
find_package(utf8cpp CONFIG QUIET) # header-only, may not generate a config file
find_package(sol2 REQUIRED)
find_package(Bullet REQUIRED)
find_package(libraw CONFIG REQUIRED)

file(GLOB_RECURSE ENGINE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)
list(FILTER ENGINE_SOURCES EXCLUDE REGEX "CMakeLists\\.txt")

add_library(SF_Engine STATIC ${ENGINE_SOURCES})

set_target_properties(SF_Engine PROPERTIES
    OUTPUT_NAME "SF_Engine"
    WINDOWS_EXPORT_ALL_SYMBOLS ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    LINKER_LANGUAGE CXX
)

target_compile_features(SF_Engine PUBLIC cxx_std_20)

target_compile_definitions(SF_Engine PUBLIC
    SF_Engine_EXPORTS
    GLM_FORCE_RADIANS
    GLM_FORCE_DEPTH_ZERO_TO_ONE
    GLM_ENABLE_EXPERIMENTAL
    SPDLOG_COMPILED_LIB
)

target_include_directories(SF_Engine PRIVATE "./")

if(WIN32)
    target_compile_definitions(SF_Engine PUBLIC
        _PLATFORM_WINDOWS
        VK_USE_PLATFORM_WIN32_KHR
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
    if(MSVC)
        target_compile_options(SF_Engine PRIVATE /utf-8)
    endif()
    target_link_libraries(SF_Engine PUBLIC
        kernel32 user32 gdi32 winspool shell32 ole32 oleaut32
        uuid comdlg32 advapi32
    )
elseif(APPLE)
    target_compile_definitions(SF_Engine PUBLIC
        _PLATFORM_MACOS
        VK_USE_PLATFORM_MACOS_MVK
    )
elseif(UNIX)
    target_compile_definitions(SF_Engine PUBLIC
        _PLATFORM_LINUX
        VK_USE_PLATFORM_XLIB_KHR
    )
    target_link_libraries(SF_Engine PUBLIC X11 pthread dl)
endif()

target_link_libraries(SF_Engine PUBLIC
    OpenAL::OpenAL
    assimp::assimp
    OpenCL::Headers
    OpenCL::HeadersCpp
    OpenCL::OpenCL
    Freetype::Freetype
    glfw
    glm::glm
    glslang::glslang
    glslang::SPIRV
    imgui::imgui
    implot::implot
    imguizmo::imguizmo
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
    sol2::sol2
    Bullet::Bullet
    #Bullet::BulletSoftBody
    #Bullet::BulletDynamics
    #Bullet::BulletCollision
    libraw::libraw
)

target_include_directories(SF_Engine PUBLIC
    $<TARGET_PROPERTY:volk::volk,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:Vulkan::Headers,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:spirv-cross::spirv-cross,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:SPIRV-Tools,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:spirv-reflect-static,INTERFACE_INCLUDE_DIRECTORIES>
)

if(MSVC)
    target_link_libraries(SF_Engine PRIVATE kernel32)
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(ARCH_SUFFIX "x64")
else()
    set(ARCH_SUFFIX "x86")
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

set(SF_ENGINE_SHADER_OUT_DIR "../Shaders")

add_custom_target(SF_CopyShaders ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/Shaders"
        "${SF_ENGINE_SHADER_OUT_DIR}"
    COMMENT "[SF] Copying engine shaders"
)