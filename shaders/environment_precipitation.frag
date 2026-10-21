#version 450
layout(location=0) in vec2 uv;
layout(location=1) flat in float snow;
layout(location=2) in float opacity;
layout(location=3) in vec3 worldPosition;
layout(location=0) out vec4 outputColor;
layout(location=1) out vec4 outputMotion;
#ifdef GRAPHENE_PARTICLE_GRAPH
layout(set=0,binding=0) uniform Environment {vec4 settings[528];vec4 camera;vec4 forward;vec4 right;vec4 up;mat4 vp;vec4 shelter;} env;
#include "material_graph_generated.glsl"
#endif
void main(){
    vec2 p=uv*2.-1.;float shape=mix(pow(max(1.-p.x*p.x,0.),2.)*(1.-abs(p.y)),1.-smoothstep(.35,1.,dot(p,p)),snow);
    float alpha=opacity*shape*mix(.28,.8,snow);if(alpha<.005)discard;
    vec3 particleColor=mix(vec3(.48,.62,.76),vec3(.85,.92,1.),snow);
#ifdef GRAPHENE_PARTICLE_GRAPH
    mg_position=worldPosition;mg_direction=normalize(worldPosition-env.camera.xyz);mg_uv=uv;
    for(int i=0;i<8;++i)mg_environment[i]=env.settings[i];
    float graphOpacity,unused;evaluateMaterial(particleColor,graphOpacity,unused);alpha*=graphOpacity;
#endif
    outputColor=vec4(particleColor,alpha);
    outputMotion=vec4(0,0,1,0);
}
