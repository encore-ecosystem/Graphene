#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 vertex_color;
layout(location = 0) out vec3 world_normal;
layout(location = 1) out vec3 color;
layout(location = 2) flat out vec4 material_base_color;
layout(location = 3) flat out vec4 material_surface;

layout(set = 0, binding = 0) uniform Frame {
    mat4 view_projection;
    vec4 light_direction;
    vec4 light_color_intensity;
} frame;

struct Instance {
    mat4 model;
    vec4 material_base_color;
    vec4 material_surface;
};

layout(std430, set = 0, binding = 1) readonly buffer Instances {
    Instance values[];
} instances;

void main() {
    Instance instance = instances.values[gl_InstanceIndex];
    gl_Position = frame.view_projection * instance.model * vec4(position, 1.0);
    world_normal = normalize(mat3(instance.model) * normal);
    color = vertex_color;
    material_base_color = instance.material_base_color;
    material_surface = instance.material_surface;
}
