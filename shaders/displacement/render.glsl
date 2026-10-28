#include "common.glsl"
uint displacedTriangleIdentity(uint jobIndex,uint vertex) {
    uvec4 job=patchJobs.data[jobIndex];
    return patchFace(job.x)*65537u+(job.z+vertex/3u)+patchInstance(job.x)*16777619u;
}
struct DisplacedVertex {
    vec4 clip, previousClip;
    vec3 position, normal, color;
    vec2 uv;
    uint instance;
    bool valid;
};
vec3 displacedNormal(uint face, vec3 b, uint instance) {
    uvec3 c=patchCorners(face);
    vec3 n=normalize(sourceNormal(c.x,instance,false)*b.x+sourceNormal(c.y,instance,false)*b.y+sourceNormal(c.z,instance,false)*b.z);
    vec2 texel=1./vec2(textureSize(patchHeight,0)),uv=faceUV(c,b);
    float dx=(textureLod(patchHeight,uv+vec2(texel.x,0),0).r-textureLod(patchHeight,uv-vec2(texel.x,0),0).r)*patchFrame.displacement.x/(2.*texel.x);
    float dy=(textureLod(patchHeight,uv+vec2(0,texel.y),0).r-textureLod(patchHeight,uv-vec2(0,texel.y),0).r)*patchFrame.displacement.x/(2.*texel.y);
    vec3 e1=sourcePosition(c.y,instance,false)-sourcePosition(c.x,instance,false),e2=sourcePosition(c.z,instance,false)-sourcePosition(c.x,instance,false);
    vec2 uv1=(sourceUV(c.y)-sourceUV(c.x))*patchFrame.displacement.z,uv2=(sourceUV(c.z)-sourceUV(c.x))*patchFrame.displacement.z;
    float det=uv1.x*uv2.y-uv1.y*uv2.x;
    if(abs(det)<1e-12)return n;
    vec3 u=(e1*uv2.y-e2*uv1.y)/det,v=(e2*uv1.x-e1*uv2.x)/det;
#ifdef GRAPHENE_TERRAIN
    texel=terrainHeightStep();
    vec3 position=sourcePosition(c.x,instance,false)*b.x+sourcePosition(c.y,instance,false)*b.y+sourcePosition(c.z,instance,false)*b.z;
    mat4 model=patchInstances.data[instance].model;
    vec2 world=(model*vec4(position,1)).xz,du=(model*vec4(u,0)).xz,dv=(model*vec4(v,0)).xz;
    dx=(terrainOffset(uv+vec2(texel.x,0),world+du*texel.x)-terrainOffset(uv-vec2(texel.x,0),world-du*texel.x))/(2.*texel.x);
    dy=(terrainOffset(uv+vec2(0,texel.y),world+dv*texel.y)-terrainOffset(uv-vec2(0,texel.y),world-dv*texel.y))/(2.*texel.y);
#endif
    float uu=dot(u,u),vv=dot(v,v),uvv=dot(u,v),metric=uu*vv-uvv*uvv;
    if(metric<1e-12)return n;
    return normalize(n-u*((dx*vv-dy*uvv)/metric)-v*((dy*uu-dx*uvv)/metric));
}
DisplacedVertex displacedVertex(uint jobIndex, uint vertex) {
    DisplacedVertex result;result.valid=false;
    if(jobIndex>=patchState.data[0])return result;
    uvec4 job=patchJobs.data[jobIndex];uint triangle=job.z+vertex/3u;
    if(triangle>=job.w)return result;
    uvec4 pattern=patchPatterns.data[job.y+triangle];vec3 b=patternBarycentric(pattern[vertex%3u],pattern.w);
    uint instance=patchInstance(job.x),face=patchFace(job.x);PatchInstance item=patchInstances.data[instance];
    vec3 p=surfacePosition(face,b,instance,false),previous=surfacePosition(face,b,instance,true);
    vec4 world=item.model*vec4(p,1.);result.position=world.xyz;
    mat3 basis=mat3(item.model);vec3 normal=displacedNormal(face,b,instance);
    result.normal=normalize(abs(determinant(basis))>1e-12?transpose(inverse(basis))*normal:normal);
    result.clip=patchFrame.viewProjection*world;
    result.previousClip=patchFrame.previousViewProjection*item.previousModel*vec4(previous,1.);
    uvec3 corners=patchCorners(face);result.uv=faceUV(corners,b);
    result.color=vec3(0);
    for(uint k=0;k<3;++k) {
        uint offset=corners[k]*11u;result.color+=vec3(patchVertices.data[offset+6],patchVertices.data[offset+7],patchVertices.data[offset+8])*b[k];
    }
    if(patchFrame.control.w!=0u)result.color=vec3(1);
#ifdef GRAPHENE_TERRAIN
    result.color=vec3(1);
#endif
    result.instance=instance;result.valid=true;return result;
}
