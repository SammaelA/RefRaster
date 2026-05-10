#ifndef VECTORS_H
#define VECTORS_H
#include <math.h>

#define M_PI 3.14159265358979323846

static float minf(float a, float b) { return a < b ? a : b; }
static float maxf(float a, float b) { return a > b ? a : b; }
static float clampf(float x, float min, float max) { return minf(maxf(x, min), max); }

typedef struct
{
    union
    {
        struct
        {
            float x;
            float y;
        };
        float M[2];
    };
} vec2;
typedef struct
{
    union
    {
        struct
        {
            float x;
            float y;
            float z;
        };
        float M[3];
    };
} vec3;
typedef struct
{
    union
    {
        struct
        {
            float x;
            float y;
            float z;
            float w;
        };
        float M[4];
    };
} vec4;

typedef struct
{
    vec3 cols[3];
} mat3;

typedef struct
{
    vec4 cols[4];
} mat4;

static inline vec2 make_zero2()
{
    vec2 v;
    v.x = 0.0f;
    v.y = 0.0f;
    return v;
}
static inline vec2 make2(float x, float y)
{
    vec2 v;
    v.x = x;
    v.y = y;
    return v;
}
static inline vec2 add2(vec2 a, vec2 b)
{
    vec2 v;
    v.x = a.x + b.x;
    v.y = a.y + b.y;
    return v;
}
static inline vec2 sub2(vec2 a, vec2 b)
{
    vec2 v;
    v.x = a.x - b.x;
    v.y = a.y - b.y;
    return v;
}
static inline vec2 mul2(vec2 a, vec2 b)
{
    vec2 v;
    v.x = a.x * b.x;
    v.y = a.y * b.y;
    return v;
}
static inline vec2 div2(vec2 a, vec2 b)
{
    vec2 v;
    v.x = a.x / b.x;
    v.y = a.y / b.y;
    return v;
}
static inline vec2 cmul2(float c, vec2 v)
{
    vec2 w;
    w.x = c * v.x;
    w.y = c * v.y;
    return w;
}
static inline float len2(vec2 v)
{
    return sqrtf(v.x * v.x + v.y * v.y);
}
static inline vec2 norm2(vec2 v)
{
    vec2 w;
    float l = len2(v);
    w.x = v.x / l;
    w.y = v.y / l;
    return w;
}
static inline float dot2(vec2 a, vec2 b)
{
    return a.x * b.x + a.y * b.y;
}

static inline vec3 make_zero3()
{
    vec3 v;
    v.x = 0.0f;
    v.y = 0.0f;
    v.z = 0.0f;
    return v;
}
static inline vec3 make3(float x, float y, float z)
{
    vec3 v;
    v.x = x;
    v.y = y;
    v.z = z;
    return v;
}
static inline vec3 to_vec3(vec4 v)
{
    vec3 w;
    w.x = v.x;
    w.y = v.y;
    w.z = v.z;
    return w;
}
static inline vec3 add3(vec3 a, vec3 b)
{
    vec3 v;
    v.x = a.x + b.x;
    v.y = a.y + b.y;
    v.z = a.z + b.z;
    return v;
}
static inline vec3 sub3(vec3 a, vec3 b)
{
    vec3 v;
    v.x = a.x - b.x;
    v.y = a.y - b.y;
    v.z = a.z - b.z;
    return v;
}
static inline vec3 mul3(vec3 a, vec3 b)
{
    vec3 v;
    v.x = a.x * b.x;
    v.y = a.y * b.y;
    v.z = a.z * b.z;
    return v;
}
static inline vec3 div3(vec3 a, vec3 b)
{
    vec3 v;
    v.x = a.x / b.x;
    v.y = a.y / b.y;
    v.z = a.z / b.z;
    return v;
}
static inline vec3 cmul3(float c, vec3 v)
{
    vec3 w;
    w.x = c * v.x;
    w.y = c * v.y;
    w.z = c * v.z;
    return w;
}
static inline vec3 cross3(vec3 a, vec3 b)
{
    vec3 v;
    v.x = a.y * b.z - a.z * b.y;
    v.y = a.z * b.x - a.x * b.z;
    v.z = a.x * b.y - a.y * b.x;
    return v;
}
static inline float len3(vec3 v)
{
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}
static inline vec3 norm3(vec3 v)
{
    vec3 w;
    float l = len3(v);
    w.x = v.x / l;
    w.y = v.y / l;
    w.z = v.z / l;
    return w;
}
static inline float dot3(vec3 a, vec3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static inline vec4 make_zero4()
{
    vec4 v;
    v.x = 0.0f;
    v.y = 0.0f;
    v.z = 0.0f;
    v.w = 0.0f;
    return v;
}
static inline vec4 make4(float x, float y, float z, float w)
{
    vec4 v;
    v.x = x;
    v.y = y;
    v.z = z;
    v.w = w;
    return v;
}
static inline vec4 to_vec4(vec3 v, float w)
{
    vec4 res;
    res.x = v.x;
    res.y = v.y;
    res.z = v.z;
    res.w = w;
    return res;
}
static inline vec4 add4(vec4 a, vec4 b)
{
    vec4 v;
    v.x = a.x + b.x;
    v.y = a.y + b.y;
    v.z = a.z + b.z;
    v.w = a.w + b.w;
    return v;
}
static inline vec4 sub4(vec4 a, vec4 b)
{
    vec4 v;
    v.x = a.x - b.x;
    v.y = a.y - b.y;
    v.z = a.z - b.z;
    v.w = a.w - b.w;
    return v;
}
static inline vec4 mul4(vec4 a, vec4 b)
{
    vec4 v;
    v.x = a.x * b.x;
    v.y = a.y * b.y;
    v.z = a.z * b.z;
    v.w = a.w * b.w;
    return v;
}
static inline vec4 div4(vec4 a, vec4 b)
{
    vec4 v;
    v.x = a.x / b.x;
    v.y = a.y / b.y;
    v.z = a.z / b.z;
    v.w = a.w / b.w;
    return v;
}
static inline vec4 cmul4(float c, vec4 v)
{
    vec4 w;
    w.x = c * v.x;
    w.y = c * v.y;
    w.z = c * v.z;
    w.w = c * v.w;
    return w;
}
static inline float len4(vec4 v)
{
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w);
}
static inline vec4 norm4(vec4 v)
{
    vec4 w;
    float l = len4(v);
    w.x = v.x / l;
    w.y = v.y / l;
    w.z = v.z / l;
    w.w = v.w / l;
    return w;
}

static inline mat3 make_ident3x3()
{
    mat3 m;
    m.cols[0] = make3(1.0f, 0.0f, 0.0f);
    m.cols[1] = make3(0.0f, 1.0f, 0.0f);
    m.cols[2] = make3(0.0f, 0.0f, 1.0f);
    return m;
}
static inline mat3 make3x3_cols(vec3 a, vec3 b, vec3 c)
{
    mat3 m;
    m.cols[0] = a;
    m.cols[1] = b;
    m.cols[2] = c;
    return m;
}
static inline mat3 make3x3_rows(vec3 a, vec3 b, vec3 c)
{
    mat3 m;
    m.cols[0] = make3(a.x, b.x, c.x);
    m.cols[1] = make3(a.y, b.y, c.y);
    m.cols[2] = make3(a.z, b.z, c.z);
    return m;
}
static inline vec3 get_row3(mat3 m, int row)
{
    return make3(m.cols[0].M[row], m.cols[1].M[row], m.cols[2].M[row]);
}
// ZYX convention: R = Rz(c) * Ry(b) * Rx(a)
static inline mat3 rotate_euler3x3_zyx(float x, float y, float z)
{
    // Matrix form:
    // [cb*cc,  sa*sb*cc - ca*sc,  ca*sb*cc + sa*sc]
    // [cb*sc,  sa*sb*sc + ca*cc,  ca*sb*sc - sa*cc]
    // [  -sb,            sa*cb,            ca*cb]
    //
    // Where: sa = sin(a), ca = cos(a), etc.

    float cx, cy, cz, sx, sy, sz;
    cx = cosf(x);
    cy = cosf(y);
    cz = cosf(z);
    sx = sinf(x);
    sy = sinf(y);
    sz = sinf(z);
    // sincosf(x, &sx, &cx);
    // sincosf(y, &sy, &cy);
    // sincosf(z, &sz, &cz);

    mat3 m;
    m.cols[0] = make3(cy * cz, sx * sy * cz - cx * sz, cx * sy * cz + sx * sz);
    m.cols[1] = make3(cy * sz, sx * sy * sz + cx * cz, cx * sy * sz - sx * cz);
    m.cols[2] = make3(-sy, sx * cy, cx * cy);
    return m;
}
static inline mat3 scale3x3(float s)
{
    mat3 m;
    m.cols[0] = make3(s, 0.0f, 0.0f);
    m.cols[1] = make3(0.0f, s, 0.0f);
    m.cols[2] = make3(0.0f, 0.0f, s);
    return m;
}
static inline mat3 diag3x3(vec3 v)
{
    mat3 m;
    m.cols[0] = make3(v.x, 0.0f, 0.0f);
    m.cols[1] = make3(0.0f, v.y, 0.0f);
    m.cols[2] = make3(0.0f, 0.0f, v.z);
    return m;
}
static inline mat3 mul3x3(mat3 a, mat3 b)
{
    mat3 m;
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            m.cols[i].M[j] = a.cols[i].M[0] * b.cols[0].M[j] +
                             a.cols[i].M[1] * b.cols[1].M[j] +
                             a.cols[i].M[2] * b.cols[2].M[j];
        }
    }
    return m;
}
static inline mat3 cmul3x3(float c, mat3 m)
{
    mat3 r = m;
    cmul3(c, r.cols[0]);
    cmul3(c, r.cols[1]);
    cmul3(c, r.cols[2]);
    return r;
}
static inline vec3 vmul3(mat3 m, vec3 v)
{
    vec3 res;
    res.x = m.cols[0].x * v.x + m.cols[1].x * v.y + m.cols[2].x * v.z;
    res.y = m.cols[0].y * v.x + m.cols[1].y * v.y + m.cols[2].y * v.z;
    res.z = m.cols[0].z * v.x + m.cols[1].z * v.y + m.cols[2].z * v.z;
    return res;
}
static inline mat3 transpose3x3(mat3 m)
{
    mat3 t;
    t.cols[0] = get_row3(m, 0);
    t.cols[1] = get_row3(m, 1);
    t.cols[2] = get_row3(m, 2);
    return t;
}
static inline float det3x3(mat3 m)
{
    const float a = m.cols[0].x;
    const float b = m.cols[1].x;
    const float c = m.cols[2].x;
    const float d = m.cols[0].y;
    const float e = m.cols[1].y;
    const float f = m.cols[2].y;
    const float g = m.cols[0].z;
    const float h = m.cols[1].z;
    const float i = m.cols[2].z;
    return a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
}
static inline mat3 inverse3x3(mat3 m)
{
    float det = det3x3(m);
    float inv_det = 1.0f / det;
    float a = m.cols[0].x;
    float b = m.cols[1].x;
    float c = m.cols[2].x;
    float d = m.cols[0].y;
    float e = m.cols[1].y;
    float f = m.cols[2].y;
    float g = m.cols[0].z;
    float h = m.cols[1].z;
    float i = m.cols[2].z;

    mat3 inv;
    inv.cols[0].x = (e * i - f * h) * inv_det;
    inv.cols[1].x = (c * h - b * i) * inv_det;
    inv.cols[2].x = (b * f - c * e) * inv_det;
    inv.cols[0].y = (f * g - d * i) * inv_det;
    inv.cols[1].y = (a * i - c * g) * inv_det;
    inv.cols[2].y = (c * d - a * f) * inv_det;
    inv.cols[0].z = (d * h - e * g) * inv_det;
    inv.cols[1].z = (b * g - a * h) * inv_det;
    inv.cols[2].z = (a * e - b * d) * inv_det;

    return inv;
}

static inline mat4 make_ident4x4()
{
    mat4 m;
    m.cols[0] = make4(1.0f, 0.0f, 0.0f, 0.0f);
    m.cols[1] = make4(0.0f, 1.0f, 0.0f, 0.0f);
    m.cols[2] = make4(0.0f, 0.0f, 1.0f, 0.0f);
    m.cols[3] = make4(0.0f, 0.0f, 0.0f, 1.0f);
    return m;
}
static inline mat4 make4x4(mat3 m, vec3 v)
{
    mat4 res;
    res.cols[0] = to_vec4(m.cols[0], 0);
    res.cols[1] = to_vec4(m.cols[1], 0);
    res.cols[2] = to_vec4(m.cols[2], 0);
    res.cols[3] = to_vec4(v, 1);
    return res;
}
static inline mat4 translate(vec3 v)
{
    mat4 m;
    m.cols[0] = make4(1.0f, 0.0f, 0.0f, 0.0f);
    m.cols[1] = make4(0.0f, 1.0f, 0.0f, 0.0f);
    m.cols[2] = make4(0.0f, 0.0f, 1.0f, 0.0f);
    m.cols[3] = make4(v.x, v.y, v.z, 1.0f);
    return m;
}
static inline mat4 mul4x4(mat4 a, mat4 b)
{
    mat4 m;
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            m.cols[i].M[j] = a.cols[i].M[0] * b.cols[0].M[j] +
                             a.cols[i].M[1] * b.cols[1].M[j] +
                             a.cols[i].M[2] * b.cols[2].M[j] +
                             a.cols[i].M[3] * b.cols[3].M[j];
        }
    }
    return m;
}
static inline mat4 cmul4x4(float c, mat4 m)
{
    mat4 r = m;
    cmul4(c, r.cols[0]);
    cmul4(c, r.cols[1]);
    cmul4(c, r.cols[2]);
    cmul4(c, r.cols[3]);
    return r;
}
static inline vec4 vmul4(mat4 m, vec4 v)
{
    vec4 res;
    res.x = m.cols[0].x * v.x + m.cols[1].x * v.y + m.cols[2].x * v.z + m.cols[3].x * v.w;
    res.y = m.cols[0].y * v.x + m.cols[1].y * v.y + m.cols[2].y * v.z + m.cols[3].y * v.w;
    res.z = m.cols[0].z * v.x + m.cols[1].z * v.y + m.cols[2].z * v.z + m.cols[3].z * v.w;
    res.w = m.cols[0].w * v.x + m.cols[1].w * v.y + m.cols[2].w * v.z + m.cols[3].w * v.w;
    return res;
}
static inline vec3 vmul4p(mat4 m, vec3 p)
{
    return to_vec3(vmul4(m, to_vec4(p, 1.0f)));
}
static inline vec3 vmul4v(mat4 m, vec3 v)
{
    return to_vec3(vmul4(m, to_vec4(v, 0.0f)));
}
static inline mat4 inverse4x4(mat4 m1)
{
    float tmp[12]; // temp array for pairs
    mat4 m;

    // calculate pairs for first 8 elements (cofactors)
    //
    tmp[0] = m1.cols[2].M[2] * m1.cols[3].M[3];
    tmp[1] = m1.cols[3].M[2] * m1.cols[2].M[3];
    tmp[2] = m1.cols[1].M[2] * m1.cols[3].M[3];
    tmp[3] = m1.cols[3].M[2] * m1.cols[1].M[3];
    tmp[4] = m1.cols[1].M[2] * m1.cols[2].M[3];
    tmp[5] = m1.cols[2].M[2] * m1.cols[1].M[3];
    tmp[6] = m1.cols[0].M[2] * m1.cols[3].M[3];
    tmp[7] = m1.cols[3].M[2] * m1.cols[0].M[3];
    tmp[8] = m1.cols[0].M[2] * m1.cols[2].M[3];
    tmp[9] = m1.cols[2].M[2] * m1.cols[0].M[3];
    tmp[10] = m1.cols[0].M[2] * m1.cols[1].M[3];
    tmp[11] = m1.cols[1].M[2] * m1.cols[0].M[3];

    // calculate first 8 m1.rowents (cofactors)
    //
    m.cols[0].M[0] = tmp[0] * m1.cols[1].M[1] + tmp[3] * m1.cols[2].M[1] + tmp[4] * m1.cols[3].M[1];
    m.cols[0].M[0] -= tmp[1] * m1.cols[1].M[1] + tmp[2] * m1.cols[2].M[1] + tmp[5] * m1.cols[3].M[1];
    m.cols[0].M[1] = tmp[1] * m1.cols[0].M[1] + tmp[6] * m1.cols[2].M[1] + tmp[9] * m1.cols[3].M[1];
    m.cols[0].M[1] -= tmp[0] * m1.cols[0].M[1] + tmp[7] * m1.cols[2].M[1] + tmp[8] * m1.cols[3].M[1];
    m.cols[0].M[2] = tmp[2] * m1.cols[0].M[1] + tmp[7] * m1.cols[1].M[1] + tmp[10] * m1.cols[3].M[1];
    m.cols[0].M[2] -= tmp[3] * m1.cols[0].M[1] + tmp[6] * m1.cols[1].M[1] + tmp[11] * m1.cols[3].M[1];
    m.cols[0].M[3] = tmp[5] * m1.cols[0].M[1] + tmp[8] * m1.cols[1].M[1] + tmp[11] * m1.cols[2].M[1];
    m.cols[0].M[3] -= tmp[4] * m1.cols[0].M[1] + tmp[9] * m1.cols[1].M[1] + tmp[10] * m1.cols[2].M[1];
    m.cols[1].M[0] = tmp[1] * m1.cols[1].M[0] + tmp[2] * m1.cols[2].M[0] + tmp[5] * m1.cols[3].M[0];
    m.cols[1].M[0] -= tmp[0] * m1.cols[1].M[0] + tmp[3] * m1.cols[2].M[0] + tmp[4] * m1.cols[3].M[0];
    m.cols[1].M[1] = tmp[0] * m1.cols[0].M[0] + tmp[7] * m1.cols[2].M[0] + tmp[8] * m1.cols[3].M[0];
    m.cols[1].M[1] -= tmp[1] * m1.cols[0].M[0] + tmp[6] * m1.cols[2].M[0] + tmp[9] * m1.cols[3].M[0];
    m.cols[1].M[2] = tmp[3] * m1.cols[0].M[0] + tmp[6] * m1.cols[1].M[0] + tmp[11] * m1.cols[3].M[0];
    m.cols[1].M[2] -= tmp[2] * m1.cols[0].M[0] + tmp[7] * m1.cols[1].M[0] + tmp[10] * m1.cols[3].M[0];
    m.cols[1].M[3] = tmp[4] * m1.cols[0].M[0] + tmp[9] * m1.cols[1].M[0] + tmp[10] * m1.cols[2].M[0];
    m.cols[1].M[3] -= tmp[5] * m1.cols[0].M[0] + tmp[8] * m1.cols[1].M[0] + tmp[11] * m1.cols[2].M[0];

    // calculate pairs for second 8 m1.rowents (cofactors)
    //
    tmp[0] = m1.cols[2].M[0] * m1.cols[3].M[1];
    tmp[1] = m1.cols[3].M[0] * m1.cols[2].M[1];
    tmp[2] = m1.cols[1].M[0] * m1.cols[3].M[1];
    tmp[3] = m1.cols[3].M[0] * m1.cols[1].M[1];
    tmp[4] = m1.cols[1].M[0] * m1.cols[2].M[1];
    tmp[5] = m1.cols[2].M[0] * m1.cols[1].M[1];
    tmp[6] = m1.cols[0].M[0] * m1.cols[3].M[1];
    tmp[7] = m1.cols[3].M[0] * m1.cols[0].M[1];
    tmp[8] = m1.cols[0].M[0] * m1.cols[2].M[1];
    tmp[9] = m1.cols[2].M[0] * m1.cols[0].M[1];
    tmp[10] = m1.cols[0].M[0] * m1.cols[1].M[1];
    tmp[11] = m1.cols[1].M[0] * m1.cols[0].M[1];

    // calculate second 8 m1 (cofactors)
    //
    m.cols[2].M[0] = tmp[0] * m1.cols[1].M[3] + tmp[3] * m1.cols[2].M[3] + tmp[4] * m1.cols[3].M[3];
    m.cols[2].M[0] -= tmp[1] * m1.cols[1].M[3] + tmp[2] * m1.cols[2].M[3] + tmp[5] * m1.cols[3].M[3];
    m.cols[2].M[1] = tmp[1] * m1.cols[0].M[3] + tmp[6] * m1.cols[2].M[3] + tmp[9] * m1.cols[3].M[3];
    m.cols[2].M[1] -= tmp[0] * m1.cols[0].M[3] + tmp[7] * m1.cols[2].M[3] + tmp[8] * m1.cols[3].M[3];
    m.cols[2].M[2] = tmp[2] * m1.cols[0].M[3] + tmp[7] * m1.cols[1].M[3] + tmp[10] * m1.cols[3].M[3];
    m.cols[2].M[2] -= tmp[3] * m1.cols[0].M[3] + tmp[6] * m1.cols[1].M[3] + tmp[11] * m1.cols[3].M[3];
    m.cols[2].M[3] = tmp[5] * m1.cols[0].M[3] + tmp[8] * m1.cols[1].M[3] + tmp[11] * m1.cols[2].M[3];
    m.cols[2].M[3] -= tmp[4] * m1.cols[0].M[3] + tmp[9] * m1.cols[1].M[3] + tmp[10] * m1.cols[2].M[3];
    m.cols[3].M[0] = tmp[2] * m1.cols[2].M[2] + tmp[5] * m1.cols[3].M[2] + tmp[1] * m1.cols[1].M[2];
    m.cols[3].M[0] -= tmp[4] * m1.cols[3].M[2] + tmp[0] * m1.cols[1].M[2] + tmp[3] * m1.cols[2].M[2];
    m.cols[3].M[1] = tmp[8] * m1.cols[3].M[2] + tmp[0] * m1.cols[0].M[2] + tmp[7] * m1.cols[2].M[2];
    m.cols[3].M[1] -= tmp[6] * m1.cols[2].M[2] + tmp[9] * m1.cols[3].M[2] + tmp[1] * m1.cols[0].M[2];
    m.cols[3].M[2] = tmp[6] * m1.cols[1].M[2] + tmp[11] * m1.cols[3].M[2] + tmp[3] * m1.cols[0].M[2];
    m.cols[3].M[2] -= tmp[10] * m1.cols[3].M[2] + tmp[2] * m1.cols[0].M[2] + tmp[7] * m1.cols[1].M[2];
    m.cols[3].M[3] = tmp[10] * m1.cols[2].M[2] + tmp[4] * m1.cols[0].M[2] + tmp[9] * m1.cols[1].M[2];
    m.cols[3].M[3] -= tmp[8] * m1.cols[1].M[2] + tmp[11] * m1.cols[2].M[2] + tmp[5] * m1.cols[0].M[2];

    // calculate matrix inverse
    //
    const float k = 1.0f / (m1.cols[0].M[0] * m.cols[0].M[0] +
                            m1.cols[1].M[0] * m.cols[0].M[1] +
                            m1.cols[2].M[0] * m.cols[0].M[2] +
                            m1.cols[3].M[0] * m.cols[0].M[3]);

    m.cols[0] = cmul4(k, m.cols[0]);
    m.cols[1] = cmul4(k, m.cols[1]);
    m.cols[2] = cmul4(k, m.cols[2]);
    m.cols[3] = cmul4(k, m.cols[3]);

    return m;
}
static vec4 get_row4(mat4 m, int row)
{
    return make4(m.cols[0].M[row], m.cols[1].M[row], m.cols[2].M[row], m.cols[3].M[row]);
}
static mat4 transpose4(mat4 m)
{
    mat4 res;
    res.cols[0] = get_row4(m, 0);
    res.cols[1] = get_row4(m, 1);
    res.cols[2] = get_row4(m, 2);
    res.cols[3] = get_row4(m, 3);
    return res;
}

static mat4 look_at(vec3 eye, vec3 center, vec3 up)
{
    const vec3 f = norm3(sub3(center, eye));
    const vec3 s = norm3(cross3(f, up));
    const vec3 u = cross3(s, f);
    mat4 m;
    m.cols[0] = make4(s.x, u.x, -f.x, 0.0f);
    m.cols[1] = make4(s.y, u.y, -f.y, 0.0f);
    m.cols[2] = make4(s.z, u.z, -f.z, 0.0f);
    m.cols[3] = make4(-dot3(s, eye), -dot3(u, eye), dot3(f, eye), 1.0f);
    return m;
}

static inline mat4 perspective(float fovy, float aspect, float zNear, float zFar)
{
    const float tanHalfFovy = tanf(fovy / 2.0f);
    mat4 m;
    m.cols[0] = make4(1.0f / (aspect * tanHalfFovy), 0.0f, 0.0f, 0.0f);
    m.cols[1] = make4(0.0f, 1.0f / (tanHalfFovy), 0.0f, 0.0f);
    m.cols[2] = make4(0.0f, 0.0f, -(zFar + zNear) / (zFar - zNear), -1.0f);
    m.cols[3] = make4(0.0f, 0.0f, -(2.0f * zFar * zNear) / (zFar - zNear), 0.0f);
    return m;
}

#endif