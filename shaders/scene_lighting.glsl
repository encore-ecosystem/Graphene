// Graphene forward lighting shared by conventional and virtual mesh paths.
vec3 actorDirectLight(vec3 position, vec3 normal) {
    vec3 radiance = vec3(0.0);
    int count = min(int(actor_lighting[0].x), 64);
    for (int index = 0; index < count; ++index) {
        int base = 1 + index * 4;
        vec4 source = actor_lighting[base];
        vec4 axis = actor_lighting[base + 1];
        vec4 energy = actor_lighting[base + 2];
        vec4 cone = actor_lighting[base + 3];
        vec3 delta = source.xyz - position;
        float distanceSquared = dot(delta, delta);
        bool directional = axis.w > 2.5;
        if (!directional && distanceSquared >= source.w * source.w) continue;
        vec3 toLight = directional ? normalize(-axis.xyz) : delta * inversesqrt(max(distanceSquared, 0.0001));
        float attenuation = 1.0;
        if (!directional) {
            float fade = max(1.0 - pow(distanceSquared / (source.w * source.w), 2.0), 0.0);
            attenuation = fade * fade / max(distanceSquared, 0.04);
            if (axis.w > 1.5) {
                float outer = cos(radians(cone.y));
                float inner = cos(radians(cone.x));
                float spot = clamp((dot(-toLight, normalize(axis.xyz)) - outer) / max(inner - outer, 0.00001), 0.0, 1.0);
                attenuation *= spot * spot * (3.0 - 2.0 * spot);
            }
        }
        float cosine=max(dot(normal,toLight),0.);
        if(cosine>0. && attenuation>0.)
            radiance += energy.rgb * energy.w * attenuation * cosine *
                lightingVisibility(position,normal,toLight,directional?10000.:sqrt(distanceSquared));
    }
    return radiance;
}
