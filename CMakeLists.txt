cmake_minimum_required(VERSION 3.21)
project(RaceGame LANGUAGES CXX)

# ----------------------------------
# C++ configuration
# ----------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ----------------------------------
# MSVC runtime configuration
# ----------------------------------
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# ----------------------------------
# FetchContent: GLFW, GLAD, GLM, JSON
# ----------------------------------
include(FetchContent)

# GLFW
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.3.9
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# GLAD
FetchContent_Declare(
    glad
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG v0.1.36
)
FetchContent_MakeAvailable(glad)

# # GLM
# FetchContent_Declare(
#     glm
#     GIT_REPOSITORY https://github.com/g-truc/glm.git
#     GIT_TAG 1.0.1
# )
# set(GLM_ENABLE_CXX_17 ON CACHE BOOL "" FORCE)
# FetchContent_MakeAvailable(glm)

# nlohmann/json
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(json)

# cpp-httplib (single header)
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.12.6
)
FetchContent_MakeAvailable(httplib)

# cesium native
FetchContent_Declare(
    cesium-native
    GIT_REPOSITORY https://github.com/CesiumGS/cesium-native.git
    GIT_TAG v0.56.0
)
FetchContent_MakeAvailable(cesium-native)

# ----------------------------------
# OpenGL
# ----------------------------------
find_package(OpenGL REQUIRED)

# ----------------------------------
# Executable
# ----------------------------------
add_executable(game
    main.cpp
)

# ----------------------------------
# Include directories
# ----------------------------------
target_include_directories(game PRIVATE
    ${CMAKE_SOURCE_DIR}/libs
    ${httplib_SOURCE_DIR}
)

find_package(OpenSSL REQUIRED)

# ----------------------------------
# Link libraries
# ----------------------------------
target_link_libraries(game PRIVATE
    glfw
    glad
    # glm::glm
    nlohmann_json::nlohmann_json
    OpenGL::GL
    OpenSSL::SSL
    OpenSSL::Crypto
    ws2_32
    crypt32
)

# ----------------------------------
# Compiler warnings
# ----------------------------------
if(MSVC)
    target_compile_options(game PRIVATE /W4)
else()
    target_compile_options(game PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ----------------------------------
# Shader hot-reload
# ----------------------------------
set(SHADER_SOURCE_DIR ${CMAKE_SOURCE_DIR}/shaders)
set(SHADER_BINARY_DIR ${CMAKE_BINARY_DIR}/shaders)

add_custom_target(copy_shaders ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${SHADER_SOURCE_DIR}
            ${SHADER_BINARY_DIR}
    COMMENT "Copying shaders to build directory"
)
add_dependencies(game copy_shaders)

target_compile_definitions(game PRIVATE SHADER_PATH="${SHADER_BINARY_DIR}/")