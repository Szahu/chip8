cmake_minimum_required(VERSION 3.7)
project(CHIP8EMU)

# SDL
set(SDL_TEST OFF)
add_subdirectory("./SDL")

include_directories("./SDL/include")

add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(${PROJECT_NAME} SDL3::SDL3)