// Reserved actor-light tail, identical for conventional, mesh and triangle paths.
#define ENV(i) actor_lighting[257+(i)]
#include "environment_weather.glsl"
#ifdef GRAPHENE_ENVIRONMENT_SHELTER
#ifndef GRAPHENE_SHELTER_BINDING
#define GRAPHENE_SHELTER_BINDING 7
#endif
layout(set=0,binding=GRAPHENE_SHELTER_BINDING) uniform sampler2D environmentShelter;
#endif
float environmentExposure(vec3 position) {
#ifdef GRAPHENE_ENVIRONMENT_SHELTER
    if(ENV(8).w>.5) {
        vec2 uv=(position.xz-ENV(8).xy)/ENV(8).z+.5;
        if(all(greaterThanEqual(uv,vec2(0))) && all(lessThanEqual(uv,vec2(1)))) {
            float height=texture(environmentShelter,uv).r;
            return position.y>=height-.12?1.:0.;
        }
    }
#endif
    return 1.;
}
float environmentCloudShadow(vec3 position) {
    if(ENV(0).x<.5 || ENV(5).w<=0.) return 1.;
    vec3 sun=ENV(4).xyz;
    vec2 p=position.xz+sun.xz*(ENV(5).y-position.y)/max(sun.y,.1)-ENV(2).xz*ENV(0).w;
    float clouds=sin(p.x*.0018+sin(p.y*.0011))*sin(p.y*.0015)+sin(p.x*.0037+p.y*.0029)*.35;
    return 1.-smoothstep(1.-ENV(1).x*2.,1.5-ENV(1).x*2.,clouds)*ENV(1).x*.55;
}
void environmentSurface(vec3 position, vec3 normal, inout vec3 base, inout float roughness) {
    if(ENV(0).x<.5) return;
    EnvironmentWeather weather=environmentWeather(position);
    if(weather.surface.x<=0. && weather.surface.y<=0.)return;
    float upward=smoothstep(.25,.85,normal.y);
    float exposure=environmentExposure(position);
    float wet=weather.surface.x*exposure;
    base*=1.-wet*.32;
    roughness=mix(roughness,max(.05,roughness*.3),wet);
    float snow=weather.surface.y*upward*exposure;
    base=mix(base,vec3(.88,.93,.99),snow);
    roughness=mix(roughness,.85,snow);
}
vec3 environmentFog(vec3 position, vec3 camera, vec3 radiance) {
    if(ENV(0).x<.5) return radiance;
    float distance=length(position-camera);
    float height=(position.y+camera.y)*.5-ENV(6).y;
    float density=ENV(6).x*exp(clamp(-height/max(ENV(6).z,1.),-15.,15.));
    density+=environmentWeather(position).sky.w*.0005;
    float visibility=exp(-distance*density);
    float day=smoothstep(-.1,.2,ENV(4).y);
    vec3 fog=mix(vec3(.012,.018,.035),vec3(.48,.57,.68),day);
    float sunGlow=pow(max(dot(normalize(position-camera),ENV(4).xyz),0.),12.);
    fog+=vec3(.3,.2,.1)*sunGlow*day;
    return radiance*visibility+fog*(1.-visibility)+ENV(3).z*vec3(.3,.35,.5);
}
