# RefRaster
pure C software rasterizer

cmake -S . -B build_debug -DCMAKE_BUILD_TYPE=Debug && cmake --build build_debug -j16

cmake -S . -B build_release -DCMAKE_BUILD_TYPE=Release && cmake --build build_release -j16

cmake -S . -B build_debug -DCMAKE_BUILD_TYPE=Debug && cmake --build build_debug -j16 && valgrind --leak-check=full --show-leak-kinds=all --num-callers=40 --show-reachable=yes --leak-resolution=high ./build_debug/raster