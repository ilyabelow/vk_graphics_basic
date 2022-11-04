#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "unpack_attributes.h"
#include "common.h"

layout(location = 0) in vec4 vPosNorm;
layout(location = 1) in vec4 vTexCoordAndTang;

layout(push_constant) uniform params_t
{
    mat4 mProjView;
    vec4 mModel;
    uint mModelId;
} params;


layout (location = 0 ) out VS_OUT
{
    vec3 wPos;
    vec3 wNorm;
    vec3 wTangent;
    vec2 texCoord;

} vOut;

layout(binding = 0, set = 0) uniform AppData
{
  UniformParams Params;
};


out gl_PerVertex { vec4 gl_Position; };

mat4 rotateY(float a) {
    float c = cos(a);
    float s = sin(a);

    return mat4( c,  0,  s,  0,
                 0,  1,  0,  0,
                -s,  0,  c,  0,
                 0,  0,  0,  1);
}

mat4 translate(vec3 t) {
    return mat4( 1,  0,  0,  0,
                 0,  1,  0,  0,
                 0,  0,  1,  0,
                 t.x,  t.y,  t.z,  1);
}

mat4 fromVec(vec4 a) {

    return mat4( 1,  0,  0,  0,
                 0,  1,  0,  0,
                 0,  0,  1,  0,
                 a.x,  a.y,  a.z,  1);
}



void main(void)
{
    mat4 modelM = fromVec(params.mModel);
    if (params.mModelId == 1) {
        modelM = modelM * rotateY(Params.time) * translate(vec3(0,abs(sin(Params.time*2.1)), 0)*.3);
    }
    if (params.mModelId == 2) {
        modelM = modelM * translate(vec3(0,0,sin(Params.time))*.3);
    }
    if (params.mModelId == 3) {
        modelM = modelM * rotateY(sin(Params.time)*5);
    }
    if (params.mModelId == 5) {
        modelM = modelM * translate(vec3(sin(Params.time),sin(Params.time*10)*.1,cos(Params.time))*.5) ;
    }
    const vec4 wNorm = vec4(DecodeNormal(floatBitsToInt(vPosNorm.w)),         0.0f);
    const vec4 wTang = vec4(DecodeNormal(floatBitsToInt(vTexCoordAndTang.z)), 0.0f);
    if (params.mModelId != 4) {
        vOut.wPos = (modelM * vec4(vPosNorm.xyz, 1.0f)).xyz;
    } else {
        vOut.wPos = (modelM *  vec4(vPosNorm.xyz, 1.0f) + wNorm * abs(sin(Params.time))*0.1).xyz;
    }
    vOut.wNorm    = normalize(mat3(transpose(inverse(modelM))) * wNorm.xyz);
    vOut.wTangent = normalize(mat3(transpose(inverse(modelM))) * wTang.xyz);
    vOut.texCoord = vTexCoordAndTang.xy;

    gl_Position   = params.mProjView * vec4(vOut.wPos, 1.0);
}
