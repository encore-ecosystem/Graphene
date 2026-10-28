#ifndef GRAPHENE_DISPLACEMENT_COMMON
#define GRAPHENE_DISPLACEMENT_COMMON
// Procedural microgeometry: only patchIndex jobs and shared barycentric templates
// reside in memory. No expanded scene-sized vertex buffer is generated.
layout(set=0,binding=20,std140) uniform PatchFrame {
    mat4 viewProjection, previousViewProjection;
    vec4 camera, viewport; // viewport: width, height, target edge pixels, normal flip
    vec4 displacement; // magnitude, center, UV tiling, height range
    uvec4 geometry; // triangle count, instance count, job capacity, max level
    uvec4 control; // phase, input parity, reserved, skinned source vertex count
} patchFrame;
layout(set=0,binding=21,std430) readonly buffer PatchVertices { float data[]; } patchVertices;
layout(set=0,binding=22,std430) readonly buffer PatchIndices { uint data[]; } patchIndices;
layout(set=0,binding=23,std430) readonly buffer PatchNeighbours { uint data[]; } patchNeighbours;
layout(set=0,binding=24,std430) readonly buffer PatchPatterns { uvec4 data[]; } patchPatterns;
layout(set=0,binding=25,std430) readonly buffer PatchRanges { uvec2 data[]; } patchRanges;
layout(set=0,binding=26,std430) buffer PatchLevels { uint data[]; } patchLevels;
layout(set=0,binding=27,std430) buffer PatchOffsets { uvec4 data[]; } patchOffsets;
layout(set=0,binding=28,std430) buffer PatchBlocks { uvec2 data[]; } patchBlocks;
layout(set=0,binding=29,std430) buffer PatchJobs { uvec4 data[]; } patchJobs;
layout(set=0,binding=30,std430) buffer PatchState { uint data[]; } patchState;
layout(set=0,binding=31) uniform sampler2D patchHeight;
struct PatchSkinVertex { vec4 position, normal, tangent, previousPosition, previousNormal; };
layout(set=0,binding=32,std430) readonly buffer PatchSkin { PatchSkinVertex data[]; } patchSkin;
layout(set=0,binding=33,std430) readonly buffer PatchWeld { uvec2 data[]; } patchWeld;
struct PatchInstance {
    mat4 model, previousModel;
    vec4 tint, surface, textureFlags;
    vec4 geometry; // first triangle, triangle count, skin instance, history valid
};
layout(set=0,binding=34,std430) readonly buffer PatchInstances { PatchInstance data[]; } patchInstances;

#ifdef GRAPHENE_TERRAIN
#define GRAPHENE_TERRAIN_HEIGHT
#include "../terrain_material.glsl"
#endif
uint patchCount() { return patchFrame.geometry.x * patchFrame.geometry.y; }
uint patchFace(uint patchIndex) { return patchIndex % patchFrame.geometry.x; }
uint patchInstance(uint patchIndex) { return patchIndex / patchFrame.geometry.x; }
uvec3 patchCorners(uint face) { return uvec3(patchIndices.data[face*3],patchIndices.data[face*3+1],patchIndices.data[face*3+2]); }
vec3 sourcePosition(uint vertex, uint instance, bool previous) {
    if(patchFrame.control.w!=0) {
        uint i=uint(patchInstances.data[instance].geometry.z)*patchFrame.control.w+uint(patchVertices.data[vertex*11+6]);
        return previous?patchSkin.data[i].previousPosition.xyz:patchSkin.data[i].position.xyz;
    }
    return vec3(patchVertices.data[vertex*11],patchVertices.data[vertex*11+1],patchVertices.data[vertex*11+2]);
}
vec3 sourceNormal(uint vertex, uint instance, bool previous) {
    if(patchFrame.control.w!=0) {
        uint i=uint(patchInstances.data[instance].geometry.z)*patchFrame.control.w+uint(patchVertices.data[vertex*11+6]);
        return previous?patchSkin.data[i].previousNormal.xyz:patchSkin.data[i].normal.xyz;
    }
    return vec3(patchVertices.data[vertex*11+3],patchVertices.data[vertex*11+4],patchVertices.data[vertex*11+5]);
}
vec2 sourceUV(uint vertex) { return vec2(patchVertices.data[vertex*11+9],patchVertices.data[vertex*11+10]); }
vec2 faceUV(uvec3 corners, vec3 b) { return (sourceUV(corners.x)*b.x+sourceUV(corners.y)*b.y+sourceUV(corners.z)*b.z)*patchFrame.displacement.z; }
vec3 rawOffset(uvec3 c, vec3 b, uint instance, bool previous) {
    vec3 normal=sourceNormal(c.x,instance,previous)*b.x+sourceNormal(c.y,instance,previous)*b.y+sourceNormal(c.z,instance,previous)*b.z;
#ifdef GRAPHENE_TERRAIN
    vec3 position=sourcePosition(c.x,instance,previous)*b.x+sourcePosition(c.y,instance,previous)*b.y+sourcePosition(c.z,instance,previous)*b.z;
    mat4 model=previous?patchInstances.data[instance].previousModel:patchInstances.data[instance].model;
    return normalize(normal)*terrainOffset(faceUV(c,b),(model*vec4(position,1)).xz);
#else
    float height=textureLod(patchHeight,faceUV(c,b),0).r;
    return normalize(normal)*((height-patchFrame.displacement.y)*patchFrame.displacement.x);
#endif
}
vec3 cornerOffset(uint vertex, uint instance, bool previous) {
    uint head=patchWeld.data[vertex].x, current=head;
    vec3 sum=vec3(0);uint count=0;
    do {
        vec3 n=normalize(sourceNormal(current,instance,previous));
#ifdef GRAPHENE_TERRAIN
        mat4 model=previous?patchInstances.data[instance].previousModel:patchInstances.data[instance].model;
        sum+=n*terrainOffset(sourceUV(current),(model*vec4(sourcePosition(current,instance,previous),1)).xz);
#else
        sum+=n*((textureLod(patchHeight,sourceUV(current)*patchFrame.displacement.z,0).r-patchFrame.displacement.y)*patchFrame.displacement.x);
#endif
        current=patchWeld.data[current].y;++count;
    } while(current!=0xffffffffu && count<256u);
    return sum/max(float(count),1.);
}
vec3 surfacePosition(uint face, vec3 b, uint instance, bool previous) {
    uvec3 c=patchCorners(face);
    vec3 p=sourcePosition(c.x,instance,previous)*b.x+sourcePosition(c.y,instance,previous)*b.y+sourcePosition(c.z,instance,previous)*b.z;
    for(uint k=0;k<3;++k) if(b[k]>.999999) return p+cornerOffset(c[k],instance,previous);
    vec3 offset=rawOffset(c,b,instance,previous);
    for(uint edge=0;edge<3;++edge) if(b[edge]<.000001) {
        uint other=patchNeighbours.data[face*3+edge];
        if(other==0xffffffffu) return p+offset;
        uint otherFace=other/3,otherEdge=other%3;uvec3 d=patchCorners(otherFace);
        uint a=(edge+1)%3,z=(edge+2)%3;
        vec3 q=vec3(0);uint da=(otherEdge+1)%3,dz=(otherEdge+2)%3;
        bool same=patchWeld.data[c[a]].x==patchWeld.data[d[da]].x;
        q[da]=same?b[a]:b[z];q[dz]=same?b[z]:b[a];
        offset=(offset+rawOffset(d,q,instance,previous))*.5;
        // Match shared endpoints even at corners with more than two incident faces.
        for(uint k=0;k<2;++k) {
            uint ca=k==0?a:z,di=(same==(k==0))?da:dz;
            vec3 one=vec3(0),two=vec3(0);one[ca]=1;two[di]=1;
            offset+=(cornerOffset(c[ca],instance,previous)-(rawOffset(c,one,instance,previous)+rawOffset(d,two,instance,previous))*.5)*b[ca];
        }
        break;
    }
    return p+offset;
}
vec3 patternBarycentric(uint packed, uint denominator) {
    vec2 uv=vec2(packed&65535u,packed>>16u)/float(denominator);return vec3(1.-uv.x-uv.y,uv);
}
#endif
