cmake_minimum_required (VERSION 3.24)

project (engine C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIG>)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIG>)

include(FetchContent)
set(SDL3_VERSION 3.4.8)
set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(SDL3 URL https://github.com/libsdl-org/SDL/releases/download/release-${SDL3_VERSION}/SDL3-${SDL3_VERSION}.tar.gz)
FetchContent_MakeAvailable(SDL3)

add_library(game MODULE src/game.c)
set_target_properties(game PROPERTIES PREFIX "")
target_include_directories(game PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(game PRIVATE SDL3::SDL3)

add_executable (engine src/main.c)
target_include_directories(engine PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(engine PRIVATE SDL3::SDL3)
add_dependencies(engine game)

target_compile_definitions(engine PRIVATE
    GAME_LIB_PATH="$<TARGET_FILE:game>"
    GAME_LIB_TEMP_PATH="$<TARGET_FILE_DIR:game>/game_active$<TARGET_FILE_SUFFIX:game>")

add_custom_command(TARGET engine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3-shared> $<TARGET_FILE_DIR:engine>
    VERBATIM)