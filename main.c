#include "image_utils.h"
#include "vectors.h"
#include "mesh_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <GLFW/glfw3.h>

// Work time sheet
// 1) Basic render (matrix ops + obj loading + camera + window): ~5 hours

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

static inline vec4 pixel_shader(vec3 n, vec2 tc, float depth)
{
    const vec3 light_dir = norm3(make3(1,1,1));
    vec3 albedo = make3(1, 1, 1);
    float q = maxf(0.0f, dot3(n, light_dir))*0.5f + 0.25f;
    vec3 col = cmul3(q, albedo);

    return to_vec4(col, 1.0f);
}

void rasterize_triangle(const RastPoint *pts, FrameBuffer *fb)
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

            vec3 n = cmul3(depth, add3(add3(cmul3(bary.x, nx), cmul3(bary.y, ny)), cmul3(bary.z, nz)));
            vec2 tc = cmul2(depth, add2(add2(cmul2(bary.x, tx), cmul2(bary.y, ty)), cmul2(bary.z, tz)));

            vec4 res_color = pixel_shader(n, tc, depth);

            fb->data[(y * fb->w + x) * fb->ch + CHANNEL_R] = res_color.x;
            fb->data[(y * fb->w + x) * fb->ch + CHANNEL_G] = res_color.y;
            fb->data[(y * fb->w + x) * fb->ch + CHANNEL_B] = res_color.z;
            fb->data[(y * fb->w + x) * fb->ch + CHANNEL_DEPTH] = depth;
        }
    }
}

typedef struct
{
  vec3 pos;
  vec3 lookAt;
  vec3 up;
  float  fovy;
  float  z_near;
  float  z_far;
  float  aspect;
} Camera;
typedef struct
{
    Camera cam;
    mesh m;

    RastPoint *all_pts;

    FrameBuffer fb;
    unsigned char *present_buffer;
} Scene;

Scene init_scene(int width, int height, const char *filename)
{
    Scene s;

    s.cam.fovy = M_PI/6;
    s.cam.pos = make3(0,0,3);
    s.cam.lookAt = make3(0, 0, 0);
    s.cam.up = make3(0, 1, 0);
    s.cam.z_near = 0.01f;
    s.cam.z_far = 1000.0f;
    s.cam.aspect = (float)width/(float)height;

    s.m = load_obj(filename);

    s.fb = init_framebuffer(width, height, 4);
    clear_framebuffer(&s.fb, 0.0f);
    s.present_buffer = malloc(4 * width * height);
    s.all_pts = malloc(s.m.num_vertices * sizeof(RastPoint));

    return s;
}

void free_scene(Scene *s)
{
    free_mesh(&s->m);
    free_framebuffer(&s->fb);
    free(s->all_pts);
    free(s->present_buffer);
}

void render_scene(Scene *s)
{
clock_t t1 = clock();

    clear_framebuffer(&s->fb, 0.0f);

clock_t t2 = clock();

    const mat4 view = look_at(s->cam.pos, s->cam.lookAt, s->cam.up);
    const mat4 proj = perspective(s->cam.fovy, s->cam.aspect, s->cam.z_near, s->cam.z_far);
    const mat4 viewProj = mul4x4(proj, view);
    const mat4 viewInvTransposed = transpose4(inverse4x4(view));

    for (int i = 0; i < s->m.num_vertices; i++)
    {
        vec4 pt = vmul4(viewProj, to_vec4(s->m.verts[i], 1.0f));
        vec3 pt_NDC = make3(pt.x / pt.w, pt.y / pt.w, pt.z / pt.w);

        s->all_pts[i].depth = pt_NDC.z;
        s->all_pts[i].screen_pos = make2(0.5f*(pt_NDC.x+1.0f)*s->fb.w, 0.5f*(pt_NDC.y+1.0f)*s->fb.h);
        s->all_pts[i].tc = s->m.tcs[i];
        s->all_pts[i].norm = norm3(vmul4v(viewInvTransposed, s->m.normals[i]));
    }

clock_t t3 = clock();

    RastPoint cur_pts[3];
    for (int i = 0; i < s->m.num_triangles; i++)
    {
        cur_pts[0] = s->all_pts[s->m.indices[3*i+0]];
        cur_pts[1] = s->all_pts[s->m.indices[3*i+1]];
        cur_pts[2] = s->all_pts[s->m.indices[3*i+2]];

        if (cur_pts[0].depth < 0.0f || cur_pts[1].depth < 0.0f || cur_pts[2].depth < 0.0f)
            continue;

        rasterize_triangle(cur_pts, &s->fb);
    }

clock_t t4 = clock();

    for (int i=0;i<s->fb.w*s->fb.h;i++)
    {
        s->present_buffer[4*i+0] = 255*clampf(s->fb.data[4*i+0], 0.0f, 1.0f);
        s->present_buffer[4*i+1] = 255*clampf(s->fb.data[4*i+1], 0.0f, 1.0f);
        s->present_buffer[4*i+2] = 255*clampf(s->fb.data[4*i+2], 0.0f, 1.0f);
        s->present_buffer[4*i+3] = 255;
    }

clock_t t5 = clock();

    static uint32_t frameId = 0;
    static float average_times[4] = {0,0,0,0}; 

    const float alpha = frameId == 0 ? 0.0f : 0.99f;
    float times[4];
    times[0] = 1000.0f * (t2 - t1) / CLOCKS_PER_SEC;
    times[1] = 1000.0f * (t3 - t2) / CLOCKS_PER_SEC;
    times[2] = 1000.0f * (t4 - t3) / CLOCKS_PER_SEC;
    times[3] = 1000.0f * (t5 - t4) / CLOCKS_PER_SEC;

    for (int i=0;i<4;i++)
        average_times[i] = alpha*average_times[i] + (1-alpha)*times[i];
    frameId++;
    
    if (frameId % 100 == 0)
    {
        printf("clear FB:      %5.2f ms\n", average_times[0]);
        printf("vertex shader: %5.2f ms\n", average_times[1]);
        printf("pixel shader:  %5.2f ms\n", average_times[2]);
        printf("resolve:       %5.2f ms\n\n", average_times[3]);
    }
}

int firstMouse = 1;
float lastX = 0.0f;
float lastY = 0.0f;
float yaw = 0.0f, pitch = 0.0f, roll = 0.0f;
float sensitivity = 0.001f;
void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = 0;
    }
  
    float xoffset = xpos - lastX;
    float yoffset = ypos - lastY; 
    lastX = xpos;
    lastY = ypos;

    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw   += xoffset;
    pitch += yoffset;

    if(pitch > M_PI/2.0f - 0.01f)
        pitch = M_PI/2.0f - 0.01f;
    if(pitch < -(M_PI/2.0f - 0.01f))
        pitch = -(M_PI/2.0f - 0.01f);
} 

void process_input(GLFWwindow *window, Scene *s, float dt)
{
    mat3 rot = rotate_euler3x3_zyx(pitch, yaw, roll);
    s->cam.up = rot.cols[1];
    vec3 cameraFront = cmul3(-1.0f, rot.cols[2]);

    const float cameraSpeed = 1.0f; // adjust accordingly
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        s->cam.pos = add3(s->cam.pos, cmul3(cameraSpeed*dt, cameraFront));
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        s->cam.pos = add3(s->cam.pos, cmul3(-1.0f*cameraSpeed*dt, cameraFront));
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        s->cam.pos = add3(s->cam.pos, cmul3(cameraSpeed*dt, norm3(cross3(cameraFront, s->cam.up))));
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        s->cam.pos = add3(s->cam.pos, cmul3(-1.0f*cameraSpeed*dt, norm3(cross3(cameraFront, s->cam.up))));

    s->cam.lookAt = add3(s->cam.pos, cameraFront);

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, 1);
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        printf("Usage: %s <obj_file> (optional: <width>, <height>)\n", argv[0]);
        return -1;
    }
    const char *filename = argv[1];
    int width  = argc > 2 ? atoi(argv[2]) : 640;
    int height = argc > 3 ? atoi(argv[3]) : 480;
    Scene scene = init_scene(width, height, filename);
    GLFWwindow *window;
    
    if (!glfwInit())
        return -1;
    
    window = glfwCreateWindow(width, height, "Software Renderer", NULL, NULL);
    if (!window) 
    {
        glfwTerminate();
        return -1;
    }
    
    glfwMakeContextCurrent(window);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); 
    glfwSetCursorPosCallback(window, mouse_callback);  
    
    // Main loop
    float dt = 0;
    while (!glfwWindowShouldClose(window)) 
    {
        process_input(window, &scene, dt);

        clock_t begin = clock();
        render_scene(&scene);
        clock_t end = clock();
        double time_spent = (double)(end - begin) / CLOCKS_PER_SEC;   
        dt = time_spent;  
        
        glDrawPixels(width, height, GL_RGBA, GL_UNSIGNED_BYTE, scene.present_buffer);
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    
    glfwTerminate();  

    //save_framebuffer_to_image_RGB(&scene.fb, "saves/test.png");
    free_scene(&scene);
    return 0;
}