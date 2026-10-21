#version 450

layout(location = 0) in vec3 world_normal;
layout(location = 1) in vec3 color;
layout(location = 2) flat in vec4 material_base_color;
layout(location = 3) flat in vec4 material_surface;
layout(location = 4) in vec3 world_position;
#ifdef GRAPHENE_BASE_COLOR_TEXTURE
layout(location = 5) in vec2 texture_uv;
layout(set = 0, binding = 5) uniform sampler2D base_color_texture;
layout(set = 0, binding = 6) uniform sampler2D normal_texture;
layout(location = 6) flat in vec4 material_texture;
#include "material_normal.glsl"
#endif
layout(location = 0) out vec4 output_color;

layout(set = 0, binding = 0) uniform Frame {
    mat4 view_projection;
    vec4 light_direction;
    vec4 light_color_intensity;
    vec4 indirect_ambient;
    vec4 camera_position;
    vec4 actor_lighting[785];
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

#define actor_lighting frame.actor_lighting
#include "scene_lighting.glsl"
#include "environment_surface.glsl"
#ifdef GRAPHENE_MATERIAL_GRAPH
#include "material_graph_generated.glsl"
#endif

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

#include "aa_motion.glsl"
#include "aa_decode.glsl"
void main() {
#ifdef GRAPHENE_TEMPORAL
    writeMotion();
#endif
    if (material_surface.z > 1.5) {
        vec3 ray = normalize(world_position - frame.camera_position.xyz);
        vec3 sun = normalize(-frame.light_direction.xyz);
        float altitude = clamp(sun.y * 2.0 + 0.65, 0.0, 1.0) * clamp(frame.light_color_intensity.w, 0.0, 1.0);
        float height = clamp(ray.y, 0.0, 1.0);
        vec3 horizon = mix(vec3(0.025, 0.035, 0.075), vec3(0.24, 0.48, 0.78), altitude);
        vec3 zenith = mix(vec3(0.004, 0.008, 0.025), vec3(0.018, 0.12, 0.38), altitude);
        if (material_base_color.r > 0.2) { horizon = vec3(0.55, 0.18, 0.07); zenith = vec3(0.075, 0.055, 0.18); }
        bool nightPreset = material_base_color.r < 0.1 && material_base_color.b < 0.1;
        if (nightPreset) { horizon *= 0.06; zenith *= 0.035; }
        vec3 sky = mix(horizon, zenith, pow(height, 0.45));
        sky = mix(sky, vec3(0.035, 0.04, 0.05), (1.0 - smoothstep(-0.4, 0.0, ray.y)));
        float radius = radians(actor_lighting[0].w * 0.5);
        float angle = acos(clamp(dot(ray, sun), -1.0, 1.0));
        float disk = 1.0 - smoothstep(radius - fwidth(angle), radius + fwidth(angle), angle);
        float glow = exp(-angle * 24.0) * 0.18;
        vec3 sunColor = frame.light_color_intensity.rgb;
        float energy = clamp(frame.light_color_intensity.w, 0.0, 1.0) * (nightPreset ? 0.0 : 1.0);
        sky += (disk * 8.0 + glow) * sunColor * energy;
#ifdef GRAPHENE_LINEAR_COLOR
        output_color=vec4(aaDecode(pow(max(sky,vec3(0)),vec3(1.0/2.2))),1);
#else
        output_color = vec4(pow(max(sky, vec3(0.0)), vec3(1.0 / 2.2)), 1.0);
#endif
        return;
    }
    vec3 surface_normal = normalize(world_normal);
#ifdef GRAPHENE_BASE_COLOR_TEXTURE
    surface_normal = materialNormal(normal_texture, world_position, world_normal, texture_uv,
        material_surface.z > 0.5 ? 0.0 : material_texture.x, material_texture.y);
#endif
    float diffuse = max(dot(surface_normal, normalize(-frame.light_direction.xyz)), 0.0);
    float metallic = material_surface.x;
    float roughness = material_surface.y;
    float unlit = material_surface.z;
    float diffuse_energy = (1.0 - metallic) * (1.0 - 0.25 * roughness);
#ifdef GRAPHENE_BASE_COLOR_TEXTURE
    vec3 texel = material_surface.w > 0.5 ? texture(base_color_texture, texture_uv).rgb : vec3(1.0);
#else
    vec3 texel = vec3(1.0);
#endif
    vec3 base_color = color * material_base_color.rgb * texel;
#ifdef GRAPHENE_MATERIAL_GRAPH
    mg_position=world_position;mg_direction=normalize(world_position-frame.camera_position.xyz);
#ifdef GRAPHENE_BASE_COLOR_TEXTURE
    mg_uv=texture_uv;
#else
    mg_uv=world_position.xz;
#endif
    for(int i=0;i<8;++i)mg_environment[i]=actor_lighting[257+i];
    EnvironmentWeather mgLocal=environmentWeather(world_position);mg_environment[1]=mgLocal.sky;mg_environment[2]=mgLocal.wind;mg_environment[3].xy=mgLocal.surface.xy;
    if(material_surface.z<.5)evaluateMaterial(base_color,metallic,roughness);
#endif
    environmentSurface(world_position, surface_normal, base_color, roughness);
    diffuse_energy=(1.-metallic)*(1.-.25*roughness);
    vec3 direct = frame.light_color_intensity.xyz *
        (diffuse * frame.light_color_intensity.w * diffuse_energy) + actorDirectLight(world_position, surface_normal) * diffuse_energy;
    vec3 indirect = surfaceCacheIrradiance(world_position,
        surface_normal);
    vec3 view_direction = normalize(frame.camera_position.xyz - world_position);
    vec3 reflected = reflect(-view_direction, surface_normal);
    vec3 reflected_radiance = surfaceCacheReflection(
        world_position + reflected * 0.05, reflected, roughness);
    vec3 fresnel_zero = mix(vec3(0.04), base_color, metallic);
    float specular_strength = (1.0 - roughness) * (1.0 - roughness);
    vec3 indirect_specular = reflected_radiance * fresnel_zero * specular_strength;
    vec3 lit_color = base_color * (indirect + direct * environmentCloudShadow(world_position)) + indirect_specular;
    vec3 linear_color = mix(lit_color, base_color, unlit);
    linear_color = environmentFog(world_position, frame.camera_position.xyz, linear_color);
    vec3 encoded = mix(12.92 * linear_color,
        1.055 * pow(max(linear_color, vec3(0.0)), vec3(1.0 / 2.4)) - 0.055,
        greaterThan(linear_color, vec3(0.0031308)));
#ifdef GRAPHENE_LINEAR_COLOR
    output_color=vec4(max(linear_color,vec3(0)),material_base_color.a);
#else
    output_color = vec4(encoded, material_base_color.a);
#endif
}
