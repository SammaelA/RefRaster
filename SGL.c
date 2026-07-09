#include "SGL.h"
#include "stdlib.h"
#include "assert.h"

Texture_F32 SGL_init_framebuffer(int w, int h, int ch)
{
    Texture_F32 fb;
    fb.w = w;
    fb.h = h;
    fb.ch = ch;
    fb.data = malloc(w * h * ch * sizeof(float));
    return fb;
}
void SGL_free_framebuffer(Texture_F32 *fb)
{
    free(fb->data);
    fb->data = NULL;
}
void SGL_clear_framebuffer(Texture_F32 *fb, float value)
{
    for (int i = 0; i < fb->w * fb->h; i++)
    {
        fb->data[i*fb->ch + CHANNEL_R] = value;
        fb->data[i*fb->ch + CHANNEL_G] = value;
        fb->data[i*fb->ch + CHANNEL_B] = value;
        fb->data[i*fb->ch + CHANNEL_DEPTH] = 2.0f;
    }
}

void SGL_free_internal_ctx(SGL_InternalCtx *i_ctx)
{
    free(i_ctx->all_pts);
    free(i_ctx);
}

void* SGL_init_internal_ctx()
{
    SGL_InternalCtx *i_ctx = malloc(sizeof(SGL_InternalCtx));
    i_ctx->all_pts_size = 1024;
    i_ctx->all_pts = malloc(i_ctx->all_pts_size*sizeof(Fragment));
    return (void *)i_ctx;
}

static inline float signed_area(vec2 a, vec2 b, vec2 c)
{
    return (c.x - a.x) * (b.y - a.y) + (c.y - a.y) * (a.x - b.x);
}
static inline int in_triangle(vec2 p, vec2 a, vec2 b, vec2 c, vec3 *bary)
{
    const float areaABP = signed_area(a, b, p);
    const float areaBCP = signed_area(b, c, p);
    const float areaCAP = signed_area(c, a, p);
    int inTri = (areaABP <= 0 && areaBCP <= 0 && areaCAP <= 0);
    if (inTri)
    {
        const float totalArea = (areaABP + areaBCP + areaCAP);
		const float invAreaSum = 1.0f / totalArea;
        bary->x = areaBCP * invAreaSum;
        bary->y = areaCAP * invAreaSum;
        bary->z = areaABP * invAreaSum;
        return 1;
    }
    return 0;
}

void rasterize_triangle(const Fragment *pts, Texture_F32 *fb, SGL_PixelShader pixel_shader, const void *scene)
{
    const vec2 a = pts[0].screen_pos;
    const vec2 b = pts[1].screen_pos;
    const vec2 c = pts[2].screen_pos;

    //back face culling
    if (signed_area(a, b, c) > 0)
        return;

    // Triangle bounds
    float minfX = minf(minf(a.x, b.x), c.x);
    float minfY = minf(minf(a.y, b.y), c.y);
    float maxfX = maxf(maxf(a.x, b.x), c.x);
    float maxfY = maxf(maxf(a.y, b.y), c.y);
    // Pixel block covering the triangle bounds
    int blockStartX = clampf((int)(minfX), 0, fb->w - 1);
    int blockStartY = clampf((int)(minfY), 0, fb->h - 1);
    int blockEndX = clampf(ceil(maxfX), 0, fb->w - 1);
    int blockEndY = clampf(ceil(maxfY), 0, fb->h - 1);

    // Perspective-correct interpolation weights: 1 / clip-space w, linear in screen space.
    vec3 invW = make3(pts[0].inv_w, pts[1].inv_w, pts[2].inv_w);
    vec2 tx = cmul2(invW.M[0], pts[0].tc);
    vec2 ty = cmul2(invW.M[1], pts[1].tc);
    vec2 tz = cmul2(invW.M[2], pts[2].tc);
    vec3 nx = cmul3(invW.M[0], pts[0].norm);
    vec3 ny = cmul3(invW.M[1], pts[1].norm);
    vec3 nz = cmul3(invW.M[2], pts[2].norm);

    // Loop over the block of pixels covering the triangle bounds
    for (int y = blockStartY; y <= blockEndY; y++)
    {
        for (int x = blockStartX; x <= blockEndX; x++)
        {
            vec2 p = make2(x, y);
            vec3 bary;
            if (!in_triangle(p, a, b, c, &bary))
                continue;

            float depth = bary.x * pts[0].depth + bary.y * pts[1].depth + bary.z * pts[2].depth;
            float buffer_depth = fb->data[(y * fb->w + x) * fb->ch + CHANNEL_DEPTH];
            if (depth >= buffer_depth)
                continue;

            const float w = 1.0f / dot3(invW, bary);

            Fragment f;
            f.depth = depth;
            f.inv_w = 1.0f / w;
            f.norm = cmul3(w, add3(add3(cmul3(bary.x, nx), cmul3(bary.y, ny)), cmul3(bary.z, nz)));
            f.tc = cmul2(w, add2(add2(cmul2(bary.x, tx), cmul2(bary.y, ty)), cmul2(bary.z, tz)));
            f.screen_pos = p;

            vec4 res_color = pixel_shader(&f, scene);

            fb->data[(y * fb->w + x) * fb->ch + CHANNEL_R] = res_color.x;
            fb->data[(y * fb->w + x) * fb->ch + CHANNEL_G] = res_color.y;
            fb->data[(y * fb->w + x) * fb->ch + CHANNEL_B] = res_color.z;
            fb->data[(y * fb->w + x) * fb->ch + CHANNEL_DEPTH] = depth;
        }
    }
}

void SGL_draw_instances(const mesh *m, uint32_t instance_count, SGL_Pipeline *p)
{
    const uint32_t total_vertices = instance_count * m->num_vertices;
    SGL_InternalCtx *i_ctx = (SGL_InternalCtx*)p->internal_ctx;
    if (i_ctx->all_pts_size < total_vertices)
    {
        i_ctx->all_pts_size = total_vertices;
        i_ctx->all_pts = realloc(i_ctx->all_pts, i_ctx->all_pts_size*sizeof(Fragment));
    }

    for (int i = 0; i < instance_count; i++)
    {
        for (int j = 0; j < m->num_vertices; j++)
            i_ctx->all_pts[i*m->num_vertices + j] = p->vs(i, j, m, p->scene_ctx);
    }

    Fragment cur_pts[3];
    for (int i = 0; i < instance_count; i++)
    {
        for (int j = 0; j < m->num_triangles; j++)
        {
            cur_pts[0] = i_ctx->all_pts[i*m->num_vertices + m->indices[3*j+0]];
            cur_pts[1] = i_ctx->all_pts[i*m->num_vertices + m->indices[3*j+1]];
            cur_pts[2] = i_ctx->all_pts[i*m->num_vertices + m->indices[3*j+2]];

            // Reject triangles with any vertex at/behind the camera (clip w <= 0).
            if (cur_pts[0].inv_w <= 0.0f || cur_pts[1].inv_w <= 0.0f || cur_pts[2].inv_w <= 0.0f)
                continue;

            rasterize_triangle(cur_pts, &p->fb, p->ps, p->scene_ctx);
        }
    }
}

void SGL_resolve_simple(const Texture_F32 fb, Texture_U8 out)
{
    assert(out.data);
    assert(fb.data);
    assert(out.ch == 4);

    if (fb.w == out.w && fb.h == out.h)
    {
        for (int i=0;i<fb.w*fb.h;i++)
        {
            out.data[4*i+0] = 255*clampf(fb.data[4*i+0], 0.0f, 1.0f);
            out.data[4*i+1] = 255*clampf(fb.data[4*i+1], 0.0f, 1.0f);
            out.data[4*i+2] = 255*clampf(fb.data[4*i+2], 0.0f, 1.0f);
            out.data[4*i+3] = 255;
        }
    }
    else
    {
        for (int j=0;j<out.h;j++)
        {
            for (int i=0;i<out.w;i++)
            {
                vec2 tc = make2((float)i/out.w, (float)j/out.h);
                vec3 res = sample_f32_rgb(&fb, tc);
                out.data[4*(j*out.w+i)+0] = 255*clampf(res.x, 0.0f, 1.0f);
                out.data[4*(j*out.w+i)+1] = 255*clampf(res.y, 0.0f, 1.0f);
                out.data[4*(j*out.w+i)+2] = 255*clampf(res.z, 0.0f, 1.0f);
                out.data[4*(j*out.w+i)+3] = 255;
            }
        }
    }
}

vec3 sample_f32_rgb(const Texture_F32 *tex, vec2 tc)
{
    const vec2 tc_t = make2(clampf(tc.x, 0.0f, 1-1e-6f)*tex->w, clampf(tc.y, 0.0f, 1-1e-6f)*tex->h);
    const vec2 tc_i = make2((int)tc_t.x, (int)tc_t.y);
    const vec2 dtc  = sub2(tc_t, tc_i);

    vec3 res = make_zero3();
    const int i = tc_i.x;
    const int j = tc_i.y;
    for (int ch=0; ch<mini(3, tex->ch); ch++)
    {
        res.M[ch] = (1-dtc.x)*(1-dtc.y)*tex->data[tex->ch*(tex->w*(j+0) + (i+0))+ch] +
                      (dtc.x)*(1-dtc.y)*tex->data[tex->ch*(tex->w*(j+0) + (i+1))+ch] + 
                    (1-dtc.x)*  (dtc.y)*tex->data[tex->ch*(tex->w*(j+1) + (i+0))+ch] + 
                      (dtc.x)*  (dtc.y)*tex->data[tex->ch*(tex->w*(j+1) + (i+1))+ch];
    }   
    return res;
}