// Main and Post share one distance policy. Projection is used only by the
// compatible low-level pixel-error API, not by project triangle-budget groups.
float lodFocalY(mat4 vp) {
    float saved = valuef(62u);
    return saved > 0.0 ? saved : length(vec3(vp[0][1], vp[1][1], vp[2][1]));
}
// Low four bits are the point count; bit four selects monotone PCHIP.
uint lodCurveCount() { return min(uint(valuef(42u)) & 15u, 8u); }
vec2 lodPoint(uint i) { return vec2(valuef(43u+i*2u),valuef(44u+i*2u)); }
float lodSecant(uint i) { vec2 a=lodPoint(i),b=lodPoint(i+1u); return (b.y-a.y)/(b.x-a.x); }
float lodSlope(uint i,uint n) {
    if(n==2u) return lodSecant(0u);
    if(i==0u || i==n-1u) {
        uint a=i==0u?0u:n-2u,b=i==0u?1u:n-3u;
        float h0=lodPoint(a+1u).x-lodPoint(a).x,h1=lodPoint(b+1u).x-lodPoint(b).x;
        float d0=lodSecant(a),d1=lodSecant(b),w=h0/(h0+h1);
        float m=(1.0+w)*d0-w*d1;
        return d0==0.0 || m>=0.0?0.0:max(m,3.0*d0);
    }
    float d0=lodSecant(i-1u),d1=lodSecant(i);
    if(d0==0.0 || d1==0.0) return 0.0;
    float h0=lodPoint(i).x-lodPoint(i-1u).x,h1=lodPoint(i+1u).x-lodPoint(i).x;
    float w=(1.0+h1/(h0+h1))/3.0;
    return 1.0/(w/d0+(1.0-w)/d1);
}
float lodCurveRatio(mat4 model, mat4 vp, vec3 camera, float scale) {
    uint count=lodCurveCount();
    if(count<2u) return 1.0;
#ifdef GRAPHENE_SKINNED
    vec3 center=(model*vec4(skinNodeSphere(animation_instance,uint(valuef(40u))).xyz,1)).xyz;
#else
    vec3 center=(model*vec4(valuef(59u),valuef(60u),valuef(61u),1.0)).xyz;
#endif
    float d=distance(camera,center);
    vec2 a=lodPoint(0u);
    if(d<=a.x) return a.y;
    for(uint i=1u;i<count;++i) {
        vec2 b=lodPoint(i);
        if(d<=b.x) {
            float h=b.x-a.x,t=(d-a.x)/h;
            if((uint(valuef(42u))&16u)==0u || count==2u) return mix(a.y,b.y,t);
            float t2=t*t,t3=t2*t;
            return clamp((2.0*t3-3.0*t2+1.0)*a.y+(t3-2.0*t2+t)*h*lodSlope(i-1u,count)+
                (-2.0*t3+3.0*t2)*b.y+(t3-t2)*h*lodSlope(i,count),b.y,a.y);
        }
        a=b;
    }
    return a.y;
}
float lodProjectedError(mat4 vp, vec4 clip, float radius, float error,
    float scale, float viewportHeight) {
    return error * scale * viewportHeight * 0.5 * lodFocalY(vp) /
        max(clip.w-radius,0.001);
}
bool lodSphereInFrustum(mat4 vp, vec4 clip, float radius) {
    vec3 x=vec3(vp[0][0],vp[1][0],vp[2][0]);
    vec3 y=vec3(vp[0][1],vp[1][1],vp[2][1]);
    vec3 z=vec3(vp[0][2],vp[1][2],vp[2][2]);
    vec3 w=vec3(vp[0][3],vp[1][3],vp[2][3]);
    return clip.w+clip.x >= -radius*length(w+x) &&
        clip.w-clip.x >= -radius*length(w-x) &&
        clip.w+clip.y >= -radius*length(w+y) &&
        clip.w-clip.y >= -radius*length(w-y) &&
        clip.z >= -radius*length(z) && clip.w-clip.z >= -radius*length(w-z);
}

// Legacy low-level pixel-error API remains available for existing callers.
// Project LOD groups use an actual mesh-specific triangle-budget schedule.
bool lodRefine(uint nodeBase, bool forceFullDetail, float ratio, float projectedError) {
    if (forceFullDetail) return true;
    if (lodCurveCount() < 2u) return projectedError > max(valuef(41u),0.125);
    return ratio >= 1.0 || ratio >= valuef(nodeBase+10u);
}

float lodTargetRatio(mat4 model, mat4 vp, vec3 camera, float scale) {
    float ratio = lodCurveRatio(model, vp, camera, scale);
    // Transform/distance round-off must not drop source triangles at an
    // endpoint authored as full detail.
    return ratio >= 0.999999 ? 1.0 : ratio;
}
