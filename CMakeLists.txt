cmake_minimum_required(VERSION 3.5)
project(RefRaster)

# Set default build configuration to Release
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(SOURCES src/main.c src/image_utils.c src/mesh_utils.c src/SGL.c src/perlin_noise.c src/FXAA.c)

add_executable(raster ${SOURCES})

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(raster PUBLIC -Wall -Wno-unused-function -O0 -g)
else()
    target_compile_options(raster PUBLIC -Wall -Wno-unused-function -O3 -march=native)
endif()

target_include_directories(raster PUBLIC src ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(raster PUBLIC m GL glfw)

find_package(OpenMP REQUIRED)
target_link_libraries(raster PUBLIC ${OpenMP_CXX_LIBRARIES})
target_compile_options(raster PUBLIC ${OpenMP_CXX_FLAGS})