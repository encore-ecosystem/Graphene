#version 450
layout(location=0) in vec3 worldPosition;
layout(location=1) in vec3 worldNormal;
layout(location=2) in vec2 texcoord;
layout(location=0) out vec4 outputColor;
layout(set=0,binding=0) uniform Preview {mat4 vp;vec4 camera;vec4 options;} frame;
layout(set=0,binding=1) uniform sampler2D baseMap;
layout(set=0,binding=2) uniform sampler2D normalMap;
layout(set=0,binding=3) uniform sampler2D ormMap;
layout(set=0,binding=4) uniform sampler2D heightMap;
#include "material_normal.glsl"
void main(){
 vec2 uv=texcoord*frame.options.y;
 vec3 base=texture(baseMap,uv).rgb,rm=texture(ormMap,uv).rgb;
 int view=int(frame.camera.w+.5);
 if(view>0){vec3 v=view==1?base:view==2?texture(normalMap,uv).rgb:view==3?vec3(rm.g):view==4?vec3(rm.b):view==6?vec3(rm.r):texture(heightMap,uv).rgb;outputColor=vec4(view==1?pow(max(v,vec3(0)),vec3(1./2.2)):v,1);return;}
 vec3 n=materialNormal(normalMap,worldPosition,worldNormal,uv,1.,frame.options.z);
 vec3 l=normalize(vec3(-.5,.8,1.)),v=normalize(frame.camera.xyz-worldPosition),h=normalize(l+v);
 float rough=clamp(rm.g,.04,1.),metal=rm.b,nl=max(dot(n,l),0.),nh=max(dot(n,h),0.);
 vec3 f0=mix(vec3(.04),base,metal);
 vec3 diffuse=base*(1.-metal)*(.18*rm.r+nl*.9);
 vec3 spec=f0*pow(nh,mix(180.,2.,rough))*(1.-rough*.75)*3.;
 outputColor=vec4(pow(max(diffuse+spec,vec3(0)),vec3(1./2.2)),1);
}
