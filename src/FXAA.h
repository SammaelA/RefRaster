#ifndef FXAA_H
#define FXAA_H
#include "SGL.h"

vec4 FXAAPixelShader(const Texture_F32 *tex, vec2 pos, vec2 frame_rcp);
#endif