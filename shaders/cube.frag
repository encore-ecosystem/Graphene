#version 450

layout(location = 0) in vec3 world_normal;
layout(location = 1) in vec3 color;
layout(location = 2) flat in vec4 material_base_color;
layout(location = 3) flat in vec4 material_surface;
layout(location = 0) out vec4 output_color;

layout(set = 0, binding = 0) uniform Frame {
    mat4 view_projection;
    vec4 light_direction;
    vec4 light_color_intensity;
} frame;

void main() {
    float diffuse = max(dot(normalize(world_normal), normalize(-frame.light_direction.xyz)), 0.0);
    float metallic = material_surface.x;
    float roughness = material_surface.y;
    float unlit = material_surface.z;
    float diffuse_energy = (1.0 - metallic) * (1.0 - 0.25 * roughness);
    float illumination = 0.28 + diffuse * frame.light_color_intensity.w * diffuse_energy;
    vec3 base_color = color * material_base_color.rgb;
    vec3 lit_color = base_color * frame.light_color_intensity.xyz * illumination;
    vec3 linear_color = mix(lit_color, base_color, unlit);
    output_color = vec4(pow(linear_color, vec3(1.0 / 2.2)), material_base_color.a);
}
