#ifndef FXAA_H
#define FXAA_H
#include "SGL.h"

vec4 FXAAPixelShader(const Texture_F32 *tex, vec2 pos, vec2 frame_rcp);
void FXAA_pass(const Texture_F32 *tex, Texture_F32 *tmp_luma, Texture_F32 *out);
#endif