#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 out_fragColor;

layout (location = 0 ) in VS_OUT
{
  vec2 texCoord;
} vOut;

layout (binding = 0) uniform sampler2D SampleImage;

void main()
{
  vec4 r = textureGather(SampleImage, vOut.texCoord, 0);
  vec4 g = textureGather(SampleImage, vOut.texCoord, 1);
  vec4 b = textureGather(SampleImage, vOut.texCoord, 2);
  vec4 a = textureGather(SampleImage, vOut.texCoord, 3);

  out_fragColor = vec4(r.x + r.y + r.z + r.w,
    g.x + g.y + g.z + g.w,
    b.x + b.y + b.z + b.w,
    a.x + a.y + a.z + a.w) * 0.25;
}