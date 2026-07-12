#include "perlin_noise.h"
#include "vectors.h"

#define PERLIN_PCOUNT 256

#define SWAP(T, x, y) do { T t = (x); (x) = (y); (y) = t; } while (0)

/*
  reentrant version of rand()
  This is an implementation of rand() from GNU standard library.
*/
#define IRAND_MAX 1u << 31u;
static int irand_r(uint32_t *seed, int flip_words)
{
  // Linear congruential generator, identical to rand()
  const uint32_t a = 1103515245;
  const uint32_t c = 12345;
  const uint32_t m = IRAND_MAX;
  uint32_t res = (a * (*seed) + c) % m;
  *seed = res;
  if (flip_words)
    return ((res & 0x7FFF0000u) >> 16) | ((res & 0x00007FFFu) << 16);
  else
    return res;
}

static vec2 get_constant_vector(uint32_t v)
{
    switch (v % 4)
    {
    case 0:
        return make2(1.0f, 1.0f);
    case 1:
        return make2(-1.0f, 1.0f);
    case 2:
        return make2(-1.0f, -1.0f);
    case 3:
        return make2(1.0f, -1.0f);
    }
    return make2(0.0f, 0.0f);
}

static float fade(float t) { return t * t * t * (t * (t * 6 - 15) + 10); }

static float noise_2D(float x, float y, const uint32_t *p_table)
{
    const uint32_t i = (uint32_t)floorf(x) % (PERLIN_PCOUNT-1);
    const uint32_t j = (uint32_t)floorf(y) % (PERLIN_PCOUNT-1);
    const float xf = x - floorf(x);
    const float yf = y - floorf(y);

    float dots[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    for (int c=0; c<4; c++)
    {
        const uint32_t di = (c==0 || c==1) ? 0 : 1;
        const uint32_t dj = (c==0 || c==2) ? 0 : 1;
        const vec2 v = make2(xf - di, yf - dj);
        const uint32_t val = p_table[(p_table[i+di] + j + dj) % PERLIN_PCOUNT];
        dots[c] = dot2(v, get_constant_vector(val));
    }

    const float u = fade(xf);
    const float v = fade(yf);
    return lerpf(u, lerpf(v, dots[0], dots[1]), lerpf(v, dots[2], dots[3]));
}

static float fractal_brownian_motion(float x, float y, const uint32_t *p_table, float base_freq, uint32_t octaves)
{
    float res = 0.0f;
    float amplitude = 1.0f;
    float freq = base_freq;
    for (int i = 0; i < octaves; i++)
    {
        res += noise_2D(x * freq, y * freq, p_table) * amplitude;
        freq *= 2.0f;
        amplitude *= 0.5f;
    }

    return res;
}

void generate_perlin(float *out, uint32_t width, uint32_t height, float base_freq, uint32_t octaves, uint32_t seed)
{
    uint32_t permutation[PERLIN_PCOUNT];
    for (int i = 0; i < PERLIN_PCOUNT; i++)
        permutation[i] = i;
    for (int i = 0; i < PERLIN_PCOUNT; i++)
    {
        int j = irand_r(&seed, 1) % PERLIN_PCOUNT;
        SWAP(uint32_t, permutation[i], permutation[j]);
    }

    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++)
            out[y * width + x] = 0.5f*(fractal_brownian_motion(x, y, permutation, base_freq, octaves)+1.0f);
}