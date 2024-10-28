cmake_minimum_required(VERSION 3.20)
project(Game)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)

include_directories(${CMAKE_SOURCE_DIR}/../Lib/SDL2/include)
include_directories(${CMAKE_SOURCE_DIR}/../Lib/SDL2_image/include)

link_directories(${CMAKE_SOURCE_DIR}/../Lib/SDL2/lib/x64)
link_directories(${CMAKE_SOURCE_DIR}/../Lib/SDL2_image/lib/x64)

add_executable(Game
    Main.cpp
    Game.cpp
    Systems.cpp
    Math.cpp
    Random.cpp
    Game.h
    Components.h
    Systems.h
    Math.h
    Random.h
)

target_link_libraries(Game SDL2 SDL2_image)
