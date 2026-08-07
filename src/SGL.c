#include "SGL.h"
#include "stdlib.h"
#include "assert.h"
#include <stdio.h>
#include <string.h>

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
    i_ctx->all_pts = malloc(i_ctx->all_pts_size*sizeof(VertexOut));
    return (void *)i_ctx;
}

static inline float signed_area(vec2 a, vec2 b, vec2 c)
{
    return (c.x - a.x) * (b.y - a.y) + (c.y - a.y) * (a.x - b.x);
}
static inline int in_triangle(vec2 p, vec2 a, vec2 b, vec2 c, float a_thr, vec3 *bary)
{
    const float areaABP = signed_area(a, b, p);
    const float areaBCP = signed_area(b, c, p);
    const float areaCAP = signed_area(c, a, p);
    int inTri = (areaABP <= a_thr && areaBCP <= a_thr && areaCAP <= a_thr);
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

// Signed distance to the near plane in clip space. Inside (z_ndc >= -1) means d >= 0.
// A vertex on the plane has w == z_near > 0, so the divide in make_fragment is always safe.
static inline float near_dist(vec4 c) { return c.w + c.z; }

static inline VertexOut lerp_vertex(VertexOut a, VertexOut b, float t)
{
    VertexOut r;
    r.clip_pos = add4(a.clip_pos, cmul4(t, sub4(b.clip_pos, a.clip_pos)));
    r.tc       = add2(a.tc,       cmul2(t, sub2(b.tc,       a.tc)));
    r.norm     = add3(a.norm,     cmul3(t, sub3(b.norm,     a.norm)));
    return r;
}

// Sutherland-Hodgman against the single near plane.
static int clip_near(const VertexOut in[3], VertexOut out[4])
{
    int n = 0;
    for (int i = 0; i < 3; i++)
    {
        const VertexOut cur = in[i];
        const VertexOut nxt = in[(i + 1) % 3];
        const float dc = near_dist(cur.clip_pos);
        const float dn = near_dist(nxt.clip_pos);

        if (dc >= 0.0f)
            out[n++] = cur;
        if ((dc >= 0.0f) != (dn >= 0.0f))
            out[n++] = lerp_vertex(cur, nxt, dc / (dc - dn));
    }
    return n;
}

// Perspective divide + viewport transform. Only valid once clip_near has run.
static Fragment make_fragment(VertexOut v, uint32_t w, uint32_t h, uint32_t frag_id)
{
    const float inv_w = 1.0f / v.clip_pos.w;   // w >= z_near > 0 after near clipping
    Fragment f;
    f.depth      = v.clip_pos.z * inv_w;
    f.inv_w      = inv_w;
    f.screen_pos = make2(0.5f * (v.clip_pos.x * inv_w + 1.0f) * w,
                         0.5f * (v.clip_pos.y * inv_w + 1.0f) * h);
    f.tc         = v.tc;
    f.norm       = v.norm;
    f.frag_id    = frag_id;
    return f;
}

inline static void rasterize_triangle(const Fragment *pts, Texture_F32 *fb, SGL_PixelShader pixel_shader, const void *scene)
{
    const uint32_t depth_only = (fb->ch < 4) || (pixel_shader == NULL);
    const uint32_t depth_ch_off = depth_only ? 0 : CHANNEL_DEPTH;
    const vec2 a = pts[0].screen_pos;
    const vec2 b = pts[1].screen_pos;
    const vec2 c = pts[2].screen_pos;

    // Back face and degenerate triangles culling.
    if (signed_area(a, b, c) >= -1e-12f)
        return;

    // Triangle bounds
    float minfX = minf(minf(a.x, b.x), c.x);
    float minfY = minf(minf(a.y, b.y), c.y);
    float maxfX = maxf(maxf(a.x, b.x), c.x);
    float maxfY = maxf(maxf(a.y, b.y), c.y);
    // Pixel block covering the triangle bounds
    int blockStartX = clampf(floorf(minfX), 0, fb->w);
    int blockStartY = clampf(floorf(minfY), 0, fb->h);
    int blockEndX = clampf(ceilf(maxfX), -1, fb->w - 1);
    int blockEndY = clampf(ceilf(maxfY), -1, fb->h - 1);

    // Perspective-correct interpolation weights: 1 / clip-space w, linear in screen space.
    vec3 invW = make3(pts[0].inv_w, pts[1].inv_w, pts[2].inv_w);
    vec2 tx = cmul2(invW.M[0], pts[0].tc);
    vec2 ty = cmul2(invW.M[1], pts[1].tc);
    vec2 tz = cmul2(invW.M[2], pts[2].tc);
    vec3 nx = cmul3(invW.M[0], pts[0].norm);
    vec3 ny = cmul3(invW.M[1], pts[1].norm);
    vec3 nz = cmul3(invW.M[2], pts[2].norm);

    const float a_thr = 1e-6f*absf(signed_area(a, b, c));

    // Loop over the block of pixels covering the triangle bounds
    for (int y = blockStartY; y <= blockEndY; y++)
    {
        for (int x = blockStartX; x <= blockEndX; x++)
        {
            vec2 p = make2(x, y);
            vec3 bary;
            int in_tri = in_triangle(p, a, b, c, a_thr, &bary);
            if (!in_tri)
                continue;

            float depth = bary.x * pts[0].depth + bary.y * pts[1].depth + bary.z * pts[2].depth;
            float buffer_depth = fb->data[(y * fb->w + x) * fb->ch + depth_ch_off];
            if (depth >= buffer_depth)
                continue;

            const float w = 1.0f / dot3(invW, bary);

            Fragment f;
            f.frag_id = pts[0].frag_id;
            f.depth = depth;
            f.inv_w = 1.0f / w;
            f.norm = cmul3(w, add3(add3(cmul3(bary.x, nx), cmul3(bary.y, ny)), cmul3(bary.z, nz)));
            f.tc = cmul2(w, add2(add2(cmul2(bary.x, tx), cmul2(bary.y, ty)), cmul2(bary.z, tz)));
            f.screen_pos = p;

            if (depth_only == 0)
            {
                vec4 res_color = pixel_shader(&f, scene);
                fb->data[(y * fb->w + x) * fb->ch + CHANNEL_R] = res_color.x;
                fb->data[(y * fb->w + x) * fb->ch + CHANNEL_G] = res_color.y;
                fb->data[(y * fb->w + x) * fb->ch + CHANNEL_B] = res_color.z;
            }

            fb->data[(y * fb->w + x) * fb->ch + depth_ch_off] = depth;
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
        i_ctx->all_pts = realloc(i_ctx->all_pts, i_ctx->all_pts_size*sizeof(VertexOut));
    }

    for (int i = 0; i < instance_count; i++)
    {
        for (int j = 0; j < m->num_vertices; j++)
            i_ctx->all_pts[i*m->num_vertices + j] = p->vs(i, j, m, p->scene_ctx);
    }

    for (int i = 0; i < instance_count; i++)
    {
        for (int j = 0; j < m->num_triangles; j++)
        {
            const VertexOut tri[3] = {
                i_ctx->all_pts[i*m->num_vertices + m->indices[3*j+0]],
                i_ctx->all_pts[i*m->num_vertices + m->indices[3*j+1]],
                i_ctx->all_pts[i*m->num_vertices + m->indices[3*j+2]],
            };

            VertexOut poly[4];
            const int n = clip_near(tri, poly);

            // Fan-triangulate the clipped polygon: 0, 1, or 2 triangles.
            for (int k = 1; k + 1 < n; k++)
            {
                const Fragment f[3] = {
                    make_fragment(poly[0],     p->fb.w, p->fb.h, j),
                    make_fragment(poly[k],     p->fb.w, p->fb.h, j),
                    make_fragment(poly[k + 1], p->fb.w, p->fb.h, j),
                };
                rasterize_triangle(f, &p->fb, p->ps, p->scene_ctx);
            }
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
            out.data[4*i+0] = 255*clampf(fb.data[fb.ch*i+0], 0.0f, 1.0f);
            out.data[4*i+1] = 255*clampf(fb.data[fb.ch*i+1], 0.0f, 1.0f);
            out.data[4*i+2] = 255*clampf(fb.data[fb.ch*i+2], 0.0f, 1.0f);
            out.data[4*i+3] = 255;
        }
    }
    else if (2*fb.w == out.w && 2*fb.h == out.h)
    {
        for (int j=0;j<out.h;j++)
        {
            for (int i=0;i<out.w;i++)
            {
                out.data[4*(j*out.w+i)+0] = 255*clampf(fb.data[fb.ch*((j/2)*fb.w + (i/2))+0], 0.0f, 1.0f);
                out.data[4*(j*out.w+i)+1] = 255*clampf(fb.data[fb.ch*((j/2)*fb.w + (i/2))+1], 0.0f, 1.0f);
                out.data[4*(j*out.w+i)+2] = 255*clampf(fb.data[fb.ch*((j/2)*fb.w + (i/2))+2], 0.0f, 1.0f);
                out.data[4*(j*out.w+i)+3] = 255;
            }
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

Texture_F32Mip SGL_create_mipmaps(const Texture_F32 *tex)
{
    Texture_F32Mip mip_tex;

    mip_tex.data[0] = malloc(tex->w * tex->h * tex->ch * sizeof(float));
    memcpy(mip_tex.data[0], tex->data, tex->w * tex->h * tex->ch * sizeof(float));
    mip_tex.ws[0] = tex->w;
    mip_tex.hs[0] = tex->h;
    mip_tex.ch = tex->ch;
    mip_tex.mips = 1;

    while (mip_tex.ws[mip_tex.mips - 1] > 1 || mip_tex.hs[mip_tex.mips - 1] > 1)
    {
        int prev_w = mip_tex.ws[mip_tex.mips - 1];
        int prev_h = mip_tex.hs[mip_tex.mips - 1];
        int new_w = maxf(1, prev_w / 2);
        int new_h = maxf(1, prev_h / 2);

        mip_tex.data[mip_tex.mips] = malloc(new_w * new_h * tex->ch * sizeof(float));
        mip_tex.ws[mip_tex.mips] = new_w;
        mip_tex.hs[mip_tex.mips] = new_h;

        for (int j = 0; j < new_h; j++)
        {
            for (int i = 0; i < new_w; i++)
            {
                for (int ch = 0; ch < tex->ch; ch++)
                {
                    float sum = 0.0f;
                    int count = 0;
                    for (int y = 0; y < 2; y++)
                    {
                        for (int x = 0; x < 2; x++)
                        {
                            int src_x = mini(prev_w - 1, i * 2 + x);
                            int src_y = mini(prev_h - 1, j * 2 + y);
                            sum += mip_tex.data[mip_tex.mips - 1][tex->ch*(src_y * prev_w + src_x) + ch];
                            count++;
                        }
                    }
                    mip_tex.data[mip_tex.mips][tex->ch*(j * new_w + i) + ch] = sum / count;
                }
            }
        }

        mip_tex.mips++;
    }

    return mip_tex;
}

void SGL_free_texture_f32(Texture_F32 *tex)
{
    free(tex->data);
    tex->data = NULL;
}

void SGL_free_texture_u8(Texture_U8 *tex)
{
    free(tex->data);
    tex->data = NULL;
}

void SGL_free_texture_f32_mip(Texture_F32Mip *tex)
{
    for (int i=0;i<tex->mips;i++)
    {
        free(tex->data[i]);
        tex->data[i] = NULL;
    }
    tex->mips = 0;
}


inline static vec3 sample_f32_rgb_raw(const float *data, int w, int h, int n_ch, vec2 tc)
{
    const vec2 tc_t = make2(clampf(tc.x, 0.0f, 1-1e-6f)*w, clampf(tc.y, 0.0f, 1-1e-6f)*h);
    const vec2 tc_i = make2((int)tc_t.x, (int)tc_t.y);
    const vec2 dtc  = sub2(tc_t, tc_i);

    vec3 res = make_zero3();
    const int i = tc_i.x;
    const int j = tc_i.y;
    const int di = (i == w-1) ? 0 : 1;
    const int dj = (j == h-1) ? 0 : 1;
    for (int ch=0; ch<mini(3, n_ch); ch++)
    {
        res.M[ch] = (1-dtc.x)*(1-dtc.y)*data[n_ch*(w*(j+0) + (i+0))+ch] +
                      (dtc.x)*(1-dtc.y)*data[n_ch*(w*(j+0) + (i+di))+ch] + 
                    (1-dtc.x)*  (dtc.y)*data[n_ch*(w*(j+dj) + (i+0))+ch] + 
                      (dtc.x)*  (dtc.y)*data[n_ch*(w*(j+dj) + (i+di))+ch];
    }   
    return res;
}

vec3 sample_f32_rgb(const Texture_F32 *tex, vec2 tc)
{
    return sample_f32_rgb_raw(tex->data, tex->w, tex->h, tex->ch, tc);
}

vec3 sample_f32_rgb_mip(const Texture_F32Mip *tex, vec2 tc, float mip)
{
    mip = clampf(mip, 0.0f, tex->mips-1);
    int mip0 = (int)floorf(mip);
    int mip1 = mini(mip0 + 1, tex->mips-1);
    float dmip = mip - mip0;
    vec3 c0 = sample_f32_rgb_raw(tex->data[mip0], tex->ws[mip0], tex->hs[mip0], tex->ch, tc);
    vec3 c1 = (mip1 != mip0 && dmip > 1e-4f) ? sample_f32_rgb_raw(tex->data[mip1], tex->ws[mip1], tex->hs[mip1], tex->ch, tc) : c0;
    return lerp3(dmip, c0, c1);
}

vec4 gather_f32(const Texture_F32 *tex, vec2 tc, int ch, vec2 *out_dtc)
{
    const vec2 tc_t = make2(clampf(tc.x, 0.0f, 1-1e-6f)*tex->w, clampf(tc.y, 0.0f, 1-1e-6f)*tex->h);
    const vec2 tc_i = make2((int)tc_t.x, (int)tc_t.y);
    const int i = tc_i.x;
    const int j = tc_i.y;

    if (out_dtc)
        *out_dtc = sub2(tc_t, tc_i);

    return make4(tex->data[tex->ch*(tex->w*(j+0) + (i+0))+ch],
                 tex->data[tex->ch*(tex->w*(j+0) + (i+1))+ch],
                 tex->data[tex->ch*(tex->w*(j+1) + (i+0))+ch],
                 tex->data[tex->ch*(tex->w*(j+1) + (i+1))+ch]);
}