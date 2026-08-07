#ifndef SGL_H
#define SGL_H
#include "vectors.h"
#include "mesh_utils.h"

#include <stdint.h>

#define CHANNEL_R 0
#define CHANNEL_G 1
#define CHANNEL_B 2
#define CHANNEL_DEPTH 3

#define MAX_MIPS 10

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
    vec4 clip_pos;   // homogeneous clip-space position, before the perspective divide
    vec2 tc;
    vec3 norm;
} VertexOut;

typedef struct
{
    vec2 screen_pos;
    vec2 tc;
    float depth;   // NDC z, affine in screen space (used for the depth buffer)
    float inv_w;   // 1 / clip-space w, the weight for perspective-correct interpolation
    vec3 norm;
    uint32_t frag_id;
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
    float *data[MAX_MIPS];
    int ws[MAX_MIPS];
    int hs[MAX_MIPS];
    int ch;
    int mips;
} Texture_F32Mip;

typedef struct 
{
    unsigned char *data;
    int w;
    int h;
    int ch;
} Texture_U8;

typedef struct
{
    vec3 cam_pos;
    mat4 model, view, proj;
    mat4 MVP, MVInvTransposed;
    mat4 normalToWorld;
    const void *scene_ctx;
} SGL_Uniforms;

typedef vec4 (*SGL_PixelShader)(const Fragment *f, const SGL_Uniforms *uniforms);
typedef VertexOut (*SGL_VertexShader)(uint32_t instance_id, uint32_t vertex_id, const mesh *m, const SGL_Uniforms *scene_ctx);

typedef struct
{
    VertexOut *all_pts;
    uint32_t all_pts_size;
} SGL_InternalCtx;

typedef struct
{
    SGL_Camera cam;
    Texture_F32 fb;
    SGL_PixelShader ps;
    SGL_VertexShader vs;
    SGL_InternalCtx *internal_ctx;
    void *scene_ctx;
} SGL_Pipeline;

void* SGL_init_internal_ctx();
void SGL_free_internal_ctx(SGL_InternalCtx *i_ctx);

vec3 sample_f32_rgb(const Texture_F32 *tex, vec2 tc);
vec3 sample_f32_rgb_mip(const Texture_F32Mip *tex, vec2 tc, float mip);
vec4 gather_f32(const Texture_F32 *tex, vec2 tc, int channel, vec2 *out_dtc);

Texture_F32Mip SGL_create_mipmaps(const Texture_F32 *tex);
void SGL_free_texture_f32(Texture_F32 *tex);
void SGL_free_texture_u8(Texture_U8 *tex);
void SGL_free_texture_f32_mip(Texture_F32Mip *tex);

Texture_F32 SGL_init_framebuffer(int w, int h, int ch);
void SGL_free_framebuffer(Texture_F32 *fb);
void SGL_clear_framebuffer(Texture_F32 *fb, float value);

void SGL_draw_model(SGL_Pipeline *p, const mesh *m);
void SGL_draw_instances(SGL_Pipeline *p, const mesh *m, const mat4 *instances, uint32_t instance_count);
void SGL_resolve_simple(const Texture_F32 fb, Texture_U8 present_buffer);

#endif