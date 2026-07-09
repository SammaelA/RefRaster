cmake_minimum_required(VERSION 3.5)
project(RefRaster)

# Set default build configuration to Release
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(SOURCES main.c image_utils.c mesh_utils.c SGL.c)

add_executable(raster ${SOURCES})

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(raster PUBLIC -Wall -Wno-unused-function -O0 -g)
else()
    # No -ffast-math: it implies -ffinite-math-only, which lets the compiler assume NaN/Inf
    # never occur and fold away the depth test's implicit NaN rejection.
    target_compile_options(raster PUBLIC -Wall -Wno-unused-function -O3 -march=native)
endif()

target_link_libraries(raster PUBLIC m GL glfw)