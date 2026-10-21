#version 450
layout(location=0)in vec2 uv;
layout(location=0)out vec4 outputColor;
layout(set=0,binding=0)uniform Environment{vec4 settings[32];vec4 camera;vec4 forward;vec4 right;vec4 up;}env;
layout(set=0,binding=3)uniform sampler2D transmittanceLut;
layout(set=0,binding=4)uniform sampler2D multipleScatteringLut;
#include "environment_atmosphere.glsl"
void main(){
#ifdef GRAPHENE_TRANSMITTANCE_LUT
    vec3 p=vec3(0.,ATM_GROUND+max(uv.y*uv.y*100.,.001),0.);
    float mu=uv.x*2.-1.;vec3 ray=vec3(sqrt(max(1.-mu*mu,0.)),mu,0.);
    float distance=atmosphereDistance(p,ray);vec3 optical=vec3(0.);
    for(int i=0;i<40;++i){vec3 r,m,e;atmosphereMedium(p+ray*(float(i)+.5)*(distance/40.),r,m,e);optical+=e*(distance/40.);}
    outputColor=vec4(exp(-optical),1.);
#elif defined(GRAPHENE_MULTIPLE_SCATTERING_LUT)
    vec3 p=vec3(0.,ATM_GROUND+max(uv.y*uv.y*100.,.001),0.);
    float mu=uv.x*2.-1.;vec3 sun=vec3(sqrt(max(1.-mu*mu,0.)),mu,0.);
    vec3 integrated=vec3(0.),feedback=vec3(0.);
    for(int s=0;s<32;++s){
        float z=1.-2.*(float(s)+.5)/32.,phi=float(s)*2.39996323;
        vec3 ray=vec3(cos(phi)*sqrt(1.-z*z),z,sin(phi)*sqrt(1.-z*z));
        float distance=atmosphereDistance(p,ray),dt=distance/16.;vec3 through=vec3(1.);
        for(int i=0;i<16;++i){
            vec3 q=p+ray*(float(i)+.5)*dt,r,m,e;atmosphereMedium(q,r,m,e);
            vec3 segment=exp(-e*dt),weight=through*(1.-segment)/max(e,vec3(1e-6));
            integrated+=weight*(r+m)*atmosphereSun(q,sun)/(4.*ATM_PI*32.);
            feedback+=weight*(r+m)/32.;through*=segment;
        }
    }
    outputColor=vec4(integrated/max(vec3(1.)-min(feedback,vec3(.95)),vec3(.05)),1.);
#else
    float azimuth=(uv.x-.5)*2.*ATM_PI;
    float elevation=(uv.y-.5)*ATM_PI;
    vec3 ray=vec3(cos(elevation)*cos(azimuth),sin(elevation),cos(elevation)*sin(azimuth));
    vec3 p=vec3(0.,ATM_GROUND+max(env.camera.y*.001,.002),0.);
    outputColor=vec4(integrateSky(p,ray,normalize(env.settings[4].xyz)),1.);
#endif
}
