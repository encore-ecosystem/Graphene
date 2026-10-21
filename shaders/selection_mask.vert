#version 450
layout(location = 0) in vec3 position;
layout(location = 0) flat out float selected;
layout(set = 0, binding = 0) uniform Frame { mat4 view_projection; } frame;
struct Instance { mat4 model; vec4 base_color; vec4 surface; };
layout(std430, set = 0, binding = 1) readonly buffer Instances { Instance values[]; } instances;
void main() {
    Instance instance = instances.values[gl_InstanceIndex];
    gl_Position = frame.view_projection * instance.model * vec4(position, 1.0);
    selected = instance.base_color.r;
}
