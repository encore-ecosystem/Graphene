#version 450
// Graphene environment: linear HDR radiance, independent from surface shaders.
layout(location=0) in vec2 uv;
layout(location=0) out vec4 outputColor;
layout(location=1) out vec4 outputMotion;
layout(set=0,binding=0) uniform Environment {
    vec4 settings[32];
    vec4 camera;
    vec4 forward;
    vec4 right;
    vec4 up;
} env;
#if defined(GRAPHENE_SKY_GRAPH) || defined(GRAPHENE_VOLUME_GRAPH)
#include "material_graph_generated.glsl"
#endif
layout(set=0,binding=1) uniform sampler2D panorama;
layout(set=0,binding=2) uniform sampler2D cloudHistory;
layout(set=0,binding=3) uniform sampler2D transmittanceLut;
layout(set=0,binding=4) uniform sampler2D multipleScatteringLut;
layout(set=0,binding=5) uniform sampler2D skyViewLut;
layout(set=0,binding=6) uniform sampler2D cloudDepth;
#include "environment_atmosphere.glsl"
float hash(vec3 p) { p=fract(p*.1031); p+=dot(p,p.yzx+33.33); return fract((p.x+p.y)*p.z); }
float noise(vec3 p) {
    vec3 i=floor(p),f=fract(p); f=f*f*(3.-2.*f);
    return mix(mix(mix(hash(i),hash(i+vec3(1,0,0)),f.x),mix(hash(i+vec3(0,1,0)),hash(i+vec3(1,1,0)),f.x),f.y),
        mix(mix(hash(i+vec3(0,0,1)),hash(i+vec3(1,0,1)),f.x),mix(hash(i+vec3(0,1,1)),hash(i+vec3(1)),f.x),f.y),f.z);
}
float fbm(vec3 p) { return noise(p)*.57+noise(p*2.03)*.28+noise(p*4.11)*.15; }
float cloudDensity(vec3 p) {
#ifdef GRAPHENE_VOLUME_GRAPH
    mg_position=p;mg_direction=vec3(0,1,0);mg_uv=p.xz;
    for(int i=0;i<8;++i)mg_environment[i]=env.settings[i];
    vec3 scattering,emission;float d;evaluateVolume(scattering,d,emission);return max(d,0.);
#endif
    vec4 layer=env.settings[5];
    float h=(p.y-layer.y)/max(layer.z,1.);
    float profile=smoothstep(0.,.15,h)*(1.-smoothstep(.65,1.,h));
    vec3 advect=env.settings[2].xyz*env.settings[0].w;
    float shape=fbm((p-advect)*.0006);
    float coverage=env.settings[1].x;
    float threshold=mix(.72,.24,coverage);
    float density=smoothstep(threshold,threshold+.16,shape)*profile*layer.x;
    if(env.settings[0].y>.5) density=smoothstep(.12,.35,density)*.7;
    return density;
}
vec3 skyRadiance(vec3 ray, vec3 sun) {
    float daylight=smoothstep(-.12,.18,sun.y);
    float horizon=pow(1.-max(ray.y,0.),3.);
    float mu=dot(ray,sun);
    vec2 skyUv=vec2(atan(ray.z,ray.x)/(2.*ATM_PI)+.5,asin(clamp(ray.y,-1.,1.))/ATM_PI+.5);
    vec3 day=texture(skyViewLut,skyUv).rgb;
    vec3 night=mix(vec3(.002,.004,.012),vec3(.012,.017,.035),horizon);
    vec3 sunset=vec3(.65,.17,.035)*horizon*exp(-abs(sun.y)*9.)*(.25+.75*max(mu,0.));
    vec3 sky=night+day;
    if(env.settings[0].y>.5) {
        vec3 zenith=mix(vec3(.013,.018,.06),vec3(.12,.35,.68),daylight);
        vec3 edge=mix(vec3(.07,.04,.13),vec3(.65,.83,.94),daylight);
        sky=mix(zenith,edge,smoothstep(.02,1.,horizon))+sunset*.6;
    }
    float disk=smoothstep(cos(.0048),cos(.0043),mu)*daylight*env.settings[8].y;
    sky+=vec3(1.,.84,.61)*disk*12.;
    vec3 moon=-sun;
    float moonDisk=smoothstep(cos(.006),cos(.0055),dot(ray,moon))*(1.-daylight)*env.settings[8].z;
    sky+=vec3(.55,.65,.9)*moonDisk*1.4;
    if(daylight<.3 && ray.y>0.) {
        vec3 cell=floor(ray*700.);
        float star=step(.9993,hash(cell))*pow(max(hash(cell+13.),0.),6.);
        sky+=star*(1.-daylight)*.8;
    }
    return max(sky,vec3(0));
}
void main() {
    vec2 screen=uv*2.-1.;
    vec3 ray=normalize(env.forward.xyz+env.right.xyz*screen.x-env.up.xyz*screen.y);
    vec3 sun=normalize(env.settings[4].xyz);
    vec3 sky=skyRadiance(ray,sun);
    if(env.settings[0].y>1.5 && env.settings[8].x>.5) {
        vec2 spherical=vec2(atan(ray.z,ray.x)*.15915494+.5,acos(clamp(ray.y,-1.,1.))*.31830989);
        sky=texture(panorama,spherical).rgb;
    }
#ifdef GRAPHENE_SKY_GRAPH
    mg_position=env.camera.xyz+ray*1000.;mg_direction=ray;mg_uv=uv;
    for(int i=0;i<8;++i)mg_environment[i]=env.settings[i];
    float unusedMetal,unusedRough;evaluateMaterial(sky,unusedMetal,unusedRough);
#endif
    float transmittance=1.;
    float cloudDistance=40000.;
    vec3 cloudRadiance=vec3(0);
    if(env.settings[5].w>0. && env.settings[1].x>.01 && abs(ray.y)>.003) {
        float bottom=env.settings[5].y, top=bottom+env.settings[5].z;
        float begin=max(0.,min((bottom-env.camera.y)/ray.y,(top-env.camera.y)/ray.y));
        float end=max((bottom-env.camera.y)/ray.y,(top-env.camera.y)/ray.y);
        end=min(end,40000.);
        if(end>begin) {
            cloudDistance=(begin+end)*.5;
            int steps=env.settings[6].w<.5?16:(env.settings[6].w<1.5?32:48);
            float stepSize=(end-begin)/float(steps);
            vec3 accumulated=vec3(0);
            for(int i=0;i<48;++i) {
                if(i>=steps || transmittance<.02) break;
                vec3 p=env.camera.xyz+ray*(begin+(float(i)+.5)*stepSize);
                float density=cloudDensity(p);
                if(density<.001) continue;
                float extinction=1.-exp(-density*stepSize*.012);
                float shadow=exp(-cloudDensity(p+sun*150.)*1.8-cloudDensity(p+sun*400.)*2.4);
                float daylight=smoothstep(-.12,.2,sun.y);
                vec3 light=mix(vec3(.035,.045,.08),mix(vec3(.26,.3,.37),vec3(1.,.95,.87),shadow),daylight);
                if(env.settings[0].y>.5) light=mix(vec3(.36,.4,.55),vec3(.98,.97,.91),step(.4,shadow))*max(daylight,.18);
#ifdef GRAPHENE_VOLUME_GRAPH
                mg_position=p;mg_direction=ray;mg_uv=p.xz;
                for(int k=0;k<8;++k)mg_environment[k]=env.settings[k];
                vec3 customScatter,customEmission;float customDensity;evaluateVolume(customScatter,customDensity,customEmission);
                light=light*customScatter+customEmission;
#endif
                accumulated+=transmittance*extinction*light*env.settings[5].w;
                transmittance*=1.-extinction;
            }
            sky=sky*transmittance+accumulated;cloudRadiance=accumulated;
        }
    }
    float haze=env.settings[1].w*(1.-smoothstep(-.05,.3,ray.y));
    sky=mix(sky,mix(vec3(.17,.22,.29),vec3(.5,.57,.63),smoothstep(-.1,.2,sun.y)),haze*.65);
    sky+=env.settings[3].z*vec3(.65,.72,1.);
#ifdef GRAPHENE_CLOUD_ONLY
    vec4 current=vec4(cloudRadiance*env.settings[3].w,1.-transmittance);
    // Reprojecting a slab midpoint cannot represent parallax through a volume.
    // Accumulate only at a stationary view; camera movement always uses this frame.
    bool stationary=length(env.camera.xyz-env.settings[10].xyz)<.0001 &&
        length(env.forward.xyz-env.settings[11].xyz)<.00001 &&
        length(env.right.xyz-env.settings[12].xyz)<.00001 &&
        length(env.up.xyz-env.settings[13].xyz)<.00001;
    if(env.settings[10].w>.5 && stationary && cloudDistance<40000.) {
        vec3 previousRay=env.camera.xyz+ray*cloudDistance-env.settings[10].xyz-env.settings[2].xyz*env.settings[14].w;
        vec3 oldForward=env.settings[11].xyz,oldRight=env.settings[12].xyz,oldUp=env.settings[13].xyz;
        float depth=dot(previousRay,oldForward);
        vec2 previousUv=vec2(dot(previousRay,oldRight)/max(dot(oldRight,oldRight)*depth,.001),-dot(previousRay,oldUp)/max(dot(oldUp,oldUp)*depth,.001))*.5+.5;
        if(depth>0. && all(greaterThan(previousUv,vec2(0.))) && all(lessThan(previousUv,vec2(1.)))) {
            vec4 history=texture(cloudHistory,previousUv);
            float oldDepth=texture(cloudDepth,previousUv).x*40000.;
            float trust=exp(-abs(oldDepth-cloudDistance)/max(cloudDistance*.02,1.));
            // Transparent pixels reject history entirely, preventing silhouette trails.
            float alphaTrust=1.-smoothstep(.015,.08,abs(history.a-current.a));
            float lightTrust=1.-smoothstep(.03,.15,length(history.rgb-current.rgb));
            history=clamp(history,current-vec4(.025),current+vec4(.025));
            current=mix(current,history,.65*trust*alphaTrust*lightTrust);
        }
    }
    outputColor=current;
    outputMotion=vec4(cloudDistance/40000.,0,0,0);
#else
    outputColor=vec4(sky*env.settings[3].w,1.);
#endif
#ifndef GRAPHENE_CLOUD_ONLY
    outputMotion=vec4(0,0,1,0);
#endif
}
