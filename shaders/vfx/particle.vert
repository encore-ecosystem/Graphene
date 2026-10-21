#version 450
#extension GL_GOOGLE_include_directive : require
#include "common.glsl"
layout(location=0) out vec2 uv;
layout(location=1) out vec4 tint;
layout(location=2) out vec2 motion;
layout(location=3) out float clipW;
void main(){uint slot=pool[8+capacity()+gl_InstanceIndex];vec2 q=vec2((gl_VertexIndex&1)*2-1,((gl_VertexIndex>>1)&1)*2-1);uv=q*.5+.5;
 vec4 sizeRotation=attr(slot,3);float cs=cos(sizeRotation.w),sn=sin(sizeRotation.w);q=mat2(cs,sn,-sn,cs)*q;
 vec3 pos=attr(slot,0).xyz+(u.control.w>.5?u.origin.xyz:vec3(0));vec3 offset=u.right.xyz*q.x*sizeRotation.x+u.up.xyz*q.y*sizeRotation.y;
 gl_Position=u.vp*vec4(pos+offset,1);clipW=gl_Position.w;vec4 previous=u.vp*vec4(attr(slot,4).xyz+offset+(u.control.w>.5?u.origin.xyz:vec3(0)),1);motion=gl_Position.xy/gl_Position.w-previous.xy/previous.w;tint=attr(slot,2);
}
