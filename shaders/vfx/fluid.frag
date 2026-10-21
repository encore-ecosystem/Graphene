#version 450
#extension GL_GOOGLE_include_directive : require
#include "fluid_common.glsl"
layout(location=0) in vec2 uv;
layout(location=0) out vec4 color;
layout(location=1) out vec4 outputMotion;
vec3 blackbody(float heat){return mix(vec3(.7,.025,.002),mix(vec3(1,.35,.02),vec3(1,1,.7),clamp(heat-1,0,1)),clamp(heat,0,1))*heat;}
void main(){vec2 ndc=uv*2-1;vec3 ray=normalize(f.forward.xyz+f.right.xyz*ndc.x*f.render.x*f.render.y-f.up.xyz*ndc.y*f.render.x);vec3 ro=f.camera.xyz-f.origin.xyz,extent=f.extent.xyz;if(uint(f.settings.w)==2)extent.y=max(extent.y,2);vec3 inv=1/(ray+vec3(1e-10));vec3 a=-ro*inv,b=(extent-ro)*inv,low=min(a,b),high=max(a,b);float near=max(0,max(low.x,max(low.y,low.z))),far=min(high.x,min(high.y,high.z));if(far<=near)discard;
 float stepSize=max(f.grid.w*.6,(far-near)/192),transmittance=1;vec3 radiance=vec3(0);uint kind=uint(f.settings.w);
 for(int i=0;i<192;i++){float t=near+(float(i)+.5)*stepSize;if(t>=far||transmittance<.01)break;vec3 p=ro+ray*t;vec3 q=p/f.grid.w-.5;
  if(kind==2){q=vec3(p.x/f.grid.w-.5,p.z/f.grid.w-.5,0);float height=sampleState(q,0).w;if(p.y>height)continue;}
  vec4 state=sampleState(q,0),thermal=sampleState(q,1);float density=kind==2?1:max(0,state.w);if(f.render.w>=0&&abs(q.z-f.render.w)>1)continue;if(density<.0001)continue;
  float alpha=1-exp(-density*stepSize*f.render.z);vec3 light=vec3(.15,.2,.28)+vec3(.8,.72,.55)*exp(-sampleState(q+vec3(2,3,1),0).w*.2);vec3 sampleColor=f.color.rgb*light+blackbody(thermal.x)*max(0,thermal.y+thermal.x*.1);
  if(kind==1||kind==2){vec3 gradient=vec3(sampleState(q+vec3(1,0,0),0).w-sampleState(q-vec3(1,0,0),0).w,sampleState(q+vec3(0,1,0),0).w-sampleState(q-vec3(0,1,0),0).w,sampleState(q+vec3(0,0,1),0).w-sampleState(q-vec3(0,0,1),0).w);vec3 normal=normalize(-gradient+vec3(.00001));sampleColor=mix(vec3(.015,.15,.22),vec3(.5,.75,.9),pow(1-abs(dot(normal,ray)),5));sampleColor+=pow(max(0,dot(reflect(ray,normal),normalize(vec3(1,2,1)))),32)*2;}
  radiance+=transmittance*alpha*sampleColor;transmittance*=1-alpha;
 }
 if(transmittance>.9999)discard;color=vec4(radiance/max(.0001,1-transmittance),1-transmittance);outputMotion=vec4(0,0,1,0);
}
