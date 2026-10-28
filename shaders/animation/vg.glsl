// Graphene VG skinning ABI. Static variants declare no additional descriptors.
#ifdef GRAPHENE_SKINNED
struct SkinDeformedVertex {vec4 position,normal,tangent,previous_position,previous_normal;};
struct SkinDynamicBound {vec4 center_radius,minimum,maximum;};
layout(set=0,binding=12,std430) readonly buffer SkinVertexCache {SkinDeformedVertex values[];} skin_cache;
layout(set=0,binding=13,std430) readonly buffer SkinConfiguration {
    uvec4 geometry; // source vertex count, bone count, morph count, frame epoch
    uvec4 scene; // maximum instances, cluster count, hierarchy count, active instances
    uvec4 instances[]; // x=history valid; remaining reserved
} skin_config;
layout(set=0,binding=14,std430) readonly buffer SkinBounds {SkinDynamicBound values[];} skin_bounds;
uint animation_instance;
vec4 skinNodeSphere(uint instance,uint node) {
    return skin_bounds.values[instance*(skin_config.scene.y+skin_config.scene.z)+skin_config.scene.y+node].center_radius;
}
#endif
