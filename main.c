#include "image_utils.h"
#include "vectors.h"
#include <stdio.h>
#include <stdlib.h>

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
    for (int i = 0; i < fb->w * fb->h * fb->ch; i++)
        fb->data[i] = value;
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

int main(int argc, char **argv)
{

    FrameBuffer fb = init_framebuffer(640, 480, 3);
    clear_framebuffer(&fb, 0.0f);

    for (int y = 0; y < fb.h; y++)
    {
        for (int x = 0; x < fb.w; x++)
        {
            float u = (float)x / fb.w;
            float v = (float)y / fb.h;
            fb.data[y * fb.w * fb.ch + x * fb.ch + 0] = u;
            fb.data[y * fb.w * fb.ch + x * fb.ch + 1] = v;
            fb.data[y * fb.w * fb.ch + x * fb.ch + 2] = 0.0f;
        }
    }

    save_framebuffer_to_image_RGB(&fb, "saves/test.png");

    free_framebuffer(&fb);
    return 0;
}