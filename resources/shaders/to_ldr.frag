#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"

layout (location = 0) out vec4 out_fragColor;

layout (location = 0 ) in VS_OUT
{
  vec2 texCoord;
} surf;

layout (binding = 0, set = 0) uniform AppData
{
  UniformParams Params;
};

layout (binding = 1) uniform sampler2D hdrImage;

// Tone mappings are taken from https://www.shadertoy.com/view/lslGzl

vec3 Uncharted2ToneMapping(vec3 color)
{
	float A = 0.15;
	float B = 0.50;
	float C = 0.10;
	float D = 0.20;
	float E = 0.02;
	float F = 0.30;
	float W = 11.2;
	color *= Params.exposure;
	color = ((color * (A * color + C * B) + D * E) / (color * (A * color + B) + D * F)) - E / F;
	float white = ((W * (A * W + C * B) + D * E) / (W * (A * W + B) + D * F)) - E / F;
	color /= white;
	color = pow(color, vec3(1. / 2.2));
	return color;
}

vec3 simpleReinhardToneMapping(vec3 color)
{
	color *= Params.exposure / (1. + color / Params.exposure);
	color = pow(color, vec3(1. / 2.2));
	return color;
}

// this was taken from lecture

vec3 exponentialToneMapping(vec3 color)
{
    float luminance = max(dot(color, vec3(0.299, 0.587, 0.114)), 0.0001);
    float mappedLuminance = 1 - exp(- luminance / Params.whiteLevel);
    return color * (mappedLuminance / luminance);
}


void main()
{
  const vec4 hdrColor = texture(hdrImage, surf.texCoord);
  switch (Params.toneMappingMode) {
    case 0: // None
        // no need to clamp, because of the swapchain's format it will be clamped in [0, 1]^4 anyway
        out_fragColor = hdrColor; 
        break;
    case 1: // U2
        out_fragColor = vec4(Uncharted2ToneMapping(hdrColor.rgb), 1.f);
        break;
    case 2: // Reinhard
        out_fragColor = vec4(simpleReinhardToneMapping(hdrColor.rgb), 1.f);
        break;
    case 3: // exponential
        out_fragColor = vec4(exponentialToneMapping(hdrColor.rgb), 1.f);
        break;
  }
}