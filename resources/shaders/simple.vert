#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "unpack_attributes.h"


layout(location = 0) in vec4 vPosNorm;
layout(location = 1) in vec4 vTexCoordAndTang;

layout(push_constant) uniform params_t
{
    mat4 mProjView;
    mat4 mModel;
    uint resolution;
} params;


layout (location = 0 ) out VS_OUT
{
    vec3 wPos;
    vec3 wNorm;
    vec3 wTangent;
    vec2 texCoord;

} vOut;

// Noise taken from Wikipedia

vec2 randomGradient(ivec2 v) {
    uint w = 32; // sizeof(uint)
    uint s = w / 2;
    uint a = v.x, b = v.y;
    a *= 3284157443; b ^= a << s | a >> w-s;
    b *= 1911520717; a ^= b << s | b >> w-s;
    a *= 2048419325;
    float random = a * (3.14159265 / ~(~0u >> 1));
    return vec2(cos(random), sin(random));
}

float dotGridGradient(ivec2 node, vec2 p) {
    vec2 gradient = randomGradient(node);

    vec2 displacement = vec2(p.x - float(node.x),
                             p.y - float(node.y));

    return (displacement.x*gradient.x + displacement.y*gradient.y);
}

float perlin(vec2 p, uint resolution) {
    p *= resolution;
    int x0 = int(floor(p.x));
    int y0 = int(floor(p.y));
    int x1 = x0 + 1;
    int y1 = y0 + 1;

    return mix(
        mix(
            dotGridGradient(ivec2(x0, y0), p),
            dotGridGradient(ivec2(x1, y0), p),
            smoothstep(0, 1, p.x - floor(p.x))
        ),
        mix(
            dotGridGradient(ivec2(x0, y1), p),
            dotGridGradient(ivec2(x1, y1), p),
            smoothstep(0, 1, p.x - floor(p.x))
        ),
        smoothstep(0, 1, p.y - floor(p.y))
    )*.5 + .5; // returns from 0 to 1
}

float getHeight(vec2 p) {
    float dist = distance(vec2(0.3, 0.5), p);
    return mix(0, 16.0*dist*dist, perlin(p, 8)) 
            + mix(0, 0.25, perlin(p, 32)) 
            + mix(0, 0.025, perlin(p, 128));
}


out gl_PerVertex { vec4 gl_Position; };
void main(void)
{
    const vec4 wNorm = vec4(DecodeNormal(floatBitsToInt(vPosNorm.w)),         0.0f);
    const vec4 wTang = vec4(DecodeNormal(floatBitsToInt(vTexCoordAndTang.z)), 0.0f);

    vOut.wPos     = (params.mModel * vec4(vPosNorm.xyz, 1.0f)).xyz;
    vOut.wNorm    = normalize(mat3(transpose(inverse(params.mModel))) * wNorm.xyz);
    vOut.wTangent = normalize(mat3(transpose(inverse(params.mModel))) * wTang.xyz);
    vOut.texCoord = vTexCoordAndTang.xy;

    float step = 1.f / float(params.resolution);
    vec2 dx = vec2(step, 0);
    vec2 dy = vec2(0, step);
    vOut.wPos.y = getHeight(vOut.texCoord);

    vOut.wNorm = normalize(vec3(
        -(getHeight(vOut.texCoord+dx) - getHeight(vOut.texCoord-dx)) / (2.f * step), 
        1, // I think there should be not a 1 but something else?????
        -(getHeight(vOut.texCoord+dy) - getHeight(vOut.texCoord-dy)) / (2.f * step)
    ));

    gl_Position = params.mProjView * vec4(vOut.wPos, 1.0);
}
