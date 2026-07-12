#include "image_utils.h"
#include "vectors.h"
#include "mesh_utils.h"
#include "SGL.h"
#include "perlin_noise.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <GLFW/glfw3.h>

// Work time sheet
// 1) Basic render (matrix ops + obj loading + camera + window): ~5 hours
// 2) Refactoring to some king of GL interface: ~2 hours
// 3) Textures: ~1 hour
// 4) Bug fixes + basic terrain: ~3 hours

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

void save_framebuffer_to_image_RGB(const Texture_F32 *fb, const char *filename)
{
    save_image_f32_png_rgb(fb->data, filename, fb->w, fb->h, fb->ch, 1.0f);
}

typedef struct
{
    SGL_Camera cam;
    mat4 view, proj, viewProj;
    mat4 viewInvTransposed, viewInvTransposedInv;

    mesh m;
    Texture_F32 tex;

    mesh terrain_mesh;
    Texture_F32 heightmap;
    float terrain_area_size;

    Texture_F32 fb;
    Texture_U8 present_buffer;

    vec3  light_dir;
    float light_intensity;
    float ambient_light_intensity;

    void *sgl_ctx;

} Scene;

VertexOut default_VS(uint32_t inst_id, uint32_t v_id, const mesh *m, const void *scene_ctx)
{
    const Scene *s = (const Scene *)scene_ctx;

    VertexOut v;
    v.clip_pos = vmul4(s->viewProj, to_vec4(m->verts[v_id], 1.0f));
    v.tc = m->tcs[v_id];
    v.norm = norm3(vmul4v(s->viewInvTransposed, m->normals[v_id]));
    return v;
}

VertexOut terrain_VS(uint32_t inst_id, uint32_t v_id, const mesh *m, const void *scene_ctx)
{
    const Scene *s = (const Scene *)scene_ctx;

    vec3 v_pos = add3(m->verts[v_id], make3(s->cam.pos.x, 0, s->cam.pos.z));
    const float t_sz = s->terrain_area_size;
    const vec2 v_tc = make2(0.5f*(v_pos.x+t_sz)/t_sz, 0.5f*(v_pos.z+t_sz)/t_sz);
    v_pos.y = sample_f32_rgb(&(s->heightmap), v_tc).x;

    VertexOut v;
    v.clip_pos = vmul4(s->viewProj, to_vec4(v_pos, 1.0f));
    v.tc = m->tcs[v_id];
    v.norm = norm3(vmul4v(s->viewInvTransposed, make3(0, 1, 0))); //TODO: terrain normals
    return v;
}

vec4 lambert_PS(const Fragment *f, const void *scene_ctx)
{
    const Scene *s = (const Scene *)scene_ctx;

    const vec3 albedo = sample_f32_rgb(&(s->tex), f->tc);
    float q = maxf(0.0f, dot3(f->norm, s->light_dir))*s->light_intensity + s->ambient_light_intensity;
    const vec3 col = cmul3(q, albedo);

    return to_vec4(col, 1.0f);
}

vec4 vis_normal_PS(const Fragment *f, const void *scene_ctx)
{
    const Scene *s = (const Scene *)scene_ctx;
    const vec3 n_world = vmul4v(s->viewInvTransposedInv, f->norm);
    return to_vec4(add3(make3(0.25f, 0.25f, 0.25f), cmul3(0.5f, pow3(abs3(n_world), 1.0f/1.5f))), 1.0f);
}

vec4 lambert_no_tex_PS(const Fragment *f, const void *scene_ctx)
{
    const Scene *s = (const Scene *)scene_ctx;

    const vec3 albedo = make3(0.5, 0.5, 0.5);
    float q = maxf(0.0f, dot3(f->norm, s->light_dir))*s->light_intensity + s->ambient_light_intensity;
    const vec3 col = cmul3(q, albedo);

    return to_vec4(col, 1.0f);
}

vec4 vis_tc_PS(const Fragment *f, const void *scene_ctx)
{
    return make4(f->tc.x, f->tc.y, 0.0f, 1.0f);
}

mesh init_empty_mesh(int n_vert, int n_tri)
{
    mesh m;
    m.num_vertices = n_vert;
    m.num_triangles = n_tri;
    m.verts = malloc(m.num_vertices*sizeof(vec3));
    m.normals = malloc(m.num_vertices*sizeof(vec3));
    m.tcs = malloc(m.num_vertices*sizeof(vec2));
    m.indices = malloc(3*m.num_triangles*sizeof(int));  

    return m;
}

mesh create_terrain_mesh(int N, float size)
{
    mesh m = init_empty_mesh((N+1)*(N+1), 2*N*N);

    //vertices
    for (int i=0;i<=N;i++)
    {
        for (int j=0;j<=N;j++)
        {
            const uint32_t idx = i*(N+1) + j;
            vec2 rp = make2((float)j/N, (float)i/N);
            
            m.tcs[idx] = rp;
            m.verts[idx] = make3(size*(2.0f*rp.y-1.0f), 0.0f, size*(2.0f*rp.x-1.0f));
            m.normals[idx] = make3(0,1,0);
        }
    }

    //triangles
    for (int i=0;i<N;i++)
    {
        for (int j=0;j<N;j++)
        {
            m.indices[6*(i*N+j) + 0] = (i+0)*(N+1) + (j+0);
            m.indices[6*(i*N+j) + 1] = (i+0)*(N+1) + (j+1);
            m.indices[6*(i*N+j) + 2] = (i+1)*(N+1) + (j+1);

            m.indices[6*(i*N+j) + 3] = (i+0)*(N+1) + (j+0);
            m.indices[6*(i*N+j) + 4] = (i+1)*(N+1) + (j+1);
            m.indices[6*(i*N+j) + 5] = (i+1)*(N+1) + (j+0);
        }
    }

    return m;
}

Texture_F32 create_heightmap(int W, float amplitude)
{
    Texture_F32 hm;
    hm.w = W;
    hm.h = W;
    hm.ch = 1;
    hm.data = malloc(W*W*sizeof(float));
    generate_perlin(hm.data, W, W, 0.025f, 12345);

    for (int i=0;i<W*W;i++)
        hm.data[i] *= amplitude;

    return hm;
}

Scene init_scene(int width, int height, const char *filename)
{
    Scene s;

    s.cam.fovy = M_PI/6;
    s.cam.pos = make3(0,0.5,3);
    s.cam.lookAt = make3(0, 0, 0);
    s.cam.up = make3(0, 1, 0);
    s.cam.z_near = 0.01f;
    s.cam.z_far = 1000.0f;
    s.cam.aspect = (float)width/(float)height;

    s.m = load_obj(filename);

    s.tex.data = load_image_f32_rgb("resources/porcelain.jpg", &(s.tex.w), &(s.tex.h), 2.2f);
    s.tex.ch = 3;
    if (s.tex.data == NULL)
        printf("failed to load texture\n");

    s.light_dir = norm3(make3(1,1,1));
    s.light_intensity = 1.0f;
    s.ambient_light_intensity = 0.33f;

    s.fb = SGL_init_framebuffer(width, height, 4);
    s.sgl_ctx = SGL_init_internal_ctx();

    s.heightmap = create_heightmap(512, 1.0f);
    s.terrain_mesh = create_terrain_mesh(64, 10.0f);
    s.terrain_area_size = 50;
    save_image_f32_png_rgb(s.heightmap.data, "saves/heightmap.png", s.heightmap.w, s.heightmap.h, s.heightmap.ch, 2.2f);

    return s;
}

void free_scene(Scene *s)
{
    free_mesh(&s->m);
    free(s->tex.data);
    SGL_free_framebuffer(&s->fb);
    free(s->present_buffer.data);
    SGL_free_internal_ctx(s->sgl_ctx);

    free(s->heightmap.data);
    free_mesh(&s->terrain_mesh);
}

void render_scene(Scene *s)
{
clock_t t1 = clock();

    s->view = look_at(s->cam.pos, s->cam.lookAt, s->cam.up);
    s->proj = perspective(s->cam.fovy, s->cam.aspect, s->cam.z_near, s->cam.z_far);
    s->viewProj = mul4x4(s->proj, s->view);
    s->viewInvTransposed = transpose4(inverse4x4(s->view));
    s->viewInvTransposedInv = inverse4x4(s->viewInvTransposed);

    SGL_clear_framebuffer(&s->fb, 0.0f);

clock_t t2 = clock();

    SGL_Pipeline p;
    p.cam = s->cam;
    p.fb =  s->fb;
    p.scene_ctx = (void *)s;
    p.internal_ctx = s->sgl_ctx;

    p.ps = lambert_PS;
    p.vs = default_VS;
    SGL_draw_instances(&(s->m), 1, &p);

    p.ps = vis_normal_PS;
    p.vs = terrain_VS;
    SGL_draw_instances(&(s->terrain_mesh), 1, &p);

clock_t t3 = clock();

    SGL_resolve_simple(s->fb, s->present_buffer);

clock_t t4 = clock();

    static uint32_t frameId = 0;
    static float average_times[3] = {0,0,0}; 

    const float alpha = frameId == 0 ? 0.0f : 0.99f;
    float times[3];
    times[0] = 1000.0f * (t2 - t1) / CLOCKS_PER_SEC;
    times[1] = 1000.0f * (t3 - t2) / CLOCKS_PER_SEC;
    times[2] = 1000.0f * (t4 - t3) / CLOCKS_PER_SEC;

    for (int i=0;i<3;i++)
        average_times[i] = alpha*average_times[i] + (1-alpha)*times[i];
    frameId++;
    
    if (frameId % 100 == 0)
    {
        printf("clear FB:   %5.2f ms\n", average_times[0]);
        printf("Draw:       %5.2f ms\n", average_times[1]);
        printf("resolve:    %5.2f ms\n\n", average_times[2]);
    }
}

int firstMouse = 1;
float lastX = 0.0f;
float lastY = 0.0f;
float yaw = 0.0f, pitch = 0.0f, roll = 0.0f;
float sensitivity = 0.001f;
float cameraSpeed = 10.0f;
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

    // Negated so mouse-right looks right and mouse-up looks up with the corrected
    // (non-transposed) rotation matrix.
    yaw   -= xoffset;
    pitch -= yoffset;

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
    int fb_width = argc > 4 ? atoi(argv[4]) : 640;
    int fb_height = argc > 5 ? atoi(argv[5]) : 480;
    Scene scene = init_scene(fb_width, fb_height, filename);
    scene.present_buffer.data = malloc(4 * width * height);
    scene.present_buffer.w = width;
    scene.present_buffer.h = height;
    scene.present_buffer.ch = 4;

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
        
        glDrawPixels(width, height, GL_RGBA, GL_UNSIGNED_BYTE, scene.present_buffer.data);
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    
    glfwTerminate();  

    //save_framebuffer_to_image_RGB(&scene.fb, "saves/test.png");
    free_scene(&scene);
    return 0;
}