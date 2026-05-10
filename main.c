#include "image_utils.h"
#include "vectors.h"
#include <stdio.h>

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

    return 0;
}