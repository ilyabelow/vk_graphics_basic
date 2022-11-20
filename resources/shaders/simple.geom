#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"

layout(triangles) in;
layout(triangle_strip, max_vertices = 9) out; // will output individual triangles

layout(push_constant) uniform params_t
{
    mat4 mProjView;
    mat4 mModel;
} params;

layout(binding = 0, set = 0) uniform AppData
{
    UniformParams Params;
};

layout (location = 0) in VS_OUT
{
    vec3 wPos;
    vec3 wNorm;
    vec3 wTangent;
    vec2 texCoord;
} gs_in[];

layout (location = 0) out GS_OUT
{
    vec3 wPos;
    vec3 wNorm;
    vec3 wTangent;
    vec2 texCoord;
} gs_out;

out gl_PerVertex { vec4 gl_Position; };

vec3 normal(vec3 p1, vec3 p2, vec3 p3)
{
   return normalize(cross(p1 - p2, p2 - p3));
}


void main()
{
    vec3 midpoint = (gs_in[0].wPos + gs_in[1].wPos + gs_in[2].wPos)*0.333;
    vec3 trig_norm = normal(gs_in[0].wPos, gs_in[1].wPos, gs_in[2].wPos);
    vec3 top = midpoint + trig_norm*(0.01 + sin(Params.time)*.025);
    vec2 mid_tex = (gs_in[0].texCoord + gs_in[1].texCoord + gs_in[2].texCoord)*0.333;

    for (int i = 0; i < 3; i++) {
        int first = i;
        int second = (i + 1) % 3;

        vec3 new_norm = normal(gs_in[first].wPos, gs_in[second].wPos, midpoint);
        vec3 new_tan = normalize(top - gs_in[first].wPos); // ???
        gs_out.wPos = gs_in[first].wPos;
        gs_out.wNorm = new_norm;
        gs_out.wTangent = new_tan;
        gs_out.texCoord = gs_in[first].texCoord;
        gl_Position = params.mProjView * vec4(gs_out.wPos, 1.0);
        EmitVertex();

        gs_out.wPos = gs_in[second].wPos;
        gs_out.wNorm = new_norm;
        gs_out.wTangent = new_tan;
        gs_out.texCoord = gs_in[second].texCoord;
        gl_Position = params.mProjView * vec4(gs_out.wPos, 1.0);
        EmitVertex();

        gs_out.wPos = top;
        gs_out.wNorm = new_norm;
        gs_out.wTangent = new_tan;
        gs_out.texCoord = mid_tex;
        gl_Position = params.mProjView * vec4(gs_out.wPos, 1.0);
        EmitVertex();
        EndPrimitive();
    }

}