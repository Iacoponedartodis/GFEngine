cmake_minimum_required(VERSION 3.25)
project(GFEngine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(SDL2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        release-2.30.3
    GIT_SHALLOW TRUE GIT_PROGRESS TRUE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_TEST   OFF CACHE BOOL "" FORCE)
set(SDL2_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(SDL2)

FetchContent_Declare(stb GIT_REPOSITORY https://github.com/nothings/stb.git GIT_SHALLOW TRUE)
FetchContent_GetProperties(stb)
if(NOT stb_POPULATED)
    FetchContent_Populate(stb)
endif()

FetchContent_Declare(tinyobjloader
    GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
    GIT_TAG v2.0.0rc13 GIT_SHALLOW TRUE)
FetchContent_GetProperties(tinyobjloader)
if(NOT tinyobjloader_POPULATED)
    FetchContent_Populate(tinyobjloader)
endif()

if(WIN32)
    set(PLATFORM_GL_LIBS opengl32 gdi32)
else()
    find_package(OpenGL REQUIRED)
    set(PLATFORM_GL_LIBS OpenGL::GL)
endif()

add_executable(GFEngine
    src/main.cpp
    src/core/Application.cpp
    src/core/Audio.cpp
    src/core/Clock.cpp
    src/core/Window.cpp
    src/core/Renderer.cpp
    src/platform/OpenGL.cpp
    src/physics/Collision.cpp
    src/core/InputManager.cpp
    src/render/Shader.cpp
    src/render/Camera.cpp
    src/render/Mesh.cpp
    src/render/Texture.cpp
    src/render/Model.cpp
    src/render/HUD.cpp
    src/vendor/stb_image_impl.cpp
    src/vendor/tinyobj_impl.cpp
    src/ecs/World.cpp
    src/ecs/systems/MovementSystem.cpp
    src/ecs/systems/CombatSystem.cpp
    src/ecs/systems/AiSystem.cpp
    src/game/game_modes/ConquestMode.cpp
)

target_include_directories(GFEngine PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/external/glm
    ${stb_SOURCE_DIR}
    ${tinyobjloader_SOURCE_DIR}
)

target_compile_features(GFEngine PRIVATE cxx_std_20)
target_compile_definitions(GFEngine PRIVATE SDL_MAIN_HANDLED)
target_link_libraries(GFEngine PRIVATE SDL2::SDL2-static ${PLATFORM_GL_LIBS})

if(MSVC)
    target_compile_options(GFEngine PRIVATE /W4 /permissive- /EHsc /external:anglebrackets /external:W0)
else()
    target_compile_options(GFEngine PRIVATE -Wall -Wextra -Wpedantic)
endif()