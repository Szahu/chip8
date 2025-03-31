cmake_minimum_required(VERSION 3.7)
project(CHIP8EMU)

set (CMAKE_CXX_STANDARD_REQUIRED 17)

# SDL
set(SDL_TEST OFF)
add_subdirectory("./SDL")

include_directories("./SDL/include")


add_executable(${PROJECT_NAME} main.cpp)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)
target_link_libraries(${PROJECT_NAME} SDL3::SDL3)