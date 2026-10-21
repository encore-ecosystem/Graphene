#version 450
layout(set=0,binding=0) uniform Environment {vec4 settings[528];vec4 camera;vec4 forward;vec4 right;vec4 up;mat4 vp;vec4 shelter;} env;
layout(set=0,binding=1) uniform sampler2D shelterMap;
layout(location=0) out vec2 uv;
layout(location=1) flat out float snow;
layout(location=2) out float opacity;
layout(location=3) out vec3 worldPosition;
#define ENV(i) env.settings[i]
#include "environment_weather.glsl"
float hash(float n){return fract(sin(n*127.1+311.7)*43758.5453);}
void main(){
    float id=float(gl_InstanceIndex);
    snow=step(env.settings[1].y,env.settings[1].z);
    float intensity=max(env.settings[1].y,env.settings[1].z);
    float speed=mix(24.,2.2,snow),t=env.settings[0].w;
    vec3 seed=vec3(hash(id+1.),hash(id+83.),hash(id+173.));
    vec3 region=env.camera.xyz+vec3(seed.x*48.-24.,seed.y*24.-12.,seed.z*48.-24.);
    EnvironmentWeather weather=environmentWeather(region);
    snow=step(weather.sky.y,weather.sky.z);intensity=max(weather.sky.y,weather.sky.z);
    speed=mix(24.,2.2,snow);
    vec3 p=vec3(seed.x*48.-24.,12.-mod(seed.y*24.+t*speed,24.),seed.z*48.-24.);
    p.xz+=weather.wind.xz*t;
    p.xz=mod(p.xz-env.camera.xz+24.,48.)-24.+env.camera.xz;
    p.y+=floor(env.camera.y/8.)*8.;
    p.xz+=sin(vec2(id*.7+t,id*1.3+t*.6))*.35*snow;
    vec2 q=vec2((gl_VertexIndex&1),((gl_VertexIndex>>1)&1));uv=q;
    float width=mix(.009,.035,snow),streak=mix(.65,.07,snow);
    vec3 side=env.right.xyz*width;
    vec3 vertical=mix(normalize(vec3(-weather.wind.x*.025,1.,-weather.wind.z*.025)),env.up.xyz,snow)*streak;
    vec3 world=p+side*(q.x*2.-1.)+vertical*(q.y*2.-1.);
    weather=environmentWeather(p);snow=step(weather.sky.y,weather.sky.z);intensity=max(weather.sky.y,weather.sky.z);
    vec2 mapUV=(p.xz-env.shelter.xy)/env.shelter.z+.5;
    float roof=textureLod(shelterMap,mapUV,0.).r;
    opacity=intensity*(1.-smoothstep(18.,24.,length(p.xz-env.camera.xz)));
    if(all(greaterThanEqual(mapUV,vec2(0))) && all(lessThanEqual(mapUV,vec2(1))) && p.y<roof+.02)opacity=0.;
    worldPosition=world;gl_Position=env.vp*vec4(world,1.);
}
