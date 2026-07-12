#ifndef PERLIN_NOISE_H
#define PERLIN_NOISE_H
#include <stdint.h>

void generate_perlin(float *out, uint32_t width, uint32_t height, float scale, uint32_t octaves, uint32_t seed);

#endif