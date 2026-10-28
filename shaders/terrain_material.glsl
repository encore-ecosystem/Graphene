#ifndef GRAPHENE_TERRAIN_MATERIAL
#define GRAPHENE_TERRAIN_MATERIAL
// Layer zero reuses the ordinary PBR bindings (5/6/8/31). Keeping the complete
// terrain at fourteen sampled images also fits Vulkan's sixteen-image minimum.
layout(set=0,binding=35) uniform sampler2D terrainBase1;
layout(set=0,binding=36) uniform sampler2D terrainNormal1;
layout(set=0,binding=37) uniform sampler2D terrainOrm1;
layout(set=0,binding=38) uniform sampler2D terrainHeight1;
layout(set=0,binding=39) uniform sampler2D terrainBase2;
layout(set=0,binding=40) uniform sampler2D terrainNormal2;
layout(set=0,binding=41) uniform sampler2D terrainOrm2;
layout(set=0,binding=42) uniform sampler2D terrainHeight2;
layout(set=0,binding=43) uniform sampler2D terrainWeights;
layout(set=0,binding=44,std140) uniform TerrainMaterial {
    vec4 geometry; // local heightfield extent, reserved
    vec4 tint[3]; // base color, signed normal scale (negative = DirectX Y)
    vec4 surface[3]; // metallic, roughness, unlit, reserved
    vec4 mapping[3]; // world tile size, local displacement magnitude/center, enabled
    vec4 flags[3]; // base/normal/roughness/metallic maps present
} terrainMaterial;
vec3 terrainBlend(vec2 uv) {
    vec2 size=vec2(textureSize(terrainWeights,0));
    vec2 sampleUV=(clamp(uv,0.,1.)*(size-1.)+.5)/size;
    vec3 w=max(textureLod(terrainWeights,sampleUV,0).rgb,vec3(0));
    return w/max(dot(w,vec3(1)),1e-8);
}
vec2 terrainLayerUV(vec2 worldXZ,uint layer) {return worldXZ/max(terrainMaterial.mapping[layer].x,.0001);}
#ifdef GRAPHENE_TERRAIN_HEIGHT
float terrainOffset(vec2 uv,vec2 worldXZ) {
    vec3 weights=terrainBlend(uv);
    vec3 height=vec3(textureLod(patchHeight,terrainLayerUV(worldXZ,0u),0).r,
                     textureLod(terrainHeight1,terrainLayerUV(worldXZ,1u),0).r,
                     textureLod(terrainHeight2,terrainLayerUV(worldXZ,2u),0).r);
    float value=0.;
    for(uint i=0u;i<3u;++i) {
        vec4 m=terrainMaterial.mapping[i];
        value+=weights[i]*(height[i]-m.z)*m.y*m.w;
    }
    return value;
}
vec2 terrainHeightStep() {
    vec2 density=vec2(textureSize(terrainWeights,0));
    density=max(density,vec2(textureSize(patchHeight,0))*terrainMaterial.geometry.x/max(terrainMaterial.mapping[0].x,.0001));
    density=max(density,vec2(textureSize(terrainHeight1,0))*terrainMaterial.geometry.x/max(terrainMaterial.mapping[1].x,.0001));
    density=max(density,vec2(textureSize(terrainHeight2,0))*terrainMaterial.geometry.x/max(terrainMaterial.mapping[2].x,.0001));
    return 1./max(density,vec2(1));
}
#endif
#ifdef GRAPHENE_TERRAIN_FRAGMENT
vec3 terrainAlbedo(vec2 uv,vec2 worldXZ) {
    vec3 w=terrainBlend(uv);
    vec3 a=mix(vec3(1),texture(base_color_texture,terrainLayerUV(worldXZ,0u)).rgb,terrainMaterial.flags[0].x);
    vec3 b=mix(vec3(1),texture(terrainBase1,terrainLayerUV(worldXZ,1u)).rgb,terrainMaterial.flags[1].x);
    vec3 c=mix(vec3(1),texture(terrainBase2,terrainLayerUV(worldXZ,2u)).rgb,terrainMaterial.flags[2].x);
    return a*terrainMaterial.tint[0].rgb*w.x+b*terrainMaterial.tint[1].rgb*w.y+c*terrainMaterial.tint[2].rgb*w.z;
}
vec4 terrainSurface(vec2 uv,vec2 worldXZ) {
    vec3 w=terrainBlend(uv);
    vec3 orm[3]=vec3[3](texture(material_orm,terrainLayerUV(worldXZ,0u)).rgb,
                           texture(terrainOrm1,terrainLayerUV(worldXZ,1u)).rgb,
                           texture(terrainOrm2,terrainLayerUV(worldXZ,2u)).rgb);
    vec4 result=vec4(0);
    for(uint i=0u;i<3u;++i) {
        vec4 s=terrainMaterial.surface[i],f=terrainMaterial.flags[i];
        result+=w[i]*vec4(s.x*mix(1.,orm[i].b,f.w),s.y*mix(1.,orm[i].g,f.z),orm[i].r,s.z);
    }
    return result;
}
vec3 terrainShadingNormal(vec2 uv,vec3 position,vec3 normal) {
    vec3 w=terrainBlend(uv);float scales[3];
    for(uint i=0u;i<3u;++i)scales[i]=terrainMaterial.tint[i].w*terrainMaterial.flags[i].y;
    vec3 a=materialNormal(normal_texture,position,normal,terrainLayerUV(position.xz,0u),abs(scales[0]),scales[0]<0.?-1.:1.);
    vec3 b=materialNormal(terrainNormal1,position,normal,terrainLayerUV(position.xz,1u),abs(scales[1]),scales[1]<0.?-1.:1.);
    vec3 c=materialNormal(terrainNormal2,position,normal,terrainLayerUV(position.xz,2u),abs(scales[2]),scales[2]<0.?-1.:1.);
    return normalize(a*w.x+b*w.y+c*w.z);
}
#endif
#endif
