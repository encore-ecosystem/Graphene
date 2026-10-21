#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 vertex_color;
#ifdef GRAPHENE_BASE_COLOR_TEXTURE
layout(location = 3) in vec2 uv;
#endif
layout(location = 0) out vec3 world_normal;
layout(location = 1) out vec3 color;
layout(location = 2) flat out vec4 material_base_color;
layout(location = 3) flat out vec4 material_surface;
layout(location = 4) out vec3 world_position;
#ifdef GRAPHENE_BASE_COLOR_TEXTURE
layout(location = 5) out vec2 texture_uv;
layout(location = 6) flat out vec4 material_texture;
#endif

layout(set = 0, binding = 0) uniform Frame {
    mat4 view_projection;
    vec4 light_direction;
    vec4 light_color_intensity;
    vec4 indirect_ambient;
    vec4 camera_position;
    vec4 actor_lighting[785];
#ifdef GRAPHENE_TEMPORAL
    mat4 previous_view_projection;
#endif
} frame;

struct Instance {
    mat4 model;
    vec4 material_base_color;
    vec4 material_surface;
#ifdef GRAPHENE_BASE_COLOR_TEXTURE
    vec4 material_texture;
#endif
#ifdef GRAPHENE_TEMPORAL
    mat4 previous_model;
    vec4 temporal_valid;
#endif
};

layout(std430, set = 0, binding = 1) readonly buffer Instances {
    Instance values[];
} instances;

#ifdef GRAPHENE_TEMPORAL
layout(location=13) out vec4 current_clip;
layout(location=14) out vec4 previous_clip;
layout(location=15) flat out float temporal_valid;
#endif
void main() {
    Instance instance = instances.values[gl_InstanceIndex];
    vec4 transformed_position = instance.model * vec4(position, 1.0);
    gl_Position = frame.view_projection * transformed_position;
    if (instance.material_surface.z > 1.5) gl_Position.z = gl_Position.w * 0.999999;
#ifdef GRAPHENE_TEMPORAL
    current_clip=gl_Position;
    previous_clip=frame.previous_view_projection * instance.previous_model * vec4(position,1);
    temporal_valid=instance.temporal_valid.x;
#endif
    world_position = transformed_position.xyz;
    mat3 basis = mat3(instance.model);
    world_normal = normalize(abs(determinant(basis)) > 1e-10 ? transpose(inverse(basis)) * normal : normal);
    color = vertex_color;
#ifdef GRAPHENE_BASE_COLOR_TEXTURE
    texture_uv = uv;
    material_texture = instance.material_texture;
#endif
    material_base_color = instance.material_base_color;
    material_surface = instance.material_surface;
}
