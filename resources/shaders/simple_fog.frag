#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"

layout(location = 0) out vec4 out_fragColor;

layout (location = 0) in VS_OUT
{
  vec2 texCoord;
} vOut;

layout(binding = 0, set = 0) uniform AppData
{
  UniformParams Uniforms;
};

layout(binding = 1) uniform sampler2D fogDepthMap;


const vec3 AABBleft = vec3(0, 0, -8);
const vec3 AABBright = vec3(8, 8, 8);

// Magic
float hash(float n) {
    return fract(sin(n)*43758.5453);
}

float noise(in vec3 x) {
    vec3 p = floor(x);
    vec3 f = fract(x);
    vec3 t = f*f*(3.0 - 2.0*f);

    float n = p.x + 57.0*p.y + 113.0*p.z;
    float result = mix(mix(mix(hash(n+0.0),   hash(n+1.0), t.x),
                       mix(hash(n+57.0),  hash(n+58.0), t.x), t.y),
                   mix(mix(hash(n+113.0), hash(n+114.0), t.x),
                       mix(hash(n+170.0), hash(n+171.0), t.x), t.y), t.z);
    return result;
}

float fbm(vec3 p) {
    return (((0.5000*noise(p))*2.02 + 0.2500*noise(p))*2.03 + 0.1250*noise(p))*.3;
}


float get_depth() {
  vec2 uv = vOut.texCoord * 2 - vec2(1.);
  float depth = textureLod(fogDepthMap, vOut.texCoord, 0).x;
  // to view space
  vec4 pos_view = Uniforms.projInverse*vec4(uv.x, -uv.y, depth, 1.0);
  return -(pos_view/pos_view.w).z;
}

// NOTE returns incorrect results on borders of fov
vec3 get_dir()
{
  vec2 uv = vOut.texCoord * 2 - vec2(1.);
  float fov_tan = tan(radians(Uniforms.fov / 2));
  float x = uv.x * fov_tan;
  float y = - uv.y * fov_tan;
  vec4 dir = Uniforms.viewInverse * vec4(x, y, -1., 0.);
  return normalize(dir.xyz);
}


bool check_AA_plane_intersection(vec2 p, vec2 left, vec2 right) {
  return p.x > left.x && p.x < right.x && p.y > left.y && p.y < right.y;
}

bool check_AABB_intersection(vec3 origin, vec3 direction, out float t_min, out float t_max) {
  // not exactly the most gpu-friendly code
  bool intersected = false;
  t_min = 100000.;
  t_max = 0.;
  if (abs(direction.y) > 0.01){
    float t1 = (AABBleft.y - origin.y) / direction.y;
    if (check_AA_plane_intersection((origin + direction * t1).xz, AABBleft.xz, AABBright.xz)) {
      intersected = intersected || t1 > 0;
      t_min = min(t_min, t1);
      t_max = max(t_max, t1);
    }
    float t2 = (AABBright.y - origin.y) / direction.y;
    if (check_AA_plane_intersection((origin + direction * t2).xz, AABBleft.xz, AABBright.xz)) {
      intersected = intersected || t2 > 0 ;
      t_min = min(t_min, t2);
      t_max = max(t_max, t2);
    }
  }
  if (abs(direction.x) > 0.01){
    float t1 = (AABBleft.x - origin.x) / direction.x;
    if (check_AA_plane_intersection((origin + direction * t1).yz, AABBleft.yz, AABBright.yz)) {
      intersected = intersected || t1 > 0;
      t_min = min(t_min, t1);
      t_max = max(t_max, t1);
    }
    float t2 = (AABBright.x - origin.x) / direction.x;
    if (check_AA_plane_intersection((origin + direction * t2).yz, AABBleft.yz, AABBright.yz)) {
      intersected = intersected || t2 > 0 ;
      t_min = min(t_min, t2);
      t_max = max(t_max, t2);
    }
  }
  if (abs(direction.z) > 0.01){
    float t1 = (AABBleft.z - origin.z) / direction.z;
    if (check_AA_plane_intersection((origin + direction * t1).xy, AABBleft.xy, AABBright.xy)) {
      intersected = intersected || t1 > 0;
      t_min = min(t_min, t1);
      t_max = max(t_max, t1);
    }
    float t2 = (AABBright.z - origin.z) / direction.z;
    if (check_AA_plane_intersection((origin + direction * t2).xy, AABBleft.xy, AABBright.xy)) {
      intersected = intersected || t2 > 0 ;
      t_min = min(t_min, t2);
      t_max = max(t_max, t2);
    }
  }
  return intersected;
}

// https://iquilezles.org/articles/distfunctions/
float sdCapsule( vec3 p, vec3 a, vec3 b, float r )
{
  vec3 pa = p - a, ba = b - a;
  float h = clamp( dot(pa,ba)/dot(ba,ba), 0.0, 1.0 );
  return length( pa - ba*h ) - r;
}

float opUnion( float d1, float d2 ) { return min(d1,d2); }

float sdfT(vec3 p) {
  return opUnion(
    sdCapsule(p, vec3(4,6,-7), vec3(4,6,-5), .5),
    sdCapsule(p, vec3(4,6,-6), vec3(4,2,-6), .5) );
}

float sdfY(vec3 p) {
    return opUnion(
    sdCapsule(p, vec3(4,2,-4), vec3(4,6,-2), .5),
    sdCapsule(p, vec3(4,6,-4), vec3(4,4,-3), .5) );
}

float sdfM(vec3 p) {
    return
    opUnion(
    opUnion(sdCapsule(p, vec3(4,2,-1.33), vec3(4,6,-0.66), .5),
            sdCapsule(p, vec3(4,6,-0.66), vec3(4,5,0),     .5)
            ),
    opUnion(sdCapsule(p, vec3(4,5,0), vec3(4,6, 0.66),     .5), 
            sdCapsule(p, vec3(4,6,0.66), vec3(4,2,1.33),   .5)) 
    );
}

float sdfA(vec3 p) {
    return 
    opUnion(
            sdCapsule(p, vec3(4,3,2.5), vec3(4,3,3.5), .5),
    opUnion(sdCapsule(p, vec3(4,2,2), vec3(4,6,3), .5), 
            sdCapsule(p, vec3(4,6,3), vec3(4,2,4), .5))
    );
}

float sdfH(vec3 p) {
    return opUnion(
            sdCapsule(p, vec3(4,4,5), vec3(4,4,7), .5),
    opUnion(sdCapsule(p, vec3(4,2,5), vec3(4,6,5), .5), 
            sdCapsule(p, vec3(4,2,7), vec3(4,6,7), .5)) 
    );
}


float sdfTYMAH(vec3 p) {
  return opUnion(opUnion(sdfT(p), sdfY(p)), opUnion(sdfM(p), opUnion(sdfA(p), sdfH(p))));
}

float additionalDensity(vec3 p) {
  return exp(-sdfTYMAH(p+vec3(0, sin(Uniforms.time+p.z)*.15,sin(Uniforms.time*3+p.y*3)*0.02))*5);
}


float fog(vec3 p) {
  // uncomment i sampling outside aabb
  // if (p.x < AABBleft.x || p.y < AABBleft.y || p.z < AABBleft.z || p.x > AABBright.x || p.y > AABBright.y || p.z > AABBright.z) {
  //   return 0.0;
  // }

  // making bb borders less sharp
  float x_dist = min(min(AABBright.x - p.x, p.x - AABBleft.x), 2) * .5; 
  float y_dist = min(min(AABBright.y - p.y, p.y - AABBleft.y), 2) * .5; 
  float z_dist = min(min(AABBright.z - p.z, p.z - AABBleft.z), 2) * .5; 

  float f = fbm(p + vec3(-0.7, 0.3, 0.2)*Uniforms.time);
  return (f + additionalDensity(p))* x_dist * y_dist * z_dist;
}

void main()
{
  vec3 rayDir = get_dir();
  float geometryDepth = get_depth();
  vec3 p = Uniforms.cameraPos;

  // Do not calculate fog if not looking at a for bb
  float t_min, t_max;
  if(!check_AABB_intersection(p, rayDir, t_min, t_max) || t_min > geometryDepth) {
    out_fragColor = vec4(0);
    return;
  }
  // Setting start and finish of marching
  if (t_min > 0) {
    p += t_min * rayDir;
  } else {
    t_min = 0;
  }
  if (t_max > geometryDepth) {
    t_max = geometryDepth;
  }

  float stepSize = .1f;
  float thickness = t_max - t_min;
  int stepsTotal = min(int(thickness / stepSize), 1000);
  const float extinction = .5;
  // Marching itself
  float transmittance = 1.0;
  for (int step = 0; step < stepsTotal; step++) {
    p += rayDir * stepSize;
    float density = fog(p);
    if (density > 0) {
      transmittance *= exp(- density * stepSize * extinction);
      if (transmittance < 0.01) {
        break;
      }
    }
  }

  out_fragColor = vec4(0.6, 0.7, 0.8, 1 - transmittance);
}