#version 460
#extension GL_GOOGLE_include_directive : require
#include "render.glsl"
layout(location=0) out vec3 world_normal;
layout(location=1) out vec3 color;
layout(location=2) flat out vec4 material_base_color;
layout(location=3) flat out vec4 material_surface;
layout(location=4) out vec3 world_position;
layout(location=5) out vec2 texture_uv;
layout(location=6) flat out vec4 material_texture;
layout(location=7) noperspective out vec3 triangle_barycentric;
layout(location=8) flat out uint triangle_identity;
layout(location=13) out vec4 current_clip;
layout(location=14) out vec4 previous_clip;
layout(location=15) flat out float temporal_valid;
void main() {
    DisplacedVertex v=displacedVertex(gl_InstanceIndex,gl_VertexIndex);
    if(!v.valid) { gl_Position=vec4(0,0,0,1);temporal_valid=0.;return; }
    PatchInstance item=patchInstances.data[v.instance];
    gl_Position=v.clip;world_position=v.position;world_normal=v.normal;color=v.color;
    material_base_color=item.tint;material_surface=item.surface;material_texture=item.textureFlags;texture_uv=v.uv;
    current_clip=v.clip;previous_clip=v.previousClip;temporal_valid=float(item.geometry.w);
    triangle_barycentric=vec3(0);triangle_barycentric[gl_VertexIndex%3]=1;
    triangle_identity=displacedTriangleIdentity(gl_InstanceIndex,gl_VertexIndex);
}
