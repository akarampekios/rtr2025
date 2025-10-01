cmake_minimum_required(VERSION 3.20)

project(CyberpunkCityDemo)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Find packages
find_package(Vulkan REQUIRED)
if (NOT Vulkan_GLSLC_EXECUTABLE)
    find_program(Vulkan_GLSLC_EXECUTABLE NAMES glslc HINTS ${Vulkan_GLSLC_EXECUTABLE})
    if(NOT Vulkan_GLSLC_EXECUTABLE)
        message(FATAL_ERROR "glslc not found. Please ensure Vulkan SDK is installed and glslc is in your PATH.")
    endif()
endif()
find_package(OpenGL REQUIRED)

# Add GLFW as subdirectory
add_subdirectory(external/glfw)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/external/auto-vk/include)
include_directories(${CMAKE_SOURCE_DIR}/external/glm)
include_directories(${CMAKE_SOURCE_DIR}/external/imgui)
include_directories(${CMAKE_SOURCE_DIR}/external/stb)
include_directories(${Vulkan_INCLUDE_DIRS})

# Auto-VK library (disabled for now)
# add_subdirectory(external/auto-vk)

# Add executable
file(GLOB_RECURSE SOURCES "src/*.cpp")
add_executable(CyberpunkCityDemo ${SOURCES})

# Precompiled headers with common dependencies
file(GLOB_RECURSE PRECOMPILED_HEADERS "include/pch/*.hpp")
target_precompile_headers(CyberpunkCityDemo PRIVATE ${PRECOMPILED_HEADERS})

# Link libraries
target_link_libraries(CyberpunkCityDemo PRIVATE glfw Vulkan::Vulkan)

# Configure dependencies
target_compile_definitions(CyberpunkCityDemo PRIVATE
    GLFW_INCLUDE_VULKAN
    GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
    GLM_FORCE_DEPTH_ZERO_TO_ONE
    GLM_ENABLE_EXPERIMENTAL
    STB_IMAGE_IMPLEMENTATION
    VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
    VULKAN_HPP_NO_STRUCT_CONSTRUCTORS=1
)

# Compiler-specific options
if(MSVC)
    target_compile_options(CyberpunkCityDemo PRIVATE /W4)
    target_compile_definitions(CyberpunkCityDemo PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(CyberpunkCityDemo PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Copy shaders to output directory
file(COPY ${CMAKE_SOURCE_DIR}/shaders DESTINATION ${CMAKE_BINARY_DIR}/bin)
file(COPY ${CMAKE_SOURCE_DIR}/assets DESTINATION ${CMAKE_BINARY_DIR}/bin)

list(APPEND RAY_TRACING_SHADERS
    shaders/ray_gen.rgen
    shaders/miss.rmiss
    shaders/closest_hit.rchit
)

foreach(shader ${RAY_TRACING_SHADERS})
    get_filename_component(FILE_NAME ${shader} NAME)
    set(SPIRV_FILE ${CMAKE_BINARY_DIR}/bin/shaders/${FILE_NAME}.spv)
    add_custom_command(
        OUTPUT ${SPIRV_FILE}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/bin/shaders
        COMMAND ${Vulkan_GLSLC_EXECUTABLE} --target-env=vulkan1.2 ${CMAKE_CURRENT_SOURCE_DIR}/${shader} -o ${SPIRV_FILE}
        MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/${shader}
        COMMENT "Compiling ${shader}"
    )
    list(APPEND RAY_TRACING_SPV ${SPIRV_FILE})
endforeach()

add_custom_target(CompileRayTracingShaders ALL DEPENDS ${RAY_TRACING_SPV})
add_dependencies(CyberpunkCityDemo CompileRayTracingShaders)

# Copy ray tracing SPIR-V files to the executable directory
add_custom_command(TARGET CyberpunkCityDemo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:CyberpunkCityDemo>/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${RAY_TRACING_SPV} $<TARGET_FILE_DIR:CyberpunkCityDemo>/shaders)

install(DIRECTORY ${CMAKE_BINARY_DIR}/bin/shaders DESTINATION .)