#version 450

layout(location = 0) in vec3 world_normal;
layout(location = 1) in vec3 color;
layout(location = 2) flat in vec4 material_base_color;
layout(location = 3) flat in vec4 material_surface;
layout(location = 4) in vec3 world_position;
layout(location = 0) out vec4 output_color;

layout(set = 0, binding = 0) uniform Frame {
    mat4 view_projection;
    vec4 light_direction;
    vec4 light_color_intensity;
    vec4 indirect_ambient;
    vec4 camera_position;
} frame;

layout(std430, set = 0, binding = 2) readonly buffer GiScene {
    float values[];
} gi_scene;
layout(std430, set = 0, binding = 3) readonly buffer SurfaceCache {
    float values[];
} gi_surfaces;
layout(std430, set = 0, binding = 4) readonly buffer RadianceHistory {
    float values[];
} gi_history;

vec3 surfaceCacheIrradiance(vec3 position, vec3 normal) {
    uint sampleCount = min(uint(gi_scene.values[0]), 64u);
    vec3 irradiance = vec3(0.0);
    float totalWeight = 0.0;
    for (uint sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex) {
        uint surface = sampleIndex * 12;
        uint history = sampleIndex * 4;
        if (gi_history.values[history + 3] <= 0.0) continue;
        vec3 samplePosition = vec3(gi_surfaces.values[surface],
            gi_surfaces.values[surface + 1], gi_surfaces.values[surface + 2]);
        vec3 sampleNormal = normalize(vec3(gi_surfaces.values[surface + 4],
            gi_surfaces.values[surface + 5], gi_surfaces.values[surface + 6]));
        bool worldProbe = gi_surfaces.values[surface + 3] < 0.5;
        float normalWeight = worldProbe ? 0.22 :
            max(dot(normal, sampleNormal), 0.0);
        float distanceWeight = 1.0 /
            (1.0 + dot(position - samplePosition,
                position - samplePosition) * (worldProbe ? 0.08 : 1.0));
        float weight = normalWeight * distanceWeight;
        irradiance += vec3(gi_history.values[history],
            gi_history.values[history + 1],
            gi_history.values[history + 2]) * weight;
        totalWeight += weight;
    }
    return totalWeight > 0.0001 ? irradiance / totalWeight :
        frame.indirect_ambient.rgb;
}

vec3 surfaceCacheReflection(vec3 position, vec3 direction, float roughness) {
    uint sampleCount = min(uint(gi_scene.values[0]), 64u);
    vec3 radiance = vec3(0.0);
    float totalWeight = 0.0;
    float conePower = mix(96.0, 3.0, roughness);
    for (uint sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex) {
        uint surface = sampleIndex * 12;
        uint history = sampleIndex * 4;
        if (gi_history.values[history + 3] <= 0.0) continue;
        vec3 samplePosition = vec3(gi_surfaces.values[surface],
            gi_surfaces.values[surface + 1], gi_surfaces.values[surface + 2]);
        vec3 delta = samplePosition - position;
        float distanceSquared = max(dot(delta, delta), 0.0001);
        float alignment = max(dot(normalize(delta), direction), 0.0);
        float kind = gi_surfaces.values[surface + 3];
        float cacheWeight = kind > 1.5 ? 1.0 : (kind < 0.5 ? 0.18 : 0.55);
        float weight = pow(alignment, conePower) * cacheWeight /
            (1.0 + distanceSquared * 0.08);
        radiance += vec3(gi_history.values[history],
            gi_history.values[history + 1],
            gi_history.values[history + 2]) * weight;
        totalWeight += weight;
    }
    return totalWeight > 0.0001 ? radiance / totalWeight :
        surfaceCacheIrradiance(position, direction);
}

void main() {
    float diffuse = max(dot(normalize(world_normal), normalize(-frame.light_direction.xyz)), 0.0);
    float metallic = material_surface.x;
    float roughness = material_surface.y;
    float unlit = material_surface.z;
    float diffuse_energy = (1.0 - metallic) * (1.0 - 0.25 * roughness);
    vec3 base_color = color * material_base_color.rgb;
    vec3 direct = frame.light_color_intensity.xyz *
        (diffuse * frame.light_color_intensity.w * diffuse_energy);
    vec3 indirect = surfaceCacheIrradiance(world_position,
        normalize(world_normal));
    vec3 view_direction = normalize(frame.camera_position.xyz - world_position);
    vec3 reflected = reflect(-view_direction, normalize(world_normal));
    vec3 reflected_radiance = surfaceCacheReflection(
        world_position + reflected * 0.05, reflected, roughness);
    vec3 fresnel_zero = mix(vec3(0.04), base_color, metallic);
    float specular_strength = (1.0 - roughness) * (1.0 - roughness);
    vec3 indirect_specular = reflected_radiance * fresnel_zero * specular_strength;
    vec3 lit_color = base_color * (indirect + direct) + indirect_specular;
    vec3 linear_color = mix(lit_color, base_color, unlit);
    output_color = vec4(pow(linear_color, vec3(1.0 / 2.2)), material_base_color.a);
}
