#include "SGL.h"
#include "stdlib.h"

SGL_FrameBuffer SGL_init_framebuffer(int w, int h, int ch)
{
    SGL_FrameBuffer fb;
    fb.w = w;
    fb.h = h;
    fb.ch = ch;
    fb.data = malloc(w * h * ch * sizeof(float));
    return fb;
}
void SGL_free_framebuffer(SGL_FrameBuffer *fb)
{
    free(fb->data);
    fb->data = NULL;
}
void SGL_clear_framebuffer(SGL_FrameBuffer *fb, float value)
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

void rasterize_triangle(const Fragment *pts, SGL_FrameBuffer *fb, SGL_PixelShader pixel_shader, const void *scene)
{
    //back face culling
    if (signed_area(pts[0].screen_pos, pts[1].screen_pos, pts[2].screen_pos) > 0)
        return;

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

    vec3 invDepths = make3(1.0f / pts[0].depth, 1.0f / pts[1].depth, 1.0f / pts[2].depth);
    vec2 tx = cmul2(invDepths.M[0], pts[0].tc);
    vec2 ty = cmul2(invDepths.M[1], pts[1].tc);
    vec2 tz = cmul2(invDepths.M[2], pts[2].tc);
    vec3 nx = cmul3(invDepths.M[0], pts[0].norm);
    vec3 ny = cmul3(invDepths.M[1], pts[1].norm);
    vec3 nz = cmul3(invDepths.M[2], pts[2].norm);

    // Loop over the block of pixels covering the triangle bounds
    for (int y = blockStartY; y <= blockEndY; y++)
    {
        for (int x = blockStartX; x <= blockEndX; x++)
        {
            vec2 p = make2(x, y);
            vec3 bary;
            if (!in_triangle(p, a, b, c, &bary))
                continue;

            float depth = 1.0f / dot3(invDepths, bary);
            float buffer_depth = fb->data[(y * fb->w + x) * fb->ch + CHANNEL_DEPTH];
            if (depth >= buffer_depth)
                continue;

            Fragment f;
            f.depth = depth;
            f.norm = cmul3(depth, add3(add3(cmul3(bary.x, nx), cmul3(bary.y, ny)), cmul3(bary.z, nz)));
            f.tc = cmul2(depth, add2(add2(cmul2(bary.x, tx), cmul2(bary.y, ty)), cmul2(bary.z, tz)));
            f.screen_pos = p;

            vec4 res_color = pixel_shader(&f, scene);

            fb->data[(y * fb->w + x) * fb->ch + CHANNEL_R] = res_color.x;
            fb->data[(y * fb->w + x) * fb->ch + CHANNEL_G] = res_color.y;
            fb->data[(y * fb->w + x) * fb->ch + CHANNEL_B] = res_color.z;
            fb->data[(y * fb->w + x) * fb->ch + CHANNEL_DEPTH] = depth;
        }
    }
}

void SGL_draw_instances(const mesh *m, const mat4 *matrices, uint32_t instance_count, SGL_Pipeline *p)
{
    SGL_InternalCtx *i_ctx = (SGL_InternalCtx*)p->internal_ctx;
    if (i_ctx->all_pts_size < m->num_vertices)
    {
        i_ctx->all_pts_size = m->num_vertices;
        i_ctx->all_pts = realloc(i_ctx->all_pts, i_ctx->all_pts_size*sizeof(Fragment));
    }

    const mat4 view = look_at(p->cam.pos, p->cam.lookAt, p->cam.up);
    const mat4 proj = perspective(p->cam.fovy, p->cam.aspect, p->cam.z_near, p->cam.z_far);
    const mat4 viewProj = mul4x4(proj, view);
    const mat4 viewInvTransposed = transpose4(inverse4x4(view));

    for (int i = 0; i < m->num_vertices; i++)
    {
        vec4 pt = vmul4(viewProj, to_vec4(m->verts[i], 1.0f));
        vec3 pt_NDC = make3(pt.x / pt.w, pt.y / pt.w, pt.z / pt.w);

        i_ctx->all_pts[i].depth = pt_NDC.z;
        i_ctx->all_pts[i].screen_pos = make2(0.5f*(pt_NDC.x+1.0f)*p->fb.w, 0.5f*(pt_NDC.y+1.0f)*p->fb.h);
        i_ctx->all_pts[i].tc = m->tcs[i];
        i_ctx->all_pts[i].norm = norm3(vmul4v(viewInvTransposed, m->normals[i]));
    }

    Fragment cur_pts[3];
    for (int i = 0; i < m->num_triangles; i++)
    {
        cur_pts[0] = i_ctx->all_pts[m->indices[3*i+0]];
        cur_pts[1] = i_ctx->all_pts[m->indices[3*i+1]];
        cur_pts[2] = i_ctx->all_pts[m->indices[3*i+2]];

        if (cur_pts[0].depth < 0.0f || cur_pts[1].depth < 0.0f || cur_pts[2].depth < 0.0f)
            continue;

        rasterize_triangle(cur_pts, &p->fb, p->ps, p->scene_ctx);
    }
}

void SGL_resolve_simple(const SGL_FrameBuffer fb, unsigned char *present_buffer)
{
    for (int i=0;i<fb.w*fb.h;i++)
    {
        present_buffer[4*i+0] = 255*clampf(fb.data[4*i+0], 0.0f, 1.0f);
        present_buffer[4*i+1] = 255*clampf(fb.data[4*i+1], 0.0f, 1.0f);
        present_buffer[4*i+2] = 255*clampf(fb.data[4*i+2], 0.0f, 1.0f);
        present_buffer[4*i+3] = 255;
    }
}