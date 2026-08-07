#include "mesh_utils.h"

#define TINYOBJ_LOADER_C_IMPLEMENTATION
#include "external/tinyobj_loader_c.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN64
#define atoll(S) _atoi64(S)
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>
#endif

void free_mesh(mesh *m)
{
    free(m->verts);
    free(m->normals);
    free(m->tcs);
    free(m->indices);
}

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
		
		//recalculating normals
		vec3 *norm_sum = malloc(attrib.num_vertices * sizeof(vec3));
		uint32_t *norm_count = malloc(attrib.num_vertices * sizeof(uint32_t));
		for (int i = 0; i < attrib.num_vertices; i++)
		{
			norm_sum[i] = make3(0.0f, 0.0f, 0.0f);
			norm_count[i] = 0;	
		}

		for (int i = 0; i < attrib.num_faces; i+=3)
		{
			const int idx_A = attrib.faces[i+0].v_idx;
			const int idx_B = attrib.faces[i+1].v_idx;
			const int idx_C = attrib.faces[i+2].v_idx;
			const vec3 A = m.verts[idx_A];
			const vec3 B = m.verts[idx_B];
			const vec3 C = m.verts[idx_C];
			const vec3 norm = norm3(cross3(sub3(B, A), sub3(C, A)));

			norm_sum[idx_A] = add3(norm_sum[idx_A], norm);
			norm_sum[idx_B] = add3(norm_sum[idx_B], norm);
			norm_sum[idx_C] = add3(norm_sum[idx_C], norm);

			norm_count[idx_A]++;
			norm_count[idx_B]++;
			norm_count[idx_C]++;
		}

		for (int i = 0; i < attrib.num_vertices; i++)
		{
			if (norm_count[i] > 0)
				m.normals[i] = norm3(norm_sum[i]);
			else
				m.normals[i] = make3(1.0f, 0.0f, 0.0f);
		}

		free(norm_sum);
		free(norm_count);
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