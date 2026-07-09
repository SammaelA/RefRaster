#ifndef SGL_H
#define SGL_H
#include "vectors.h"
#include "mesh_utils.h"

#include <stdint.h>

#define CHANNEL_R 0
#define CHANNEL_G 1
#define CHANNEL_B 2
#define CHANNEL_DEPTH 3

typedef struct
{
  vec3 pos;
  vec3 lookAt;
  vec3 up;
  float  fovy;
  float  z_near;
  float  z_far;
  float  aspect;
} SGL_Camera;

typedef struct
{
    vec2 screen_pos;
    vec2 tc;
    float depth;   // NDC z, affine in screen space (used for the depth buffer)
    float inv_w;   // 1 / clip-space w, the weight for perspective-correct interpolation
    vec3 norm;
} Fragment;

typedef struct 
{
    float *data;
    int w;
    int h;
    int ch;
} Texture_F32;

typedef struct 
{
    unsigned char *data;
    int w;
    int h;
    int ch;
} Texture_U8;

typedef vec4 (*SGL_PixelShader)(const Fragment *f, const void *scene_ctx);
typedef Fragment (*SGL_VertexShader)(uint32_t instance_id, uint32_t vertex_id, const mesh *m, const void *scene_ctx);

typedef struct
{
    Fragment *all_pts;
    uint32_t all_pts_size;
} SGL_InternalCtx;

typedef struct
{
    SGL_Camera cam;
    Texture_F32 fb;
    SGL_PixelShader ps;
    SGL_VertexShader vs;
    void *internal_ctx;
    void *scene_ctx;
} SGL_Pipeline;

void* SGL_init_internal_ctx();
void SGL_free_internal_ctx(SGL_InternalCtx *i_ctx);

vec3 sample_f32_rgb(const Texture_F32 *tex, vec2 tc);
Texture_F32 SGL_init_framebuffer(int w, int h, int ch);
void SGL_free_framebuffer(Texture_F32 *fb);
void SGL_clear_framebuffer(Texture_F32 *fb, float value);

void SGL_draw_instances(const mesh *m, uint32_t instance_count, SGL_Pipeline *p);
void SGL_resolve_simple(const Texture_F32 fb, Texture_U8 present_buffer);

#endif