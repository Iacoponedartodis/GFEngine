cmake_minimum_required(VERSION 3.25)

project(GFEngine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(GFEngine
    src/main.cpp
    src/core/Application.cpp
    src/ecs/World.cpp
    src/ecs/systems/MovementSystem.cpp
    src/game/game_modes/ConquestMode.cpp
)

target_include_directories(GFEngine PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

if(MSVC)
    target_compile_options(GFEngine PRIVATE /W4 /permissive- /EHsc)
else()
    target_compile_options(GFEngine PRIVATE -Wall -Wextra -Wpedantic)
endif()