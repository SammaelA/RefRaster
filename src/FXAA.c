// FXAA 3.11 - PC quality path, taken verbatim from NVIDIA's Fxaa3_11.h reference header.
#include "FXAA.h"

#include <math.h>
#include <stdio.h>

// Quality preset, same numbering as the reference header.
// 12 = default, 25/29 = higher quality, 39 = extreme quality (slowest).
#ifndef FXAA_QUALITY_PRESET
#define FXAA_QUALITY_PRESET 12
#endif

// Amount of sub-pixel aliasing removal: 1.0 softer ... 0.0 off, 0.75 default.
#ifndef FXAA_QUALITY_SUBPIX
#define FXAA_QUALITY_SUBPIX 0.75f
#endif

// Minimum local contrast required to run the algorithm: 0.333 faster ... 0.063 overkill.
#ifndef FXAA_QUALITY_EDGE_THRESHOLD
#define FXAA_QUALITY_EDGE_THRESHOLD 0.333f
#endif

// Trims the algorithm from processing darks: 0.0833 default ... 0.0312 visible limit.
#ifndef FXAA_QUALITY_EDGE_THRESHOLD_MIN
#define FXAA_QUALITY_EDGE_THRESHOLD_MIN 0.0833f
#endif

// Edge search step lengths, FXAA_QUALITY__P0..Pn of the reference header.
#if (FXAA_QUALITY_PRESET == 12)
static const float fxaa_quality_p[] = { 1.0f, 1.5f, 2.0f, 4.0f, 12.0f };
#elif (FXAA_QUALITY_PRESET == 25)
static const float fxaa_quality_p[] = { 1.0f, 1.5f, 2.0f, 2.0f, 2.0f, 2.0f, 4.0f, 8.0f };
#elif (FXAA_QUALITY_PRESET == 29)
static const float fxaa_quality_p[] = { 1.0f, 1.5f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 4.0f, 8.0f };
#elif (FXAA_QUALITY_PRESET == 39)
static const float fxaa_quality_p[] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.5f, 2.0f, 2.0f, 2.0f, 2.0f, 4.0f, 8.0f };
#else
#error "Unsupported FXAA_QUALITY_PRESET (supported: 12, 25, 29, 39)"
#endif

#define FXAA_QUALITY_PS ((int)(sizeof(fxaa_quality_p) / sizeof(fxaa_quality_p[0])))

/*============================================================================
                            TEXTURE SAMPLING
============================================================================*/
static vec3 fxaa_tex_top(const Texture_F32 *tex, vec2 pos)
{
    const float x = pos.x * tex->w;// - 0.5f;
    const float y = pos.y * tex->h;// - 0.5f;
    const float fx = floorf(x);
    const float fy = floorf(y);
    const float dx = x - fx;
    const float dy = y - fy;

    const int i0 = clampi((int)fx, 0, tex->w - 1);
    const int j0 = clampi((int)fy, 0, tex->h - 1);
    const int i1 = clampi(i0 + 1, 0, tex->w - 1);
    const int j1 = clampi(j0 + 1, 0, tex->h - 1);

    const float *t00 = tex->data + tex->ch * (tex->w * j0 + i0);
    const float *t10 = tex->data + tex->ch * (tex->w * j0 + i1);
    const float *t01 = tex->data + tex->ch * (tex->w * j1 + i0);
    const float *t11 = tex->data + tex->ch * (tex->w * j1 + i1);

    vec3 res = make_zero3();
    for (int ch = 0; ch < mini(3, tex->ch); ch++)
    {
        res.M[ch] = (1 - dx) * (1 - dy) * t00[ch] +
                       (dx) * (1 - dy) * t10[ch] +
                    (1 - dx) *    (dy) * t01[ch] +
                       (dx) *    (dy) * t11[ch];
    }
    return res;
}

// FxaaTexOff(tex, pos, off, rcp): fetch offset by a whole number of pixels.
static inline vec3 fxaa_tex_off(const Texture_F32 *tex, vec2 pos, ivec2 off, vec2 rcp_frame)
{
    return fxaa_tex_top(tex, make2(pos.x + off.x * rcp_frame.x, pos.y + off.y * rcp_frame.y));
}

static inline float fxaa_luma(vec3 rgb)
{
    const float lin = dot3(rgb, make3(0.2126f, 0.7152f, 0.0722f));
    return sqrtf(maxf(lin, 0.0f));
}

static inline float fxaa_sat(float x) { return clampf(x, 0.0f, 1.0f); }

/*============================================================================
                             FXAA3 QUALITY - PC
============================================================================*/

// pos       - center of the pixel, in normalized texture coordinates
// frame_rcp - (1 / width, 1 / height) of the input texture
// Returns rgb of the filtered pixel; .w carries lumaM, as in the reference
// (it is the luma of the *source* pixel, not a valid alpha/depth value).
vec4 FXAAPixelShader(const Texture_F32 *tex, vec2 pos, vec2 frame_rcp)
{
    vec2 posM = pos;

    const vec3 rgbM = fxaa_tex_top(tex, posM);
    const float lumaM = fxaa_luma(rgbM);
    float lumaS = fxaa_luma(fxaa_tex_off(tex, posM, makei2( 0,  1), frame_rcp));
    float lumaE = fxaa_luma(fxaa_tex_off(tex, posM, makei2( 1,  0), frame_rcp));
    float lumaN = fxaa_luma(fxaa_tex_off(tex, posM, makei2( 0, -1), frame_rcp));
    float lumaW = fxaa_luma(fxaa_tex_off(tex, posM, makei2(-1,  0), frame_rcp));
/*--------------------------------------------------------------------------*/
    const float maxSM = maxf(lumaS, lumaM);
    const float minSM = minf(lumaS, lumaM);
    const float maxESM = maxf(lumaE, maxSM);
    const float minESM = minf(lumaE, minSM);
    const float maxWN = maxf(lumaN, lumaW);
    const float minWN = minf(lumaN, lumaW);
    const float rangeMax = maxf(maxWN, maxESM);
    const float rangeMin = minf(minWN, minESM);
    const float rangeMaxScaled = rangeMax * FXAA_QUALITY_EDGE_THRESHOLD;
    const float range = rangeMax - rangeMin;
    const float rangeMaxClamped = maxf(FXAA_QUALITY_EDGE_THRESHOLD_MIN, rangeMaxScaled);
    const int earlyExit = range < rangeMaxClamped;
/*--------------------------------------------------------------------------*/
    if (earlyExit)
        return to_vec4(rgbM, lumaM);
    //else
    //    return make4(1,0,0,1);
/*--------------------------------------------------------------------------*/
    const float lumaNW = fxaa_luma(fxaa_tex_off(tex, posM, makei2(-1, -1), frame_rcp));
    const float lumaSE = fxaa_luma(fxaa_tex_off(tex, posM, makei2( 1,  1), frame_rcp));
    const float lumaNE = fxaa_luma(fxaa_tex_off(tex, posM, makei2( 1, -1), frame_rcp));
    const float lumaSW = fxaa_luma(fxaa_tex_off(tex, posM, makei2(-1,  1), frame_rcp));
/*--------------------------------------------------------------------------*/
    const float lumaNS = lumaN + lumaS;
    const float lumaWE = lumaW + lumaE;
    const float subpixRcpRange = 1.0f / range;
    const float subpixNSWE = lumaNS + lumaWE;
    const float edgeHorz1 = (-2.0f * lumaM) + lumaNS;
    const float edgeVert1 = (-2.0f * lumaM) + lumaWE;
/*--------------------------------------------------------------------------*/
    const float lumaNESE = lumaNE + lumaSE;
    const float lumaNWNE = lumaNW + lumaNE;
    const float edgeHorz2 = (-2.0f * lumaE) + lumaNESE;
    const float edgeVert2 = (-2.0f * lumaN) + lumaNWNE;
/*--------------------------------------------------------------------------*/
    const float lumaNWSW = lumaNW + lumaSW;
    const float lumaSWSE = lumaSW + lumaSE;
    const float edgeHorz4 = (fabsf(edgeHorz1) * 2.0f) + fabsf(edgeHorz2);
    const float edgeVert4 = (fabsf(edgeVert1) * 2.0f) + fabsf(edgeVert2);
    const float edgeHorz3 = (-2.0f * lumaW) + lumaNWSW;
    const float edgeVert3 = (-2.0f * lumaS) + lumaSWSE;
    const float edgeHorz = fabsf(edgeHorz3) + edgeHorz4;
    const float edgeVert = fabsf(edgeVert3) + edgeVert4;
/*--------------------------------------------------------------------------*/
    const float subpixNWSWNESE = lumaNWSW + lumaNESE;
    float lengthSign = frame_rcp.x;
    const int horzSpan = edgeHorz >= edgeVert;
    const float subpixA = subpixNSWE * 2.0f + subpixNWSWNESE;
/*--------------------------------------------------------------------------*/
    if (!horzSpan) lumaN = lumaW;
    if (!horzSpan) lumaS = lumaE;
    if ( horzSpan) lengthSign = frame_rcp.y;
    const float subpixB = (subpixA * (1.0f / 12.0f)) - lumaM;
/*--------------------------------------------------------------------------*/
    const float gradientN = lumaN - lumaM;
    const float gradientS = lumaS - lumaM;
    float lumaNN = lumaN + lumaM;
    const float lumaSS = lumaS + lumaM;
    const int pairN = fabsf(gradientN) >= fabsf(gradientS);
    const float gradient = maxf(fabsf(gradientN), fabsf(gradientS));
    if (pairN) lengthSign = -lengthSign;
    const float subpixC = fxaa_sat(fabsf(subpixB) * subpixRcpRange);
/*--------------------------------------------------------------------------*/
    vec2 posB = posM;
    vec2 offNP;
    offNP.x = (!horzSpan) ? 0.0f : frame_rcp.x;
    offNP.y = ( horzSpan) ? 0.0f : frame_rcp.y;
    if (!horzSpan) posB.x += lengthSign * 0.5f;
    if ( horzSpan) posB.y += lengthSign * 0.5f;
/*--------------------------------------------------------------------------*/
    vec2 posN = sub2(posB, cmul2(fxaa_quality_p[0], offNP));
    vec2 posP = add2(posB, cmul2(fxaa_quality_p[0], offNP));
    const float subpixD = ((-2.0f) * subpixC) + 3.0f;
    float lumaEndN = fxaa_luma(fxaa_tex_top(tex, posN));
    const float subpixE = subpixC * subpixC;
    float lumaEndP = fxaa_luma(fxaa_tex_top(tex, posP));
/*--------------------------------------------------------------------------*/
    if (!pairN) lumaNN = lumaSS;
    const float gradientScaled = gradient * 1.0f / 4.0f;
    const float lumaMM = lumaM - lumaNN * 0.5f;
    const float subpixF = subpixD * subpixE;
    const int lumaMLTZero = lumaMM < 0.0f;
/*--------------------------------------------------------------------------*/
    lumaEndN -= lumaNN * 0.5f;
    lumaEndP -= lumaNN * 0.5f;
    int doneN = fabsf(lumaEndN) >= gradientScaled;
    int doneP = fabsf(lumaEndP) >= gradientScaled;
    if (!doneN) posN = sub2(posN, cmul2(fxaa_quality_p[1], offNP));
    int doneNP = (!doneN) || (!doneP);
    if (!doneP) posP = add2(posP, cmul2(fxaa_quality_p[1], offNP));
/*--------------------------------------------------------------------------*/
    // The reference unrolls this search as nested #if blocks, one per quality step.
    for (int i = 2; i < FXAA_QUALITY_PS && doneNP; i++)
    {
        if (!doneN) lumaEndN = fxaa_luma(fxaa_tex_top(tex, posN)) - lumaNN * 0.5f;
        if (!doneP) lumaEndP = fxaa_luma(fxaa_tex_top(tex, posP)) - lumaNN * 0.5f;
        doneN = fabsf(lumaEndN) >= gradientScaled;
        doneP = fabsf(lumaEndP) >= gradientScaled;
        if (!doneN) posN = sub2(posN, cmul2(fxaa_quality_p[i], offNP));
        doneNP = (!doneN) || (!doneP);
        if (!doneP) posP = add2(posP, cmul2(fxaa_quality_p[i], offNP));
    }
/*--------------------------------------------------------------------------*/
    float dstN = posM.x - posN.x;
    float dstP = posP.x - posM.x;
    if (!horzSpan) dstN = posM.y - posN.y;
    if (!horzSpan) dstP = posP.y - posM.y;
/*--------------------------------------------------------------------------*/
    const int goodSpanN = (lumaEndN < 0.0f) != lumaMLTZero;
    const float spanLength = (dstP + dstN);
    const int goodSpanP = (lumaEndP < 0.0f) != lumaMLTZero;
    const float spanLengthRcp = 1.0f / spanLength;
/*--------------------------------------------------------------------------*/
    const int directionN = dstN < dstP;
    const float dst = minf(dstN, dstP);
    const int goodSpan = directionN ? goodSpanN : goodSpanP;
    const float subpixG = subpixF * subpixF;
    const float pixelOffset = (dst * (-spanLengthRcp)) + 0.5f;
    const float subpixH = subpixG * FXAA_QUALITY_SUBPIX;
/*--------------------------------------------------------------------------*/
    const float pixelOffsetGood = goodSpan ? pixelOffset : 0.0f;
    const float pixelOffsetSubpix = maxf(pixelOffsetGood, subpixH);
    if (!horzSpan) posM.x += pixelOffsetSubpix * lengthSign;
    if ( horzSpan) posM.y += pixelOffsetSubpix * lengthSign;

    return to_vec4(fxaa_tex_top(tex, posM), lumaM);
}

vec3 fetch_rgb(const Texture_F32 *tex, int i, int j)
{
    return make3(tex->data[tex->ch * (tex->w * j + i) + 0],
                 tex->data[tex->ch * (tex->w * j + i) + 1],
                 tex->data[tex->ch * (tex->w * j + i) + 2]);
}

float fetch_luma(const Texture_F32 *tex, int i, int j)
{
    return tex->data[(tex->w * j + i)];
}

vec4 FXAAPixelShaderFast(const Texture_F32 *tex, const Texture_F32 *luma, vec2 frame_rcp, int x, int y)
{
    vec2 posM = make2((float)x/tex->w, (float)y/tex->h);

    const vec3 rgbM = fetch_rgb(tex, x, y);
    const float lumaM = fetch_luma(luma, x, y);
    float lumaS = fetch_luma(luma, x, y+1);
    float lumaE = fetch_luma(luma, x+1, y);
    float lumaN = fetch_luma(luma, x, y-1);
    float lumaW = fetch_luma(luma, x-1, y);
/*--------------------------------------------------------------------------*/
    const float maxSM = maxf(lumaS, lumaM);
    const float minSM = minf(lumaS, lumaM);
    const float maxESM = maxf(lumaE, maxSM);
    const float minESM = minf(lumaE, minSM);
    const float maxWN = maxf(lumaN, lumaW);
    const float minWN = minf(lumaN, lumaW);
    const float rangeMax = maxf(maxWN, maxESM);
    const float rangeMin = minf(minWN, minESM);
    const float rangeMaxScaled = rangeMax * FXAA_QUALITY_EDGE_THRESHOLD;
    const float range = rangeMax - rangeMin;
    const float rangeMaxClamped = maxf(FXAA_QUALITY_EDGE_THRESHOLD_MIN, rangeMaxScaled);
    const int earlyExit = range < rangeMaxClamped;
/*--------------------------------------------------------------------------*/
    if (earlyExit)
        return to_vec4(rgbM, lumaM);
    //else
    //    return make4(1,0,0,1);
/*--------------------------------------------------------------------------*/
    const float lumaNW = fetch_luma(luma, x-1, y-1);
    const float lumaSE = fetch_luma(luma, x+1, y+1);
    const float lumaNE = fetch_luma(luma, x+1, y-1);
    const float lumaSW = fetch_luma(luma, x-1, y+1);
/*--------------------------------------------------------------------------*/
    const float lumaNS = lumaN + lumaS;
    const float lumaWE = lumaW + lumaE;
    const float subpixRcpRange = 1.0f / range;
    const float subpixNSWE = lumaNS + lumaWE;
    const float edgeHorz1 = (-2.0f * lumaM) + lumaNS;
    const float edgeVert1 = (-2.0f * lumaM) + lumaWE;
/*--------------------------------------------------------------------------*/
    const float lumaNESE = lumaNE + lumaSE;
    const float lumaNWNE = lumaNW + lumaNE;
    const float edgeHorz2 = (-2.0f * lumaE) + lumaNESE;
    const float edgeVert2 = (-2.0f * lumaN) + lumaNWNE;
/*--------------------------------------------------------------------------*/
    const float lumaNWSW = lumaNW + lumaSW;
    const float lumaSWSE = lumaSW + lumaSE;
    const float edgeHorz4 = (fabsf(edgeHorz1) * 2.0f) + fabsf(edgeHorz2);
    const float edgeVert4 = (fabsf(edgeVert1) * 2.0f) + fabsf(edgeVert2);
    const float edgeHorz3 = (-2.0f * lumaW) + lumaNWSW;
    const float edgeVert3 = (-2.0f * lumaS) + lumaSWSE;
    const float edgeHorz = fabsf(edgeHorz3) + edgeHorz4;
    const float edgeVert = fabsf(edgeVert3) + edgeVert4;
/*--------------------------------------------------------------------------*/
    const float subpixNWSWNESE = lumaNWSW + lumaNESE;
    float lengthSign = frame_rcp.x;
    const int horzSpan = edgeHorz >= edgeVert;
    const float subpixA = subpixNSWE * 2.0f + subpixNWSWNESE;
/*--------------------------------------------------------------------------*/
    if (!horzSpan) lumaN = lumaW;
    if (!horzSpan) lumaS = lumaE;
    if ( horzSpan) lengthSign = frame_rcp.y;
    const float subpixB = (subpixA * (1.0f / 12.0f)) - lumaM;
/*--------------------------------------------------------------------------*/
    const float gradientN = lumaN - lumaM;
    const float gradientS = lumaS - lumaM;
    float lumaNN = lumaN + lumaM;
    const float lumaSS = lumaS + lumaM;
    const int pairN = fabsf(gradientN) >= fabsf(gradientS);
    const float gradient = maxf(fabsf(gradientN), fabsf(gradientS));
    if (pairN) lengthSign = -lengthSign;
    const float subpixC = fxaa_sat(fabsf(subpixB) * subpixRcpRange);
/*--------------------------------------------------------------------------*/
    vec2 posB = posM;
    vec2 offNP;
    offNP.x = (!horzSpan) ? 0.0f : frame_rcp.x;
    offNP.y = ( horzSpan) ? 0.0f : frame_rcp.y;
    if (!horzSpan) posB.x += lengthSign * 0.5f;
    if ( horzSpan) posB.y += lengthSign * 0.5f;
/*--------------------------------------------------------------------------*/
    vec2 posN = sub2(posB, cmul2(fxaa_quality_p[0], offNP));
    vec2 posP = add2(posB, cmul2(fxaa_quality_p[0], offNP));
    const float subpixD = ((-2.0f) * subpixC) + 3.0f;
    float lumaEndN = sample_f32_r(luma, posN);
    const float subpixE = subpixC * subpixC;
    float lumaEndP = sample_f32_r(luma, posP);
/*--------------------------------------------------------------------------*/
    if (!pairN) lumaNN = lumaSS;
    const float gradientScaled = gradient * 1.0f / 4.0f;
    const float lumaMM = lumaM - lumaNN * 0.5f;
    const float subpixF = subpixD * subpixE;
    const int lumaMLTZero = lumaMM < 0.0f;
/*--------------------------------------------------------------------------*/
    lumaEndN -= lumaNN * 0.5f;
    lumaEndP -= lumaNN * 0.5f;
    int doneN = fabsf(lumaEndN) >= gradientScaled;
    int doneP = fabsf(lumaEndP) >= gradientScaled;
    if (!doneN) posN = sub2(posN, cmul2(fxaa_quality_p[1], offNP));
    int doneNP = (!doneN) || (!doneP);
    if (!doneP) posP = add2(posP, cmul2(fxaa_quality_p[1], offNP));
/*--------------------------------------------------------------------------*/
    // The reference unrolls this search as nested #if blocks, one per quality step.
    for (int i = 2; i < FXAA_QUALITY_PS && doneNP; i++)
    {
        if (!doneN) lumaEndN = sample_f32_r(luma, posN) - lumaNN * 0.5f;
        if (!doneP) lumaEndP = sample_f32_r(luma, posP) - lumaNN * 0.5f;
        doneN = fabsf(lumaEndN) >= gradientScaled;
        doneP = fabsf(lumaEndP) >= gradientScaled;
        if (!doneN) posN = sub2(posN, cmul2(fxaa_quality_p[i], offNP));
        doneNP = (!doneN) || (!doneP);
        if (!doneP) posP = add2(posP, cmul2(fxaa_quality_p[i], offNP));
    }
/*--------------------------------------------------------------------------*/
    float dstN = posM.x - posN.x;
    float dstP = posP.x - posM.x;
    if (!horzSpan) dstN = posM.y - posN.y;
    if (!horzSpan) dstP = posP.y - posM.y;
/*--------------------------------------------------------------------------*/
    const int goodSpanN = (lumaEndN < 0.0f) != lumaMLTZero;
    const float spanLength = (dstP + dstN);
    const int goodSpanP = (lumaEndP < 0.0f) != lumaMLTZero;
    const float spanLengthRcp = 1.0f / spanLength;
/*--------------------------------------------------------------------------*/
    const int directionN = dstN < dstP;
    const float dst = minf(dstN, dstP);
    const int goodSpan = directionN ? goodSpanN : goodSpanP;
    const float subpixG = subpixF * subpixF;
    const float pixelOffset = (dst * (-spanLengthRcp)) + 0.5f;
    const float subpixH = subpixG * FXAA_QUALITY_SUBPIX;
/*--------------------------------------------------------------------------*/
    const float pixelOffsetGood = goodSpan ? pixelOffset : 0.0f;
    const float pixelOffsetSubpix = maxf(pixelOffsetGood, subpixH);
    if (!horzSpan) posM.x += pixelOffsetSubpix * lengthSign;
    if ( horzSpan) posM.y += pixelOffsetSubpix * lengthSign;

    return to_vec4(sample_f32_rgb(tex, posM), lumaM);
}

void FXAA_pass(const Texture_F32 *tex, Texture_F32 *tmp_luma, Texture_F32 *out)
{
    const uint32_t w = tex->w;
    const uint32_t h = tex->h;

    for (int i=0; i<w*h; i++)
    {
        tmp_luma->data[i] = 0.299f*tex->data[tex->ch*i+0]+0.587f*tex->data[tex->ch*i+1]+0.114f*tex->data[tex->ch*i+2];
    }

    const vec2 rcp = make2(1.0f/w, 1.0f/h);

    for (int y=0;y<h;y++)
    {
        for (int x=0;x<w;x++)
        {
            vec4 aa_pixel;
            if (x==0 || y==0 || x==w-1 || y==h-1)
                aa_pixel = to_vec4(fetch_rgb(tex, x, y), 1.0f);
            else
                aa_pixel = FXAAPixelShaderFast(tex, tmp_luma, rcp, x, y);

            const int off = 4*(y*w+x);
            out->data[off+0] = aa_pixel.x;
            out->data[off+1] = aa_pixel.y;
            out->data[off+2] = aa_pixel.z;
            out->data[off+3] = aa_pixel.w;
        }
    }    
}
