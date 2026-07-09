cmake_minimum_required(VERSION 3.5)
project(RefRaster)

set(SOURCES main.c image_utils.c mesh_utils.c)

add_executable(raster ${SOURCES})
target_compile_options(raster PUBLIC -Wall -Wno-unused-function -O3 -march=native -ffast-math)
target_link_libraries(raster PUBLIC m GL glfw)