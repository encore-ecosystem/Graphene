#version 450
#extension GL_EXT_fragment_shader_barycentric : require
#extension GL_EXT_mesh_shader : require

layout(location = 0) in vec3 world_normal;
layout(location = 1) in vec3 color;
layout(location = 2) flat in vec4 material_base_color;
layout(location = 3) flat in vec4 material_surface;
layout(location = 4) flat in vec4 light_direction;
layout(location = 5) flat in vec4 light_color_intensity;
layout(location = 6) flat in vec4 indirect_ambient;
layout(location = 7) in vec3 world_position;
layout(location = 8) flat in vec3 camera_position;
layout(location = 9) flat in uvec4 virtual_debug;
layout(location = 10) perprimitiveEXT flat in uint triangle_debug;
layout(location = 0) out vec4 output_color;

layout(std430, set = 0, binding = 3) readonly buffer GiScene {
    float values[];
} gi_scene;
layout(std430, set = 0, binding = 4) readonly buffer SurfaceCache {
    float values[];
} gi_surfaces;
layout(std430, set = 0, binding = 5) readonly buffer RadianceHistory {
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
        vec3 delta = position - samplePosition;
        float weight = normalWeight /
            (1.0 + dot(delta, delta) * (worldProbe ? 0.08 : 1.0));
        irradiance += vec3(gi_history.values[history],
            gi_history.values[history + 1],
            gi_history.values[history + 2]) * weight;
        totalWeight += weight;
    }
    return totalWeight > 0.0001 ? irradiance / totalWeight :
        indirect_ambient.rgb;
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
    uint viewMode = virtual_debug.w >> 24u;
    uint instanceIndex = virtual_debug.y & 0x0007ffffu;
    bool postVisible = (virtual_debug.y & 0x00080000u) != 0u;
    uint desiredLod = (virtual_debug.y >> 20u) & 0x3fu;
    uint selectedLod = (virtual_debug.y >> 26u) & 0x3fu;
    uint clusterIndex = virtual_debug.x;
    uint lod = virtual_debug.z;
    uint primitiveId = triangle_debug;
    if (viewMode == 2u) {
        output_color = vec4(0.08, 0.9, 0.2, 1.0);
        return;
    }
    uint diagnosticKey = clusterIndex;
    if (viewMode == 3u) diagnosticKey = triangle_debug;
    else if (viewMode == 4u) diagnosticKey = clusterIndex / 4u;
    else if (viewMode == 6u) diagnosticKey = primitiveId;
    else if (viewMode == 7u) diagnosticKey =
        primitiveId * 4099u + instanceIndex;
    if (viewMode >= 3u && viewMode <= 7u) {
        diagnosticKey += 0x9e3779b9u + viewMode * 0x85ebca6bu;
        diagnosticKey ^= diagnosticKey >> 16u;
        diagnosticKey *= 0x7feb352du;
        diagnosticKey ^= diagnosticKey >> 15u;
        diagnosticKey *= 0x846ca68bu;
        diagnosticKey ^= diagnosticKey >> 16u;
        vec3 diagnostic = vec3(
            float(diagnosticKey & 255u),
            float((diagnosticKey >> 8u) & 255u),
            float((diagnosticKey >> 16u) & 255u)) / 255.0;
        if (viewMode == 3u) {
            float edge = min(min(gl_BaryCoordEXT.x, gl_BaryCoordEXT.y),
                gl_BaryCoordEXT.z);
            float width = max(fwidth(edge), 0.0005);
            float edgeLine = 1.0 - smoothstep(width * 0.8, width * 2.2, edge);
            diagnostic = mix(diagnostic, vec3(0.015, 0.02, 0.03), edgeLine * 0.82);
        }
        output_color = vec4(0.22 + diagnostic * 0.78, 1.0);
        return;
    }
    if (viewMode == 8u) {
        const vec3 palette[8] = vec3[8](
            vec3(0.1, 0.9, 0.2), vec3(0.1, 0.7, 1.0),
            vec3(0.3, 0.25, 1.0), vec3(0.8, 0.2, 1.0),
            vec3(1.0, 0.2, 0.55), vec3(1.0, 0.35, 0.1),
            vec3(1.0, 0.8, 0.05), vec3(0.9, 0.95, 0.95));
        output_color = vec4(palette[min(lod, 7u)], 1.0);
        return;
    }
    if (viewMode == 9u) {
        uint fallback = selectedLod > desiredLod ?
            selectedLod - desiredLod : 0u;
        output_color = vec4(fallback == 0u ? vec3(0.08, 0.9, 0.2) :
            (fallback == 1u ? vec3(1.0, 0.75, 0.05) :
                vec3(1.0, 0.08, 0.04)), 1.0);
        return;
    }
    if (viewMode == 10u) {
        // The overdraw pipeline disables depth testing and uses ONE+ONE
        // framebuffer blending. Every evaluated covered fragment therefore
        // contributes exactly one fixed quantum to its pixel.
        output_color = vec4(0.12, 0.025, 0.004, 0.06);
        return;
    }
    if (viewMode == 11u) {
        // Green clusters survived Main using previous-frame visibility.
        // Orange clusters were recovered by Post against the current HZB.
        output_color = vec4(postVisible ?
            vec3(1.0, 0.28, 0.025) : vec3(0.04, 0.9, 0.22), 1.0);
        return;
    }
    float diffuse = max(dot(normalize(world_normal),
        normalize(-light_direction.xyz)), 0.0);
    float metallic = material_surface.x;
    float roughness = material_surface.y;
    float unlit = material_surface.z;
    float diffuseEnergy = (1.0 - metallic) * (1.0 - 0.25 * roughness);
    vec3 baseColor = color * material_base_color.rgb;
    vec3 direct = light_color_intensity.xyz *
        diffuse * light_color_intensity.w * diffuseEnergy;
    vec3 indirect = surfaceCacheIrradiance(world_position,
        normalize(world_normal));
    vec3 viewDirection = normalize(camera_position - world_position);
    vec3 reflected = reflect(-viewDirection, normalize(world_normal));
    vec3 reflectedRadiance = surfaceCacheReflection(
        world_position + reflected * 0.05, reflected, roughness);
    vec3 fresnelZero = mix(vec3(0.04), baseColor, metallic);
    float specularStrength = (1.0 - roughness) * (1.0 - roughness);
    vec3 indirectSpecular = reflectedRadiance * fresnelZero * specularStrength;
    vec3 litColor = baseColor * (indirect + direct) + indirectSpecular;
    vec3 linearColor = viewMode == 1u ? baseColor :
        mix(litColor, baseColor, unlit);
    output_color = vec4(pow(linearColor, vec3(1.0 / 2.2)),
        material_base_color.a);
}
