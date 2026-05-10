#include "image_utils.h"
#include "vectors.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

void print3x3(mat3 m)
{
    printf("%f, %f, %f\n", m.cols[0].x, m.cols[0].y, m.cols[0].z);
    printf("%f, %f, %f\n", m.cols[1].x, m.cols[1].y, m.cols[1].z);
    printf("%f, %f, %f\n", m.cols[2].x, m.cols[2].y, m.cols[2].z);
}
void print4x4(mat4 m)
{
    printf("%f, %f, %f, %f\n", m.cols[0].x, m.cols[0].y, m.cols[0].z, m.cols[0].w);
    printf("%f, %f, %f, %f\n", m.cols[1].x, m.cols[1].y, m.cols[1].z, m.cols[1].w);
    printf("%f, %f, %f, %f\n", m.cols[2].x, m.cols[2].y, m.cols[2].z, m.cols[2].w);
    printf("%f, %f, %f, %f\n", m.cols[3].x, m.cols[3].y, m.cols[3].z, m.cols[3].w);
}

typedef struct
{
    vec2 screen_pos;
    vec2 tc;
    float depth;
    vec3 norm;
} RastPoint;

#define CHANNEL_R 0
#define CHANNEL_G 1
#define CHANNEL_B 2
#define CHANNEL_DEPTH 3
typedef struct 
{
    float *data;
    int w;
    int h;
    int ch;
} FrameBuffer;

FrameBuffer init_framebuffer(int w, int h, int ch)
{
    FrameBuffer fb;
    fb.w = w;
    fb.h = h;
    fb.ch = ch;
    fb.data = malloc(w * h * ch * sizeof(float));
    return fb;
}
void free_framebuffer(FrameBuffer *fb)
{
    free(fb->data);
    fb->data = NULL;
}
void clear_framebuffer(FrameBuffer *fb, float value)
{
    for (int i = 0; i < fb->w * fb->h; i++)
    {
        fb->data[i*fb->ch + CHANNEL_R] = value;
        fb->data[i*fb->ch + CHANNEL_G] = value;
        fb->data[i*fb->ch + CHANNEL_B] = value;
        fb->data[i*fb->ch + CHANNEL_DEPTH] = 2.0f;
    }
}
void save_framebuffer_to_image_RGB(const FrameBuffer *fb, const char *filename)
{
    save_image_f32_png_rgb(fb->data, filename, fb->w, fb->h, fb->ch, 2.2f);
}

void test_vectors()
{
    vec2 v = make_zero2();
    printf("v: %f, %f\n", v.x, v.y);

    {
        mat3 rm = rotate_euler3x3_zyx(1,2,3);
        print3x3(rm);
        mat3 i_rm = inverse3x3(rm);
        mat3 id = mul3x3(rm, i_rm);
        printf("dets = %f %f %f\n", det3x3(rm), det3x3(i_rm), det3x3(id));
        print3x3(id);
    }

    {
        mat4 rm = mul4x4(perspective(1.0f, 1.0f, 0.01f, 100.0f), look_at(make3(0,0,3), make3(0,0,0), make3(0,1,0)));
        print4x4(rm);
        mat4 i_rm = inverse4x4(rm);
        mat4 id = mul4x4(rm, i_rm);
        print4x4(id);
    }
  
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
    int inTri = areaABP >= 0 && areaBCP >= 0 && areaCAP >= 0;
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

void rasterize_triangle(const RastPoint *pts, FrameBuffer *fb)
{
    const vec2 a = pts[0].screen_pos;
    const vec2 b = pts[1].screen_pos;
    const vec2 c = pts[2].screen_pos;

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

            vec3 n = cmul3(depth, add3(add3(cmul3(bary.x, nx), cmul3(bary.y, ny)), cmul3(bary.z, nz)));
            vec2 tc = cmul2(depth, add2(add2(cmul2(bary.x, tx), cmul2(bary.y, ty)), cmul2(bary.z, tz)));
            vec3 col = bary; // TODO

            fb->data[(y * fb->w + x) * fb->ch + CHANNEL_R] = col.x;
            fb->data[(y * fb->w + x) * fb->ch + CHANNEL_G] = col.y;
            fb->data[(y * fb->w + x) * fb->ch + CHANNEL_B] = col.z;
            fb->data[(y * fb->w + x) * fb->ch + CHANNEL_DEPTH] = depth;
        }
    }
}

int main(int argc, char **argv)
{
    FrameBuffer fb = init_framebuffer(640, 480, 4);
    clear_framebuffer(&fb, 0.0f);

    const int tris = 100;
    const int radius = 200;
    const vec2 center = make2(320.0f, 240.0f);
    RastPoint pts[3*tris];

    for (int i = 0; i < tris; i++)
    {
        float phi1 = 2.0f * M_PI * i / tris;
        float phi2 = 2.0f * M_PI * (i + 1.0f) / tris;
        vec2 a = center;
        vec2 c = make2(center.x + radius * cosf(phi1), center.y + radius * sinf(phi1));
        vec2 b = make2(center.x + radius * cosf(phi2), center.y + radius * sinf(phi2));

        pts[3*i+0].screen_pos = a; pts[3*i+0].depth = (float)i/tris;
        pts[3*i+1].screen_pos = b; pts[3*i+1].depth = (float)i/tris;
        pts[3*i+2].screen_pos = c; pts[3*i+2].depth = (float)i/tris;
    }

    clock_t begin = clock();

    for (int i = 0; i < tris; i++)
        rasterize_triangle(pts + 3*i, &fb);

    clock_t end = clock();
    double time_spent = 1000.0 * (double)(end - begin) / CLOCKS_PER_SEC;    

    printf("Time: %.1f ms\n", time_spent);    
    // for (int y = 0; y < fb.h; y++)
    // {
    //     for (int x = 0; x < fb.w; x++)
    //     {
    //         float u = (float)x / fb.w;
    //         float v = (float)y / fb.h;
    //         fb.data[y * fb.w * fb.ch + x * fb.ch + 0] = u;
    //         fb.data[y * fb.w * fb.ch + x * fb.ch + 1] = v;
    //         fb.data[y * fb.w * fb.ch + x * fb.ch + 2] = 0.0f;
    //     }
    // }

    save_framebuffer_to_image_RGB(&fb, "saves/test.png");

    free_framebuffer(&fb);
    return 0;
}