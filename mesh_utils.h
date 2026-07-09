#ifndef MESH_UTILS_H
#define MESH_UTILS_H
#include "vectors.h"

typedef struct
{
    int num_vertices;
    int num_triangles;
    vec3 *verts;
    vec3 *normals;
    vec2 *tcs;
    int *indices;
} mesh;

void free_mesh(mesh *m);
mesh load_obj(const char *filename);

#endif