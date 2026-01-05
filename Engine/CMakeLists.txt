cmake_minimum_required(VERSION 3.20)
project(Engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- Dependencies: Vulkan (system) ---
find_package(Vulkan REQUIRED)

# --- Vendor GLFW as static using FetchContent ---
include(FetchContent)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build libraries static" FORCE)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "GLFW docs" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "GLFW tests" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "GLFW examples" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "GLFW install target" FORCE)

FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG        3.3.8
)
FetchContent_MakeAvailable(glfw)

include(FetchContent)

FetchContent_Declare(nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# --- Engine target ---
add_library(Engine STATIC
    src/Application.cpp
    src/VulkanContext.cpp
    src/GLFWWindow.cpp
    src/SwapChain.cpp
    src/Renderer.cpp
    src/TrianglesRenderPassModule.cpp
    src/MeshRenderPassModule.cpp
    src/Pipeline.cpp
    src/BufferUtils.cpp
    src/SMeshLoader.cpp
    src/AssetManager.cpp
    src/MeshAssets.cpp
)

target_include_directories(Engine
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(Engine
    PUBLIC
        Vulkan::Vulkan
    PRIVATE
        glfw
        nlohmann_json::nlohmann_json
)

if (UNIX AND NOT APPLE)
    find_package(Threads REQUIRED)
    target_link_libraries(Engine PRIVATE Threads::Threads)
endif()

target_compile_features(Engine PUBLIC cxx_std_17)

if (MSVC)
    target_compile_options(Engine PRIVATE /W4 /permissive-)

    # Suppress noisy 3rd-party warnings (GLFW)
    target_compile_options(glfw PRIVATE /W0)
else()
    # Your code: strict
    target_compile_options(Engine PRIVATE -Wall -Wextra -Wpedantic)

    # GLFW: suppress the noisy ones
    target_compile_options(glfw PRIVATE
        -Wno-pedantic
        -Wno-unused-parameter
        -Wno-missing-field-initializers
        -Wno-sign-compare
        -Wno-cast-function-type
    )
endif()

add_subdirectory(tools/ObjToSmesh)
