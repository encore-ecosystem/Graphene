// Same displaced TLAS for direct-light visibility and reflected ray hits.
#ifdef GRAPHENE_RAY_LIGHTING
layout(set=1,binding=0) uniform accelerationStructureEXT lightingScene;
layout(set=1,binding=1,std430) readonly buffer RayLightingConfig {
    vec4 control; // enabled, trace distance, surface bias, reflections
    vec4 sky;
} rayLighting;
float lightingVisibility(vec3 position, vec3 normal, vec3 direction, float maximumDistance) {
    if(rayLighting.control.x<.5) return 1.;
    float bias=rayLighting.control.z;
    rayQueryEXT query;
    rayQueryInitializeEXT(query,lightingScene,gl_RayFlagsOpaqueEXT|gl_RayFlagsTerminateOnFirstHitEXT,
        0xff,position+normal*bias,bias,direction,max(bias,maximumDistance-bias));
    while(rayQueryProceedEXT(query)) {}
    return rayQueryGetIntersectionTypeEXT(query,true)==gl_RayQueryCommittedIntersectionNoneEXT?1.:0.;
}
vec3 lightingReflection(vec3 position, vec3 normal, vec3 direction, float roughness, vec3 fallback) {
    if(rayLighting.control.x<.5 || rayLighting.control.w<.5 || roughness>.9) return fallback;
    float bias=rayLighting.control.z;
    rayQueryEXT query;
    // Reflection needs the nearest hit; first-hit termination is only for shadows.
    rayQueryInitializeEXT(query,lightingScene,gl_RayFlagsOpaqueEXT,
        0xff,position+normal*bias,bias,direction,rayLighting.control.y);
    while(rayQueryProceedEXT(query)) {}
    vec3 radiance=rayLighting.sky.rgb;
    if(rayQueryGetIntersectionTypeEXT(query,true)!=gl_RayQueryCommittedIntersectionNoneEXT) {
        vec3 hit=position+normal*bias+direction*rayQueryGetIntersectionTEXT(query,true);
        float nearest=1e30;uint chosen=0u;bool found=false;
        uint count=min(uint(gi_scene.values[9]),uint(gi_scene.values[0]));
        for(uint i=0u;i<count;++i) {
            uint s=i*12u;if(gi_surfaces.values[s+3u]<.5 || gi_surfaces.values[s+3u]>1.5) continue;
            vec3 p=vec3(gi_surfaces.values[s],gi_surfaces.values[s+1u],gi_surfaces.values[s+2u]);
            float d=dot(p-hit,p-hit);
            if(d<nearest){nearest=d;chosen=i;found=true;}
        }
        radiance=vec3(0);
        if(found) {
            uint s=chosen*12u,h=chosen*4u;
            vec3 albedo=vec3(gi_surfaces.values[s+8u],gi_surfaces.values[s+9u],gi_surfaces.values[s+10u]);
            vec3 bounce=vec3(gi_history.values[h],gi_history.values[h+1u],gi_history.values[h+2u]);
            radiance=albedo*(vec3(.035)+bounce*.82);
        }
    }
    return mix(radiance,fallback,roughness*roughness);
}
#else
float lightingVisibility(vec3 position, vec3 normal, vec3 direction, float maximumDistance) {return 1.;}
vec3 lightingReflection(vec3 position, vec3 normal, vec3 direction, float roughness, vec3 fallback) {return fallback;}
#endif
