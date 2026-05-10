#include "image_utils.h"
#include "vectors.h"
#define TINYOBJ_LOADER_C_IMPLEMENTATION
#include "external/tinyobj_loader_c.h"
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
    int inTri = (areaABP >= 0 && areaBCP >= 0 && areaCAP >= 0) ||
                (areaABP <= 0 && areaBCP <= 0 && areaCAP <= 0);
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
    const vec3 light_dir = norm3(make3(1,1,1));

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

            vec3 albedo = make3(1, 1, 1);
            float q = maxf(0.0f, dot3(n, light_dir))*0.5f + 0.25f;
            vec3 col = cmul3(q, albedo);

            fb->data[(y * fb->w + x) * fb->ch + CHANNEL_R] = col.x;
            fb->data[(y * fb->w + x) * fb->ch + CHANNEL_G] = col.y;
            fb->data[(y * fb->w + x) * fb->ch + CHANNEL_B] = col.z;
            fb->data[(y * fb->w + x) * fb->ch + CHANNEL_DEPTH] = depth;
        }
    }
}

typedef struct
{
    int num_vertices;
    int num_triangles;
    vec3 *verts;
    vec3 *normals;
    vec2 *tcs;
    int *indices;
} mesh;

void free_mesh(mesh *m)
{
    free(m->verts);
    free(m->normals);
    free(m->tcs);
    free(m->indices);
}

#ifdef _WIN64
#define atoll(S) _atoi64(S)
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

static char* mmap_file(size_t* len, const char* filename) {
#ifdef _WIN64
  HANDLE file =
      CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                  FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);

  if (file == INVALID_HANDLE_VALUE) { /* E.g. Model may not have materials. */
    return NULL;
  }

  HANDLE fileMapping = CreateFileMapping(file, NULL, PAGE_READONLY, 0, 0, NULL);
  assert(fileMapping != INVALID_HANDLE_VALUE);

  LPVOID fileMapView = MapViewOfFile(fileMapping, FILE_MAP_READ, 0, 0, 0);
  char* fileMapViewChar = (char*)fileMapView;
  assert(fileMapView != NULL);

  DWORD file_size = GetFileSize(file, NULL);
  (*len) = (size_t)file_size;

  return fileMapViewChar;
#else

  struct stat sb;
  char* p;
  int fd;

  fd = open(filename, O_RDONLY);
  if (fd == -1) {
    perror("open");
    return NULL;
  }

  if (fstat(fd, &sb) == -1) {
    perror("fstat");
    return NULL;
  }

  if (!S_ISREG(sb.st_mode)) {
    fprintf(stderr, "%s is not a file\n", filename);
    return NULL;
  }

  p = (char*)mmap(0, sb.st_size, PROT_READ, MAP_SHARED, fd, 0);

  if (p == MAP_FAILED) {
    perror("mmap");
    return NULL;
  }

  if (close(fd) == -1) {
    perror("close");
    return NULL;
  }

  (*len) = sb.st_size;

  return p;

#endif
}

static void get_file_data(void* ctx, const char* filename, const int is_mtl,
                          const char* obj_filename, char** data, size_t* len) 
                          {
  // NOTE: If you allocate the buffer with malloc(),
  // You can define your own memory management struct and pass it through `ctx`
  // to store the pointer and free memories at clean up stage(when you quit an
  // app)
  // This example uses mmap(), so no free() required.
  (void)ctx;

  if (!filename) {
    fprintf(stderr, "null filename\n");
    (*data) = NULL;
    (*len) = 0;
    return;
  }

  size_t data_len = 0;

  *data = mmap_file(&data_len, filename);
  (*len) = data_len;
}

mesh load_obj(const char *filename)
{
    mesh m;
    m.num_triangles = 0;
    m.num_vertices = 0;

    tinyobj_attrib_t attrib;
    tinyobj_shape_t *shapes = NULL;
    size_t num_shapes;
    tinyobj_material_t *materials = NULL;
    size_t num_materials;

    unsigned int flags = TINYOBJ_FLAG_TRIANGULATE;
    int ret =
        tinyobj_parse_obj(&attrib, &shapes, &num_shapes, &materials,
                          &num_materials, filename, get_file_data, NULL, flags);
    if (ret != TINYOBJ_SUCCESS)
        return m;

    printf("# of shapes    = %d\n", (int)num_shapes);
    printf("# of materials = %d\n", (int)num_materials);
    printf("# of vertices  = %d\n", (int)attrib.num_vertices);
    printf("# of normals   = %d\n", (int)attrib.num_normals);
    printf("# of texcoords = %d\n", (int)attrib.num_texcoords);
    printf("# of faces     = %d\n", (int)attrib.num_faces/3);

    m.verts = malloc(attrib.num_vertices * sizeof(vec3));
    m.normals = malloc(attrib.num_vertices * sizeof(vec3));
    m.tcs = malloc(attrib.num_vertices * sizeof(vec2));
    m.indices = malloc(attrib.num_faces * sizeof(int));
    m.num_vertices = attrib.num_vertices;
    m.num_triangles = attrib.num_faces/3;

    for (int i = 0; i < attrib.num_vertices; i++)
        m.verts[i] = make3(attrib.vertices[3 * i + 0], attrib.vertices[3 * i + 1], attrib.vertices[3 * i + 2]);
    if (attrib.num_normals == attrib.num_vertices)
    {
        for (int i = 0; i < attrib.num_normals; i++)
            m.normals[i] = make3(attrib.normals[3 * i + 0], attrib.normals[3 * i + 1], attrib.normals[3 * i + 2]);
    }
    else
    {
        for (int i = 0; i < attrib.num_vertices; i++)
            m.normals[i] = make3(1.0f, 0.0f, 0.0f);
    }
    if (attrib.num_texcoords == attrib.num_vertices)
    {
        for (int i = 0; i < attrib.num_texcoords; i++)
            m.tcs[i] = make2(attrib.texcoords[2 * i + 0], attrib.texcoords[2 * i + 1]);
    }
    else
    {
        for (int i = 0; i < attrib.num_vertices; i++)
            m.tcs[i] = make2(0.0f, 0.0f);
    }

    for (int i = 0; i < attrib.num_faces; i++)
    {
        m.indices[i] = attrib.faces[i].v_idx;
    }

    tinyobj_attrib_free(&attrib);
    tinyobj_shapes_free(shapes, num_shapes);
    tinyobj_materials_free(materials, num_materials);
    return m;
}

int main(int argc, char **argv)
{
    mesh m = load_obj("/home/sammael/models/Bunny.obj");

    FrameBuffer fb = init_framebuffer(512, 512, 4);
    clear_framebuffer(&fb, 0.0f);

    mat4 view = look_at(make3(2, 0, 2), make3(0, 0, 0), make3(0, 1, 0));
    mat4 proj = perspective(M_PI/6, (float)(fb.w)/(fb.h), 0.01f, 1000.0f);
    mat4 viewProj = mul4x4(proj, view);
    mat4 viewInvTransposed = transpose4(inverse4x4(view));

    RastPoint *all_pts = malloc(m.num_vertices * sizeof(RastPoint));
    RastPoint cur_pts[3];

    clock_t begin = clock();

    for (int i = 0; i < m.num_vertices; i++)
    {
        vec4 pt = vmul4(viewProj, to_vec4(m.verts[i], 1.0f));
        vec3 pt_NDC = make3(pt.x / pt.w, pt.y / pt.w, pt.z / pt.w);

        all_pts[i].depth = pt_NDC.z;
        all_pts[i].screen_pos = make2(0.5f*(pt_NDC.x+1.0f)*fb.w, 0.5f*(pt_NDC.y+1.0f)*fb.h);
        all_pts[i].tc = m.tcs[i];
        all_pts[i].norm = norm3(vmul4v(viewInvTransposed, m.normals[i]));
        //printf("norm %f %f %f --> %f %f %f\n", m.normals[i].x, m.normals[i].y, m.normals[i].z, all_pts[i].norm.x, all_pts[i].norm.y, all_pts[i].norm.z);
    }

    for (int i = 0; i < m.num_triangles; i++)
    {
        cur_pts[0] = all_pts[m.indices[3*i+0]];
        cur_pts[1] = all_pts[m.indices[3*i+1]];
        cur_pts[2] = all_pts[m.indices[3*i+2]];

        rasterize_triangle(cur_pts, &fb);
    }

    clock_t end = clock();
    double time_spent = 1000.0 * (double)(end - begin) / CLOCKS_PER_SEC;    

    printf("Time: %.1f ms\n", time_spent);    

    save_framebuffer_to_image_RGB(&fb, "saves/test.png");

    free(all_pts);
    free_framebuffer(&fb);
    free_mesh(&m);
    return 0;
}