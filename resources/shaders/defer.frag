#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"

layout (location = 0 ) in VS_OUT
{
    vec3 wPos;
    vec3 wNorm;
    vec2 texCoord;
} surf;

layout(push_constant) uniform params_t
{
    mat4 mProjView;
    mat4 mModel;
    uint modelId;
} params;


layout (location = 0) out vec3 position;
layout (location = 1) out vec3 normal;
layout (location = 2) out vec3 albedo;


void main()
{
  position = surf.wPos;
  normal = surf.wNorm;
  // https://www.schemecolor.com/all-my-colors.php
  if (params.modelId == 0) {
    albedo = vec3(237, 109, 156) / 255.;
  } else if (params.modelId == 1) {
    albedo = vec3(239, 169, 153) / 255.;
  } else if (params.modelId == 2) {
    albedo = vec3(245, 237, 152) / 255.;
  } else if (params.modelId == 3) {
    albedo = vec3( 79, 236, 138) / 255.;
  } else if (params.modelId == 4) {
    albedo = vec3( 95, 177, 241) / 255.;
  } else if (params.modelId == 5) {
    albedo = vec3(136, 121, 241) / 255.;
  }
}