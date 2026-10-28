#version 450
// Vertex pulling expands each microtriangle to three unique vertices, so the
// actual generated edges need no optional fragment-barycentric GPU feature.
layout(location=7) noperspective in vec3 triangle_barycentric;
layout(location=8) flat in uint triangle_identity;
layout(location=0) out vec4 output_color;
#include "../aa_motion.glsl"
#include "../aa_decode.glsl"
void main() {
#ifdef GRAPHENE_TEMPORAL
    writeMotion();
#endif
    uint key=triangle_identity+0x9e3779b9u+3u*0x85ebca6bu;
    key^=key>>16u;key*=0x7feb352du;key^=key>>15u;
    key*=0x846ca68bu;key^=key>>16u;
    vec3 fill=.22+.78*vec3(key&255u,(key>>8u)&255u,(key>>16u)&255u)/255.;
    float edge=min(min(triangle_barycentric.x,triangle_barycentric.y),triangle_barycentric.z);
    float width=max(fwidth(edge),.0005);
    float line=1.-smoothstep(width*.8,width*2.2,edge);
    output_color=vec4(mix(fill,vec3(.015,.02,.03),line*.82),1);
#ifdef GRAPHENE_LINEAR_COLOR
    output_color.rgb=aaDecode(output_color.rgb);
#endif
}
