cmake_minimum_required(VERSION 3.12)

project(AnitraEngine)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include_directories(include)

set(SRC_FILES src/watcher/main.cpp)
list(APPEND SRC_FILES src/watcher/load_library_windows.cpp)

add_executable(${PROJECT_NAME} ${SRC_FILES})

find_package(OpenGL REQUIRED)

set(GLFW_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/lib/glfw/include")
set(GLFW_LIB_PATH    "${CMAKE_SOURCE_DIR}/lib/glfw/lib/glfw3.lib")

file(GLOB ENGINE_SRC_FILES "src/engine/*.cpp")
file(GLOB ENGINE_HEADER_FILES "src/engine/*.h")

add_library(engine SHARED ${ENGINE_SRC_FILES})

target_include_directories(
        engine PRIVATE
        "${CMAKE_SOURCE_DIR}/src/watcher"
#        "${CMAKE_SOURCE_DIR}/src/core"
#        "${CMAKE_SOURCE_DIR}/src/externals"
#        "${CMAKE_SOURCE_DIR}/src"
#        "${CMAKE_SOURCE_DIR}/include"
#        "${IMGUI_DIR}"
#        "${IMGUI_DIR}/backends"
#        "${OPENGL_INCLUDE_DIR}"
#        "${GLFW_INCLUDE_DIR}"
#        "${FLECS_INCLUDE_DIR}"
#        "${TOML_INCLUDE_DIR}"
#        "${FASTGLTF_INCLUDE_DIR}"
#        "${GLAD_INCLUDE_DIR}"
)

target_link_libraries(
        engine PRIVATE
        ${OPENGL_LIBRARIES}
        ${GLFW_LIB_PATH}
#        ${FASTGLTF_LIB_PATH}
#        externals
#        toml
        glad
)

target_compile_definitions(engine PRIVATE GLFW_STATIC)