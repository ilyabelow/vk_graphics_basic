#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"

layout(location = 0) out vec4 out_fragColor;

layout (location = 0 ) in VS_OUT
{
  vec2 texCoord;
} surf;

layout(binding = 0, set = 0) uniform AppData
{
  UniformParams Params;
};

layout (binding = 1) uniform sampler2D shadowMap;
layout (binding = 2) uniform sampler2D positionMap;
layout (binding = 3) uniform sampler2D normalMap;
layout (binding = 4) uniform sampler2D albedoMap;

// http://www.iryoku.com/translucency/downloads/Real-Time-Realistic-Skin-Translucency.pdf
vec3 T(float s) {
  return vec3(0.233, 0.455, 0.649)  * exp(-s*s/0.0064) +
         vec3(0.1, 0.336, 0.344)    * exp(-s*s/0.0484) +
         vec3(0.118, 0.198, 0.0)    * exp(-s*s/0.187) +
         vec3(0.113, 0.007, 0.007)  * exp(-s*s/0.567) +
         vec3(0.358, 0.004, 0.0)    * exp(-s*s/1.99) +
         vec3(0.078, 0.0, 0.0)      * exp(-s*s/7.41);
}

float linearizeDepth(vec3 ndc) {
  vec4 inversed = Params.projInverse * vec4(ndc, 1.f);
  return (inversed.xyz / inversed.w).z;
}

void main()
{
  vec3 pos = texture(positionMap, surf.texCoord).xyz;
  vec3 norm = texture(normalMap, surf.texCoord).xyz;
  vec3 albedo = texture(albedoMap, surf.texCoord).xyz;

  const vec4 posLightClipSpace = Params.lightMatrix*vec4(pos, 1.0f); // 
  const vec3 posLightSpaceNDC  = posLightClipSpace.xyz/posLightClipSpace.w;    // for orto matrix, we don't need perspective division, you can remove it if you want; this is general case;
  const vec2 shadowTexCoord    = posLightSpaceNDC.xy*0.5f + vec2(0.5f, 0.5f);  // just shift coords from [-1,1] to [0,1]               
    
  const bool  outOfView = (shadowTexCoord.x < 0.0001f || shadowTexCoord.x > 0.9999f || shadowTexCoord.y < 0.0091f || shadowTexCoord.y > 0.9999f);
  const float shadow    = ((posLightSpaceNDC.z < textureLod(shadowMap, shadowTexCoord, 0).x + 0.001f) || outOfView) ? 1.0f : 0.0f;

  const vec4 dark_violet = vec4(0.59f, 0.0f, 0.82f, 1.0f);
  const vec4 chartreuse  = vec4(0.5f, 1.0f, 0.0f, 1.0f);

  vec4 lightColor1 = mix(dark_violet, chartreuse, abs(sin(Params.time)));
  vec4 lightColor2 = vec4(1.0f, 1.0f, 1.0f, 1.0f);
   
  vec3 lightDir   = normalize(Params.lightPos - pos);
  vec4 lightColor = max(dot(norm, lightDir), 0.0f) * lightColor2;
  out_fragColor   = (lightColor*shadow + vec4(0.1f)) * vec4(albedo, 1.0f);

  if (Params.useSSS) {
    const float shadowDepthLinear = linearizeDepth(vec3(posLightSpaceNDC.xy, textureLod(shadowMap, shadowTexCoord, 0).x));
    const float cameraDepthLinear = (Params.lightView * vec4(pos, 1.0f)).z;

    const float s = abs(shadowDepthLinear - cameraDepthLinear) / Params.SSSintencity;
    const float E = max(0.3 + dot(-norm, lightDir), 0.0);
    const vec4 transmittance = vec4(T(s), 1.0f) * lightColor2 * vec4(albedo, 1.f) * E;
    out_fragColor += transmittance;
  }
}
