#version 460
#ifdef GRAPHENE_RAY_LIGHTING
#extension GL_EXT_ray_query : require
#endif
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
layout(location = 11) in vec2 texture_uv;
layout(set = 0, binding = 6) uniform sampler2D base_color_texture;
layout(set = 0, binding = 7) uniform sampler2D normal_texture;
layout(set = 0, binding = 18) uniform sampler2D material_orm;
layout(location = 12) flat in vec4 material_texture;
#include "material_normal.glsl"
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

layout(std430, set = 0, binding = 8) readonly buffer ActorLights { vec4 actor_lighting[]; };
#include "ray_lighting.glsl"
#include "scene_lighting.glsl"
#define GRAPHENE_SHELTER_BINDING 11
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

#include "aa_motion.glsl"
#include "aa_decode.glsl"
void main() {
#ifdef GRAPHENE_TEMPORAL
    writeMotion();
#endif
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
#ifdef GRAPHENE_LINEAR_COLOR
        output_color.rgb=aaDecode(output_color.rgb);
#endif
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
#ifdef GRAPHENE_LINEAR_COLOR
        output_color.rgb=aaDecode(output_color.rgb);
#endif
        return;
    }
    if (viewMode == 8u) {
        const vec3 palette[8] = vec3[8](
            vec3(0.1, 0.9, 0.2), vec3(0.1, 0.7, 1.0),
            vec3(0.3, 0.25, 1.0), vec3(0.8, 0.2, 1.0),
            vec3(1.0, 0.2, 0.55), vec3(1.0, 0.35, 0.1),
            vec3(1.0, 0.8, 0.05), vec3(0.9, 0.95, 0.95));
        output_color = vec4(palette[min(lod, 7u)], 1.0);
#ifdef GRAPHENE_LINEAR_COLOR
        output_color.rgb=aaDecode(output_color.rgb);
#endif
        return;
    }
    if (viewMode == 9u) {
        uint fallback = selectedLod > desiredLod ?
            selectedLod - desiredLod : 0u;
        output_color = vec4(fallback == 0u ? vec3(0.08, 0.9, 0.2) :
            (fallback == 1u ? vec3(1.0, 0.75, 0.05) :
                vec3(1.0, 0.08, 0.04)), 1.0);
#ifdef GRAPHENE_LINEAR_COLOR
        output_color.rgb=aaDecode(output_color.rgb);
#endif
        return;
    }
    if (viewMode == 10u) {
        // The overdraw pipeline disables depth testing and uses ONE+ONE
        // framebuffer blending. Every evaluated covered fragment therefore
        // contributes exactly one fixed quantum to its pixel.
        output_color = vec4(0.12, 0.025, 0.004, 0.06);
#ifdef GRAPHENE_LINEAR_COLOR
        output_color.rgb=aaDecode(output_color.rgb);
#endif
        return;
    }
    if (viewMode == 11u) {
        // Green clusters survived Main using previous-frame visibility.
        // Orange clusters were recovered by Post against the current HZB.
        output_color = vec4(postVisible ?
            vec3(1.0, 0.28, 0.025) : vec3(0.04, 0.9, 0.22), 1.0);
#ifdef GRAPHENE_LINEAR_COLOR
        output_color.rgb=aaDecode(output_color.rgb);
#endif
        return;
    }
    vec3 surfaceNormal = materialNormal(normal_texture, world_position, world_normal, texture_uv,
        material_surface.z > 0.5 ? 0.0 : material_texture.x, material_texture.y);
    float diffuse = max(dot(surfaceNormal,
        normalize(-light_direction.xyz)), 0.0);
    float metallic = material_surface.x;
    float roughness = material_surface.y;
    vec3 orm = texture(material_orm, texture_uv).rgb;
    roughness *= mix(1.0, orm.g, material_texture.z);
    metallic *= mix(1.0, orm.b, material_texture.w);
    float unlit = material_surface.z;
    float diffuseEnergy = (1.0 - metallic) * (1.0 - 0.25 * roughness);
    vec3 baseColor = color * material_base_color.rgb * texture(base_color_texture, texture_uv).rgb;
#ifdef GRAPHENE_MATERIAL_GRAPH
    mg_position=world_position;mg_direction=normalize(world_position-camera_position);mg_uv=texture_uv;
    for(int i=0;i<8;++i)mg_environment[i]=actor_lighting[257+i];
    EnvironmentWeather mgLocal=environmentWeather(world_position);mg_environment[1]=mgLocal.sky;mg_environment[2]=mgLocal.wind;mg_environment[3].xy=mgLocal.surface.xy;
    {vec3 mgNormal;float mgHeight;float mgOcclusion;evaluateSurface(baseColor,metallic,roughness,mgNormal,mgOcclusion,mgHeight);orm.r=mgOcclusion;surfaceNormal=materialSampleNormal(mgNormal,world_position,world_normal,mg_uv,mgNormalStrength(),mgNormalFlip());diffuse=max(dot(surfaceNormal,normalize(-light_direction.xyz)),0.); }
#endif
    environmentSurface(world_position, surfaceNormal, baseColor, roughness);
    diffuseEnergy=(1.-metallic)*(1.-.25*roughness);
    vec3 direct = light_color_intensity.xyz *
        diffuse * light_color_intensity.w * diffuseEnergy + actorDirectLight(world_position, surfaceNormal) * diffuseEnergy;
    vec3 indirect = surfaceCacheIrradiance(world_position,
        surfaceNormal);
    vec3 viewDirection = normalize(camera_position - world_position);
    vec3 reflected = reflect(-viewDirection, surfaceNormal);
    vec3 reflectedRadiance = surfaceCacheReflection(
        world_position + reflected * 0.05, reflected, roughness);
    vec3 fresnelZero = mix(vec3(0.04), baseColor, metallic);
    float specularStrength = (1.0 - roughness) * (1.0 - roughness);
    vec3 indirectSpecular = reflectedRadiance * fresnelZero * specularStrength;
    vec3 litColor = baseColor * (indirect * orm.r + direct * environmentCloudShadow(world_position)) + indirectSpecular * orm.r;
    vec3 linearColor = viewMode == 1u ? baseColor :
        mix(litColor, baseColor, unlit);
    if(viewMode==0u) linearColor = environmentFog(world_position, camera_position, linearColor);
    vec3 encoded = mix(12.92 * linearColor,
        1.055 * pow(max(linearColor, vec3(0.0)), vec3(1.0 / 2.4)) - 0.055,
        greaterThan(linearColor, vec3(0.0031308)));
#ifdef GRAPHENE_LINEAR_COLOR
    output_color=vec4(max(linearColor,vec3(0)),material_base_color.a);
#else
    output_color = vec4(encoded, material_base_color.a);
#endif
}
