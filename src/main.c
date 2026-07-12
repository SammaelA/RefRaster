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
// 5) Perlin + terrain normals/textures/movement: ~3 hours
// 6) Cubemap: ~1 hour

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
    Texture_F32 grass;
    Texture_F32 rocky_grass;
    float terrain_area_size;
    float terrain_tile_size_inv;

    mesh cubemap_mesh;
    Texture_F32 cubemap_tex[6];

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

VertexOut cubemap_VS(uint32_t inst_id, uint32_t v_id, const mesh *m, const void *scene_ctx)
{
    const Scene *s = (const Scene *)scene_ctx;

    VertexOut v;
    v.clip_pos = vmul4(s->viewProj, to_vec4(add3(cmul3(100.0f, m->verts[v_id]), s->cam.pos), 1.0f));
    v.tc = m->tcs[v_id];
    v.norm = norm3(vmul4v(s->viewInvTransposed, m->normals[v_id]));
    return v;
}

vec4 cubemap_PS(const Fragment *f, const void *scene_ctx)
{
    const Scene *s = (const Scene *)scene_ctx;
    const vec3 col = sample_f32_rgb(&(s->cubemap_tex[f->frag_id/2]), f->tc);
    return to_vec4(col, 1.0f);
}

VertexOut terrain_VS(uint32_t inst_id, uint32_t v_id, const mesh *m, const void *scene_ctx)
{
    const Scene *s = (const Scene *)scene_ctx;

    vec3 v_pos = add3(m->verts[v_id], make3(floorf(s->cam.pos.x), 0, floorf(s->cam.pos.z)));
    const float t_sz = s->terrain_area_size;
    const vec2 v_tc = make2(0.5f*(v_pos.x+t_sz)/t_sz, 0.5f*(v_pos.z+t_sz)/t_sz);

    vec2 dxy;
    const vec4 h_quad = gather_f32(&(s->heightmap), v_tc, 0, &dxy);

    v_pos.y = (1-dxy.x)*(1-dxy.y)*h_quad.x + 
                  dxy.x*(1-dxy.y)*h_quad.y + 
                  (1-dxy.x)*dxy.y*h_quad.z + 
                      dxy.x*dxy.y*h_quad.w;

    const float dh_dx = (1-dxy.y)*(h_quad.y - h_quad.x) + dxy.y*(h_quad.w - h_quad.z);
    const float dh_dy = (1-dxy.x)*(h_quad.z - h_quad.x) + dxy.x*(h_quad.w - h_quad.y);
    const vec3 v_norm = norm3(make3(-dh_dx, 1.0f, -dh_dy));

    VertexOut v;
    v.clip_pos = vmul4(s->viewProj, to_vec4(v_pos, 1.0f));
    v.tc = make2(v_pos.x, v_pos.z);
    v.norm = norm3(vmul4v(s->viewInvTransposed, v_norm));
    return v;
}

vec4 terrain_PS(const Fragment *f, const void *scene_ctx)   
{
    const Scene *s = (const Scene *)scene_ctx;

    const vec2 tc = make2(2.0f*absf(fractf(s->terrain_tile_size_inv*f->tc.x)-0.5f), 
                          2.0f*absf(fractf(s->terrain_tile_size_inv*f->tc.y)-0.5f));

    const vec3 nw = vmul4v(s->viewInvTransposedInv, f->norm);
    const float slope_q = clampf(3.0f*sqrtf(sqrtf(nw.x*nw.x + nw.z*nw.z) / nw.y), 0.0f, 1.0f);
    const vec3 albedo_grass = slope_q < 0.99f ? sample_f32_rgb(&(s->grass), tc) : make3(0.0f, 1.0f, 0.0f);
    const vec3 albedo_rocky = slope_q > 0.01f ? sample_f32_rgb(&(s->rocky_grass), tc) : albedo_grass;
    const vec3 albedo = lerp3(slope_q, albedo_grass, albedo_rocky);
    float q = maxf(0.0f, dot3(f->norm, s->light_dir))*s->light_intensity + s->ambient_light_intensity;
    const vec3 col = cmul3(q, albedo);

    return to_vec4(col, 1.0f);
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
    generate_perlin(hm.data, W, W, 0.005f, 5, 12345u);

    for (int i=0;i<W*W;i++)
        hm.data[i] *= amplitude;

    return hm;
}

#define CUBEMAP_FRONT  0
#define CUBEMAP_BACK   1
#define CUBEMAP_LEFT   2
#define CUBEMAP_RIGHT  3
#define CUBEMAP_BOTTOM 4
#define CUBEMAP_TOP    5

mesh create_cubemap_mesh()
{
    #define SET_VERT(i, j, x, y, z, u, v) do { m.verts[4*i+j] = make3(x, y, z); m.normals[4*i+j] = make3(1,0,0); m.tcs[4*i+j] = make2(u, v); } while (0)
    mesh m = init_empty_mesh(24, 12);

    SET_VERT(CUBEMAP_FRONT,0, -1,-1,-1, 0,0);
    SET_VERT(CUBEMAP_FRONT,1,  1,-1,-1, 1,0);
    SET_VERT(CUBEMAP_FRONT,2, -1, 1,-1, 0,1);
    SET_VERT(CUBEMAP_FRONT,3,  1, 1,-1, 1,1);

    SET_VERT(CUBEMAP_BACK,0, -1,-1, 1, 1,0);
    SET_VERT(CUBEMAP_BACK,2,  1,-1, 1, 0,0);
    SET_VERT(CUBEMAP_BACK,1, -1, 1, 1, 1,1);
    SET_VERT(CUBEMAP_BACK,3,  1, 1, 1, 0,1);

    SET_VERT(CUBEMAP_LEFT,0, -1,-1, 1, 0,0);
    SET_VERT(CUBEMAP_LEFT,1, -1,-1,-1, 1,0);
    SET_VERT(CUBEMAP_LEFT,2, -1, 1, 1, 0,1);
    SET_VERT(CUBEMAP_LEFT,3, -1, 1,-1, 1,1);

    SET_VERT(CUBEMAP_RIGHT,0,  1,-1, 1, 1,0);
    SET_VERT(CUBEMAP_RIGHT,2,  1,-1,-1, 0,0);
    SET_VERT(CUBEMAP_RIGHT,1,  1, 1, 1, 1,1);
    SET_VERT(CUBEMAP_RIGHT,3,  1, 1,-1, 0,1);

    SET_VERT(CUBEMAP_BOTTOM,0, -1,-1, 1, 0,1);
    SET_VERT(CUBEMAP_BOTTOM,1,  1,-1, 1, 0,0);
    SET_VERT(CUBEMAP_BOTTOM,2, -1,-1,-1, 1,1);
    SET_VERT(CUBEMAP_BOTTOM,3,  1,-1,-1, 1,0);

    SET_VERT(CUBEMAP_TOP,0, -1, 1, 1, 0,1);
    SET_VERT(CUBEMAP_TOP,2,  1, 1, 1, 1,1);
    SET_VERT(CUBEMAP_TOP,1, -1, 1,-1, 0,0);
    SET_VERT(CUBEMAP_TOP,3,  1, 1,-1, 1,0);

    for (int i=0;i<6;i++)
    {
        m.indices[6*i + 0] = i*4 + 0;
        m.indices[6*i + 1] = i*4 + 1;
        m.indices[6*i + 2] = i*4 + 3;
        m.indices[6*i + 3] = i*4 + 0;
        m.indices[6*i + 4] = i*4 + 3;
        m.indices[6*i + 5] = i*4 + 2;
    }

    return m;
}

Texture_F32 load_tex_gm(const char *filename, float gamma)
{
    Texture_F32 tex;
    tex.data = load_image_f32_rgb(filename, &(tex.w), &(tex.h), gamma);
    tex.ch = 3;
    if (!tex.data)
        printf("Failed to load texture: %s\n", filename);
    return tex;
}

Texture_F32 load_tex(const char *filename)
{
    return load_tex_gm(filename, 2.2f);
}

Scene init_scene(int width, int height, const char *filename)
{
    Scene s;

    s.cam.fovy = M_PI/6;
    s.cam.pos = make3(0,3,3);
    s.cam.lookAt = make3(0, 0, 0);
    s.cam.up = make3(0, 1, 0);
    s.cam.z_near = 0.01f;
    s.cam.z_far = 1000.0f;
    s.cam.aspect = (float)width/(float)height;

    s.m = load_obj(filename);
    s.tex = load_tex("resources/textures/porcelain.jpg");

    s.light_dir = norm3(make3(1,1,1));
    s.light_intensity = 1.0f;
    s.ambient_light_intensity = 0.33f;

    s.fb = SGL_init_framebuffer(width, height, 4);
    s.sgl_ctx = SGL_init_internal_ctx();

    s.grass = load_tex("resources/textures/Grass_09-256x256.png");
    s.rocky_grass = load_tex("resources/textures/Grass_11-256x256.png");
    s.heightmap = create_heightmap(1024, 5.0f);
    s.terrain_mesh = create_terrain_mesh(100, 10.0f);
    s.terrain_area_size = 50;
    s.terrain_tile_size_inv = 0.5f;
    //save_image_f32_png_rgb(s.heightmap.data, "saves/heightmap.png", s.heightmap.w, s.heightmap.h, s.heightmap.ch, 1.0f);

    s.cubemap_mesh = create_cubemap_mesh();
    s.cubemap_tex[CUBEMAP_FRONT]  = load_tex_gm("resources/textures/skybox/hills_ft.png", 1.0f);
    s.cubemap_tex[CUBEMAP_BACK]   = load_tex_gm("resources/textures/skybox/hills_bk.png", 1.0f);
    s.cubemap_tex[CUBEMAP_LEFT]   = load_tex_gm("resources/textures/skybox/hills_rt.png", 1.0f);
    s.cubemap_tex[CUBEMAP_RIGHT]  = load_tex_gm("resources/textures/skybox/hills_lf.png", 1.0f);
    s.cubemap_tex[CUBEMAP_BOTTOM] = load_tex_gm("resources/textures/skybox/hills_dn.png", 1.0f);
    s.cubemap_tex[CUBEMAP_TOP]    = load_tex_gm("resources/textures/skybox/hills_up.jpg", 1.0f);

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

    // p.ps = lambert_PS;
    // p.vs = default_VS;
    // SGL_draw_instances(&(s->m), 1, &p);

    p.ps = terrain_PS;
    p.vs = terrain_VS;
    SGL_draw_instances(&(s->terrain_mesh), 1, &p);

    p.ps = cubemap_PS;
    p.vs = cubemap_VS;
    SGL_draw_instances(&(s->cubemap_mesh), 1, &p);

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
float cameraSpeed = 1.0f;
float terrainCameraHeight = 0.1f;
float freeCameraSpeedMult = 10.0f;
int freeCamera = 1;
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

    const float cs = (freeCamera ? freeCameraSpeedMult : 1.0f) * cameraSpeed * dt;

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        s->cam.pos = add3(s->cam.pos, cmul3( cs, cameraFront));
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        s->cam.pos = add3(s->cam.pos, cmul3(-cs, cameraFront));
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        s->cam.pos = add3(s->cam.pos, cmul3( cs, norm3(cross3(cameraFront, s->cam.up))));
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        s->cam.pos = add3(s->cam.pos, cmul3(-cs, norm3(cross3(cameraFront, s->cam.up))));

    s->cam.lookAt = add3(s->cam.pos, cameraFront);

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, 1);
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_C && action == GLFW_PRESS)
        freeCamera = freeCamera ? 0 : 1;
}

void on_terrain_set_camera(Scene *s)
{
    const float t_sz = s->terrain_area_size;
    const vec2 v_tc = make2(0.5f*(s->cam.pos.x+t_sz)/t_sz, 0.5f*(s->cam.pos.z+t_sz)/t_sz);

    const float h = sample_f32_rgb(&(s->heightmap), v_tc).x;

    s->cam.pos = make3(s->cam.pos.x, h+terrainCameraHeight, s->cam.pos.z);
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
    glfwSetKeyCallback(window, key_callback);
    
    // Main loop
    float dt = 0;
    while (!glfwWindowShouldClose(window)) 
    {
        process_input(window, &scene, dt);

        if (freeCamera == 0)
            on_terrain_set_camera(&scene);

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