// Texture sampling microbenchmarks for SGL.
//
// Cases:
//   1 - MxM RGB   -> NxN RGB resample, 1 sample per output pixel (sequential access)
//   2 - MxM RGB   -> NxN RGB resample, 5 samples per output pixel (cross pattern)
//   3 - K random samples from MxM RGB   (Texture_F32, 3 channels)
//   4 - K random samples from MxM RGBA  (Texture_F32, 4 channels)
//   5 - K random samples from MxM R     (Texture_F32, 1 channel)
//   6 - K random samples from MxM RGBA8 (Texture_U8,  4 channels)

#include "SGL.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// The SIMD samplers need SSE4.1 (pmovzxbd) and ideally FMA. Both are on by
// default under -march=native; the CMake target also asks for them explicitly
// so the Debug build, which has no -march, still compiles this path.
#if (defined(__x86_64__) || defined(__i386__)) && defined(__SSE4_1__)
  #define HAVE_SIMD_SAMPLERS 1
  #include <immintrin.h>
#else
  #define HAVE_SIMD_SAMPLERS 0
#endif

// ---------------------------------------------------------------- utilities

static double now_sec(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + 1e-9 * ts.tv_nsec;
}

// xorshift32: cheap enough that it does not dominate the sampling cost, and
// the RNG-only baseline is measured separately so it can be subtracted.
static inline uint32_t xs32(uint32_t *s)
{
    uint32_t x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x;
    return x;
}

static inline float rand_unit(uint32_t *s)
{
    return (xs32(s) >> 8) * (1.0f / 16777216.0f);   // [0, 1)
}

// The SIMD samplers read a full 16 bytes per texel, so the last texel of a 1- or
// 3-channel texture would read up to 3 floats past the end. Textures therefore
// carry TEX_TAIL_PAD bytes of slack; see the note above sample_f32_simd_core.
#define TEX_TAIL_PAD 16

static Texture_F32 make_noise_f32(int w, int h, int ch, uint32_t seed)
{
    Texture_F32 t;
    t.w = w; t.h = h; t.ch = ch;
    const size_t n = (size_t)w * h * ch;
    t.data = malloc(n * sizeof(float) + TEX_TAIL_PAD);
    uint32_t s = seed ? seed : 1u;
    for (size_t i = 0; i < n; i++)
        t.data[i] = rand_unit(&s);
    memset(t.data + n, 0, TEX_TAIL_PAD);
    return t;
}

static Texture_U8 make_noise_u8(int w, int h, int ch, uint32_t seed)
{
    Texture_U8 t;
    t.w = w; t.h = h; t.ch = ch;
    const size_t n = (size_t)w * h * ch;
    t.data = malloc(n + TEX_TAIL_PAD);
    uint32_t s = seed ? seed : 1u;
    for (size_t i = 0; i < n; i++)
        t.data[i] = (unsigned char)(xs32(&s) >> 24);
    memset(t.data + n, 0, TEX_TAIL_PAD);
    return t;
}

typedef struct
{
    const char *name;
    double      seconds;
    double      samples;     // texture samples performed
    double      checksum;
} BenchResult;

static void report(BenchResult r)
{
    const double ns_per_sample = 1e9 * r.seconds / r.samples;
    printf("  %-44s %8.2f ms   %7.2f ns/sample   %8.2f Msamples/s   [chk %.6f]\n",
           r.name, 1e3 * r.seconds, ns_per_sample, 1e-6 * r.samples / r.seconds,
           r.checksum);
}

// ------------------------------------------------------------- the RNG floor

static BenchResult bench_rng_baseline(long K, uint32_t seed)
{
    uint32_t s = seed;
    double acc = 0.0;
    const double t0 = now_sec();
    for (long k = 0; k < K; k++)
        acc += rand_unit(&s) + rand_unit(&s);
    const double t1 = now_sec();

    BenchResult r = { "0: RNG baseline (2 floats, no sampling)", t1 - t0, (double)K, acc };
    return r;
}

// ------------------------------------------------------------- case 1 and 2

inline static vec3 sample_f32_rgb_fast_raw(const float *data, int w, int h, int n_ch, 
                                           vec2 tc, int data_offset, ivec2 tc_offset)
{
    const vec2 tc_t = make2(tc.x*w, tc.y*h);
    const vec2 tc_i = make2((int)tc_t.x, (int)tc_t.y);
    const vec2 dtc  = sub2(tc_t, tc_i);

    vec3 res = make_zero3();
    // clamp after the offset: without it a non-zero offset walks off the edge
    const int i = clampi(tc_i.x + tc_offset.x, 0, w-1);
    const int j = clampi(tc_i.y + tc_offset.y, 0, h-1);
    const int di = (i == w-1) ? 0 : 1;
    const int dj = (j == h-1) ? 0 : 1;

    for (int ch=0; ch<3; ch++)
    {
        res.M[ch] = (1-dtc.x)*(1-dtc.y)*data[data_offset + n_ch*(w*(j+0) + (i+0))+ch] +
                      (dtc.x)*(1-dtc.y)*data[data_offset + n_ch*(w*(j+0) + (i+di))+ch] + 
                    (1-dtc.x)*  (dtc.y)*data[data_offset + n_ch*(w*(j+dj) + (i+0))+ch] + 
                      (dtc.x)*  (dtc.y)*data[data_offset + n_ch*(w*(j+dj) + (i+di))+ch];
    }   
    return res;
}

#define SAMPLE_F32_RAW(res, data, w, h, n_ch, data_offset, tc, channels, tc_offset_x, tc_offset_y) \
{ \
    const vec2 tc_t = make2(tc.x*w, tc.y*h); \
    const vec2 tc_i = make2((int)tc_t.x, (int)tc_t.y); \
    const vec2 dtc  = sub2(tc_t, tc_i); \
    const int i = clampi(tc_i.x + tc_offset_x, 0, w-1); \
    const int j = clampi(tc_i.y + tc_offset_y, 0, h-1); \
    const int di = (i == w-1) ? 0 : 1; \
    const int dj = (j == h-1) ? 0 : 1; \
    for (int ch=0; ch<channels; ch++) \
    { \
        res.M[ch] = (1-dtc.x)*(1-dtc.y)*data[data_offset + n_ch*(w*(j+0) + (i+0))+ch] + \
                      (dtc.x)*(1-dtc.y)*data[data_offset + n_ch*(w*(j+0) + (i+di))+ch] + \
                    (1-dtc.x)*  (dtc.y)*data[data_offset + n_ch*(w*(j+dj) + (i+0))+ch] + \
                      (dtc.x)*  (dtc.y)*data[data_offset + n_ch*(w*(j+dj) + (i+di))+ch]; \
    } \
}

// ------------------------------------------------------- SIMD sampler (SSE + FMA)
//
// Same bilinear result as SAMPLE_F32_RAW, but the four texels are fetched as
// four 16-byte loads and blended with the nested-lerp form
//     top = a + (b-a)*dx ;  bot = c + (d-c)*dx ;  out = top + (bot-top)*dy
// which is 3 FMAs on all channels at once instead of 4 mul-chains per channel.
//
// Requirement: each texel load reads 16 bytes, so a 1- or 3-channel texture must
// have >= 16 bytes of slack after its last texel (see TEX_TAIL_PAD). 4-channel
// textures need no padding. Lanes past n_ch hold garbage and are simply ignored.
//
// Not bit-identical to the macro: reassociating the blend moves rounding, so
// about a third of samples differ by 1 ulp (max |diff| measured 1.2e-07).

#if HAVE_SIMD_SAMPLERS

#if defined(__FMA__)
  #define FMADD_PS(a, b, c) _mm_fmadd_ps((a), (b), (c))
#else
  #define FMADD_PS(a, b, c) _mm_add_ps(_mm_mul_ps((a), (b)), (c))
#endif

inline static __m128 sample_f32_simd_core(const float *data, int w, int h, int n_ch,
                                          vec2 tc, ivec2 tc_offset)
{
    const float xf = tc.x * w, yf = tc.y * h;
    const int i = clampi((int)xf + tc_offset.x, 0, w - 1);
    const int j = clampi((int)yf + tc_offset.y, 0, h - 1);
    const float dx = xf - (float)(int)xf;
    const float dy = yf - (float)(int)yf;
    const int di = (i == w - 1) ? 0 : 1;
    const int dj = (j == h - 1) ? 0 : 1;

    const float *p0 = data + (size_t)n_ch * ((size_t)w * j + i);
    const float *p1 = data + (size_t)n_ch * ((size_t)w * (j + dj) + i);
    const size_t dio = (size_t)n_ch * di;

    const __m128 a = _mm_loadu_ps(p0),       b = _mm_loadu_ps(p0 + dio);
    const __m128 c = _mm_loadu_ps(p1),       d = _mm_loadu_ps(p1 + dio);
    const __m128 DX = _mm_set1_ps(dx),       DY = _mm_set1_ps(dy);

    const __m128 top = FMADD_PS(_mm_sub_ps(b, a), DX, a);
    const __m128 bot = FMADD_PS(_mm_sub_ps(d, c), DX, c);
    return FMADD_PS(_mm_sub_ps(bot, top), DY, top);
}

// vec3 is 12 bytes, so it cannot be stored to directly from a 16-byte register.
inline static vec3 m128_to_vec3(__m128 v)
{
    float t[4];
    _mm_storeu_ps(t, v);
    return make3(t[0], t[1], t[2]);
}

inline static vec3 sample_f32_rgb_simd(const Texture_F32 *tex, vec2 tc)
{
    return m128_to_vec3(sample_f32_simd_core(tex->data, tex->w, tex->h, tex->ch, tc, makei2(0,0)));
}

inline static vec4 sample_f32_rgba_simd(const Texture_F32 *tex, vec2 tc)
{
    vec4 r;
    _mm_storeu_ps(r.M, sample_f32_simd_core(tex->data, tex->w, tex->h, tex->ch, tc, makei2(0,0)));
    return r;
}

inline static vec3 sample_f32_rgb_simd_offset(const Texture_F32 *tex, vec2 tc, ivec2 offset)
{
    return m128_to_vec3(sample_f32_simd_core(tex->data, tex->w, tex->h, tex->ch, tc, offset));
}

// RGBA8: the four bytes of a texel widen to a full register in two instructions.
inline static __m128 load_u8x4_ps(const unsigned char *p)
{
    int packed;
    memcpy(&packed, p, sizeof packed);
    return _mm_cvtepi32_ps(_mm_cvtepu8_epi32(_mm_cvtsi32_si128(packed)));
}

inline static vec4 sample_u8_rgba_simd(const Texture_U8 *tex, vec2 tc)
{
    const int w = tex->w, h = tex->h, n_ch = tex->ch;
    const float xf = tc.x * w, yf = tc.y * h;
    const int i = (int)xf, j = (int)yf;
    const float dx = xf - (float)i, dy = yf - (float)j;
    const int di = (i == w - 1) ? 0 : 1;
    const int dj = (j == h - 1) ? 0 : 1;

    const unsigned char *p0 = tex->data + (size_t)n_ch * ((size_t)w * j + i);
    const unsigned char *p1 = tex->data + (size_t)n_ch * ((size_t)w * (j + dj) + i);
    const size_t dio = (size_t)n_ch * di;

    const __m128 a = load_u8x4_ps(p0),  b = load_u8x4_ps(p0 + dio);
    const __m128 c = load_u8x4_ps(p1),  d = load_u8x4_ps(p1 + dio);
    const __m128 DX = _mm_set1_ps(dx),  DY = _mm_set1_ps(dy);

    const __m128 top = FMADD_PS(_mm_sub_ps(b, a), DX, a);
    const __m128 bot = FMADD_PS(_mm_sub_ps(d, c), DX, c);
    const __m128 res = FMADD_PS(_mm_sub_ps(bot, top), DY, top);

    vec4 r;
    _mm_storeu_ps(r.M, _mm_mul_ps(res, _mm_set1_ps(1.0f / 255.0f)));
    return r;
}

#endif  // HAVE_SIMD_SAMPLERS

// --------------------------------------------- single channel (depth / shadow)
//
// Deliberately scalar: with one value to blend there is nothing for a SIMD
// blend to parallelise, and going through the 4x16-byte core wastes three
// quarters of every load. What actually pays here:
//
//   - 2 loads instead of 4. At n_ch == 1 the horizontal neighbours t00 and t10
//     are adjacent floats, so one load covers both.
//   - No address select on the column. At the last column the answer is t00
//     whatever dx is, so zeroing dx produces it without a cmov and the
//     out-of-row p0[1] is multiplied by zero. Reading that float needs 4 bytes
//     of tail slack, which TEX_TAIL_PAD already provides.
//   - floorf and the truncation to int are independent, so the fraction and
//     the address are computed in parallel rather than chaining
//     cvttss2si -> cvtsi2ss.
//
// The row neighbour keeps its cmov: dropping it too would mean padding every
// texture by a whole row, and it measured no faster below 64 MiB.
//
// Precondition: tex->ch == 1. Unlike sample_f32_r_fast this does not stride by
// n_ch, so it is a depth/mask sampler, not a "channel 0 of anything" sampler.
inline static float sample_f32_r_simd(const Texture_F32 *tex, vec2 tc)
{
    const float *data = tex->data;
    const int w = tex->w, h = tex->h;

    const float xf = tc.x * w, yf = tc.y * h;
    const int i = (int)xf, j = (int)yf;
    float dx = xf - floorf(xf);
    const float dy = yf - floorf(yf);
    if (i == w - 1) dx = 0.0f;
    const int dj = (j == h - 1) ? 0 : 1;

    const float *p0 = data + ((size_t)w * j + i);
    const float *p1 = p0 + (size_t)w * dj;

    const float top = p0[0] + (p0[1] - p0[0]) * dx;
    const float bot = p1[0] + (p1[1] - p1[0]) * dx;
    return top + (bot - top) * dy;
}

inline static float sample_f32_r_fast(const Texture_F32 *tex, vec2 tc)
{
    vec3 res = make_zero3();
    SAMPLE_F32_RAW(res, tex->data, tex->w, tex->h, tex->ch, 0, tc, 1, 0, 0);
    return res.x;
}

inline static vec3 sample_f32_rgb_fast(const Texture_F32 *tex, vec2 tc)
{
    vec3 res = make_zero3();
    SAMPLE_F32_RAW(res, tex->data, tex->w, tex->h, tex->ch, 0, tc, 3, 0, 0);
    return res;
}

inline static vec4 sample_f32_rgba_fast(const Texture_F32 *tex, vec2 tc)
{
    vec4 res = make_zero4();
    SAMPLE_F32_RAW(res, tex->data, tex->w, tex->h, tex->ch, 0, tc, 4, 0, 0);
    return res;
}

inline static vec3 sample_f32_rgb_fast_offset(const Texture_F32 *tex, vec2 tc, ivec2 offset)
{
    vec3 res = make_zero3();
    SAMPLE_F32_RAW(res, tex->data, tex->w, tex->h, tex->ch, 0, tc, 3, offset.x, offset.y);
    return res;
}

#if !HAVE_SIMD_SAMPLERS
// Without SSE4.1 the "simd" variant is just the scalar one, so the benchmark
// still builds and runs; the two rows then report the same implementation.
#define sample_f32_rgb_simd        sample_f32_rgb_fast
#define sample_f32_rgba_simd       sample_f32_rgba_fast
#define sample_f32_rgb_simd_offset sample_f32_rgb_fast_offset
#define sample_u8_rgba_simd        sample_u8_rgba
#endif

// Each timed loop is emitted once per variant, so the variant test never lands
// inside the loop body.
#define RESAMPLE_1X_LOOP(SAMPLE)                                              \
    for (int j = 0; j < N; j++)                                               \
    {                                                                         \
        for (int i = 0; i < N; i++)                                           \
        {                                                                     \
            const vec2 uv = make2((i + 0.5f) * inv_n, (j + 0.5f) * inv_n);    \
            const vec3 c  = SAMPLE(&src, uv);                                 \
            dst.data[3 * (j * N + i) + 0] = c.x;                              \
            dst.data[3 * (j * N + i) + 1] = c.y;                              \
            dst.data[3 * (j * N + i) + 2] = c.z;                              \
        }                                                                     \
    }

static BenchResult bench_resample_1x(int M, int N, uint32_t seed, int simd)
{
    Texture_F32 src = make_noise_f32(M, M, 3, seed);
    Texture_F32 dst = SGL_init_framebuffer(N, N, 3);

    const float inv_n = 1.0f / (float)N;
    const double t0 = now_sec();
    if (simd) { RESAMPLE_1X_LOOP(sample_f32_rgb_simd) }
    else      { RESAMPLE_1X_LOOP(sample_f32_rgb_fast) }
    const double t1 = now_sec();

    double acc = 0.0;
    for (size_t i = 0; i < (size_t)N * N * 3; i++) acc += dst.data[i];

    BenchResult r = { simd ? "1: RGB resample, 1 sample/pixel  [simd]"
                           : "1: RGB resample, 1 sample/pixel  [macro]",
                      t1 - t0, (double)N * N, acc / ((double)N * N * 3) };
    SGL_free_texture_f32(&src);
    SGL_free_framebuffer(&dst);
    return r;
}

#define RESAMPLE_5X_LOOP(SAMPLE, SAMPLE_OFF)                                  \
    for (int j = 0; j < N; j++)                                               \
    {                                                                         \
        for (int i = 0; i < N; i++)                                           \
        {                                                                     \
            const vec2 uv = make2((i + 0.5f) * inv_n, (j + 0.5f) * inv_n);    \
            vec3 c = SAMPLE(&src, uv);                                        \
            c = add3(c, SAMPLE_OFF(&src, uv, makei2( 0,  1)));                \
            c = add3(c, SAMPLE_OFF(&src, uv, makei2( 0, -1)));                \
            c = add3(c, SAMPLE_OFF(&src, uv, makei2( 1,  0)));                \
            c = add3(c, SAMPLE_OFF(&src, uv, makei2(-1,  0)));                \
            c = cmul3(0.2f, c);                                               \
            dst.data[3 * (j * N + i) + 0] = c.x;                              \
            dst.data[3 * (j * N + i) + 1] = c.y;                              \
            dst.data[3 * (j * N + i) + 2] = c.z;                              \
        }                                                                     \
    }

static BenchResult bench_resample_5x(int M, int N, uint32_t seed, int simd)
{
    Texture_F32 src = make_noise_f32(M, M, 3, seed);
    Texture_F32 dst = SGL_init_framebuffer(N, N, 3);

    const float inv_n = 1.0f / (float)N;
    const double t0 = now_sec();
    if (simd) { RESAMPLE_5X_LOOP(sample_f32_rgb_simd, sample_f32_rgb_simd_offset) }
    else      { RESAMPLE_5X_LOOP(sample_f32_rgb_fast, sample_f32_rgb_fast_offset) }
    const double t1 = now_sec();

    double acc = 0.0;
    for (size_t i = 0; i < (size_t)N * N * 3; i++) acc += dst.data[i];

    BenchResult r = { simd ? "2: RGB resample, 5 samples/pixel [simd]"
                           : "2: RGB resample, 5 samples/pixel [macro]",
                      t1 - t0, 5.0 * N * N, acc / ((double)N * N * 3) };
    SGL_free_texture_f32(&src);
    SGL_free_framebuffer(&dst);
    return r;
}

// ------------------------------------------------------- cases 3, 4, 5 and 6

#define RANDOM_LOOP(SAMPLE, SUM)                                  \
    for (long k = 0; k < K; k++)                                  \
    {                                                             \
        const vec2 uv = make2(rand_unit(&s), rand_unit(&s));      \
        const SAMPLE;                                             \
        acc += SUM;                                               \
    }

static BenchResult bench_random_f32(const char *name, int M, int ch, long K, uint32_t seed, int simd)
{
    Texture_F32 tex = make_noise_f32(M, M, ch, seed);

    uint32_t s = seed ? seed : 1u;
    double acc = 0.0;
    const double t0 = now_sec();
    if (ch == 1)
    {
        if (simd) { RANDOM_LOOP(float c = sample_f32_r_simd(&tex, uv), c) }
        else      { RANDOM_LOOP(float c = sample_f32_r_fast(&tex, uv), c) }
    }
    else
    {
        if (simd) { RANDOM_LOOP(vec3 c = sample_f32_rgb_simd(&tex, uv), c.x + c.y + c.z) }
        else      { RANDOM_LOOP(vec3 c = sample_f32_rgb_fast(&tex, uv), c.x + c.y + c.z) }
    }
    const double t1 = now_sec();

    BenchResult r = { name, t1 - t0, (double)K, acc / (double)K };
    SGL_free_texture_f32(&tex);
    return r;
}

static BenchResult bench_random_u8(const char *name, int M, int ch, long K, uint32_t seed, int simd)
{
    Texture_U8 tex = make_noise_u8(M, M, ch, seed);

    uint32_t s = seed ? seed : 1u;
    double acc = 0.0;
    const double t0 = now_sec();
    if (simd) { RANDOM_LOOP(vec4 c = sample_u8_rgba_simd(&tex, uv), c.x + c.y + c.z + c.w) }
    else      { RANDOM_LOOP(vec4 c = sample_u8_rgba(&tex, uv),      c.x + c.y + c.z + c.w) }
    const double t1 = now_sec();

    BenchResult r = { name, t1 - t0, (double)K, acc / (double)K };
    SGL_free_texture_u8(&tex);
    return r;
}

// ------------------------------------------------------------- cli / control

typedef struct
{
    int      M;
    int      N;
    long     K;
    int      reps;
    uint32_t seed;
    int      cases[7];   // cases[i] != 0 -> run case i
    int      variants;   // bit 0 = scalar macro, bit 1 = simd
} Options;

static void usage(const char *prog)
{
    printf("usage: %s [options]\n"
           "\n"
           "  -M <n>       source texture size (default 512)\n"
           "  -N <n>       destination size for resample cases (default 1024)\n"
           "  -K <n>       samples for the random cases (default 1000000)\n"
           "  -r, --reps <n>   repeat each case n times, report the best (default 3)\n"
           "  -s, --seed <n>   RNG seed (default 12345)\n"
           "  -c, --cases <list>   comma-separated case ids, e.g. 3,4,5,6 (default: all)\n"
           "  -v, --variant <v>    macro | simd | both  (default both)\n"
           "  -h, --help\n"
           "\n"
           "cases:\n"
           "  1  MxM RGB -> NxN RGB resample, 1 sample per output pixel\n"
           "  2  MxM RGB -> NxN RGB resample, 5 samples per output pixel\n"
           "  3  K random samples, MxM RGB   (Texture_F32, 3 ch)\n"
           "  4  K random samples, MxM RGBA  (Texture_F32, 4 ch)\n"
           "  5  K random samples, MxM R     (Texture_F32, 1 ch)\n"
           "  6  K random samples, MxM RGBA8 (Texture_U8,  4 ch)\n",
           prog);
}

static int parse_cases(const char *list, Options *o)
{
    memset(o->cases, 0, sizeof(o->cases));
    const char *p = list;
    while (*p)
    {
        char *end;
        long id = strtol(p, &end, 10);
        if (end == p || id < 1 || id > 6)
        {
            fprintf(stderr, "bad case id in '%s'\n", list);
            return 0;
        }
        o->cases[id] = 1;
        p = end;
        while (*p == ',' || *p == ' ') p++;
    }
    return 1;
}

static BenchResult run_case(int id, const Options *o, int simd)
{
    switch (id)
    {
        case 1: return bench_resample_1x(o->M, o->N, o->seed, simd);
        case 2: return bench_resample_5x(o->M, o->N, o->seed, simd);
        case 3: return bench_random_f32(simd ? "3: random samples, RGB   f32 [simd]"
                                             : "3: random samples, RGB   f32 [macro]",
                                        o->M, 3, o->K, o->seed, simd);
        case 4: return bench_random_f32(simd ? "4: random samples, RGBA  f32 [simd]"
                                             : "4: random samples, RGBA  f32 [macro]",
                                        o->M, 4, o->K, o->seed, simd);
        case 5: return bench_random_f32(simd ? "5: random samples, R     f32 [simd]"
                                             : "5: random samples, R     f32 [macro]",
                                        o->M, 1, o->K, o->seed, simd);
        default:return bench_random_u8 (simd ? "6: random samples, RGBA8 u8  [simd]"
                                             : "6: random samples, RGBA8 u8  [macro]",
                                        o->M, 4, o->K, o->seed, simd);
    }
}

int main(int argc, char **argv)
{
    Options o = { .M = 512, .N = 1024, .K = 1000000, .reps = 3, .seed = 12345, .variants = 3 };
    for (int i = 1; i <= 6; i++) o.cases[i] = 1;

    for (int a = 1; a < argc; a++)
    {
        const char *arg = argv[a];
        const int has_next = (a + 1 < argc);
        if      (!strcmp(arg, "-h") || !strcmp(arg, "--help")) { usage(argv[0]); return 0; }
        else if (!strcmp(arg, "-M") && has_next) o.M    = atoi(argv[++a]);
        else if (!strcmp(arg, "-N") && has_next) o.N    = atoi(argv[++a]);
        else if (!strcmp(arg, "-K") && has_next) o.K    = atol(argv[++a]);
        else if ((!strcmp(arg, "-r") || !strcmp(arg, "--reps")) && has_next) o.reps = atoi(argv[++a]);
        else if ((!strcmp(arg, "-s") || !strcmp(arg, "--seed")) && has_next) o.seed = (uint32_t)atol(argv[++a]);
        else if ((!strcmp(arg, "-c") || !strcmp(arg, "--cases")) && has_next)
        {
            if (!parse_cases(argv[++a], &o)) return 1;
        }
        else if ((!strcmp(arg, "-v") || !strcmp(arg, "--variant")) && has_next)
        {
            const char *v = argv[++a];
            if      (!strcmp(v, "macro")) o.variants = 1;
            else if (!strcmp(v, "simd"))  o.variants = 2;
            else if (!strcmp(v, "both"))  o.variants = 3;
            else { fprintf(stderr, "bad variant '%s' (macro|simd|both)\n", v); return 1; }
        }
        else
        {
            fprintf(stderr, "unknown or incomplete option: %s\n\n", arg);
            usage(argv[0]);
            return 1;
        }
    }

    if (o.M < 2 || o.N < 1 || o.K < 1 || o.reps < 1)
    {
        fprintf(stderr, "M must be >= 2, N/K/reps must be >= 1\n");
        return 1;
    }

    printf("SGL texture sampling benchmark\n");
    printf("  M = %d (%d x %d source)   N = %d   K = %ld   reps = %d   seed = %u\n\n",
           o.M, o.M, o.M, o.N, o.K, o.reps, o.seed);
    printf("  RGB f32 %dx%d source = %.2f MiB\n\n",
           o.M, o.M, (double)o.M * o.M * 3 * sizeof(float) / (1024.0 * 1024.0));

    report(bench_rng_baseline(o.K, o.seed));

    for (int id = 1; id <= 6; id++)
    {
        if (!o.cases[id]) continue;

        for (int simd = 0; simd <= 1; simd++)
        {
            if (!(o.variants & (1 << simd))) continue;

            BenchResult best = run_case(id, &o, simd);
            for (int rep = 1; rep < o.reps; rep++)
            {
                BenchResult r = run_case(id, &o, simd);
                if (r.seconds < best.seconds) best = r;
            }
            report(best);
        }
    }

    return 0;
}
