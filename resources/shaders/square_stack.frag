#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec2 out_fragColor;

layout (location = 0 ) in VS_OUT
{
    vec3 wPos;
    vec3 wNorm;
    vec3 wTangent;
    vec2 texCoord;
} surf;


void main()
{
    float z = gl_FragCoord.z;
    out_fragColor = vec2(z, z*z);
}