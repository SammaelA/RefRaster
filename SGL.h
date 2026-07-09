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
    float depth;
    vec3 norm;
} Fragment;

typedef struct 
{
    float *data;
    int w;
    int h;
    int ch;
} SGL_FrameBuffer;

typedef vec4 (*SGL_PixelShader)(const Fragment *f, const void *scene_ctx);

typedef struct
{
    Fragment *all_pts;
    uint32_t all_pts_size;
} SGL_InternalCtx;

typedef struct
{
    SGL_Camera cam;
    SGL_FrameBuffer fb;
    SGL_PixelShader ps;
    void *internal_ctx;
    void *scene_ctx;
} SGL_Pipeline;

void* SGL_init_internal_ctx();
void SGL_free_internal_ctx(SGL_InternalCtx *i_ctx);

SGL_FrameBuffer SGL_init_framebuffer(int w, int h, int ch);
void SGL_free_framebuffer(SGL_FrameBuffer *fb);
void SGL_clear_framebuffer(SGL_FrameBuffer *fb, float value);

void SGL_draw_instances(const mesh *m, const mat4 *matrices, uint32_t instance_count, SGL_Pipeline *p);
void SGL_resolve_simple(const SGL_FrameBuffer fb, unsigned char *present_buffer);

#endif