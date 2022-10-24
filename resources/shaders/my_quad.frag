#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 color;

layout (binding = 0) uniform sampler2D colorTex;

layout (location = 0 ) in VS_OUT
{
  vec2 texCoord;
} surf;

vec4 bilateralFilter(int d, float sigmaColor, float sigmaSpace) {
  float sigma_d2 = .5/(sigmaSpace*sigmaSpace);
  float sigma_r2 = .5/(sigmaColor*sigmaColor);
  vec2 step = vec2(1.) / textureSize(colorTex, 0);
  vec4 origColor = textureLod(colorTex, surf.texCoord, 0);

  vec4 colorSum = vec4(0);
  float weightSum = 0;
  for (int i = -d/2; i <= d/2; i++) {
    for (int j = -d/2; j <= d/2; j++) {
      vec2 curOffset = vec2(i, j);
      vec4 curColor = textureLod(colorTex, surf.texCoord + curOffset*step, 0);

      float dist2 = dot(curOffset, curOffset);
      float range2 = dot(origColor - curColor, origColor - curColor);
      float weight = exp(-dist2*sigma_d2 - range2*sigma_r2);

      colorSum += weight * curColor;
      weightSum += weight;
    } 
  }
  return colorSum / weightSum;
}

void main() {
  color = bilateralFilter(9, .1, 3);
}