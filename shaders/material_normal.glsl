// Tangent-space data maps are linear UNORM, not sRGB.
// Reconstruct the UV frame from derivatives without growing every mesh vertex.
// Dividing by the signed UV determinant preserves mirrored UV handedness.
vec3 materialNormal(sampler2D normalMap, vec3 position, vec3 normal,
                    vec2 uv, float strength, float flipY) {
    vec3 N = normalize(normal);
    if (!gl_FrontFacing) N = -N;
    if (strength <= 0.0) return N;
    vec3 dx = dFdx(position);
    vec3 dy = dFdy(position);
    vec2 ux = dFdx(uv);
    vec2 uy = dFdy(uv);
    float determinant = ux.x * uy.y - ux.y * uy.x;
    if (abs(determinant) < 1e-10) return N;
    vec3 tangent = (dx * uy.y - dy * ux.y) / determinant;
    vec3 bitangent = (dy * ux.x - dx * uy.x) / determinant;
    tangent -= N * dot(N, tangent);
    float lengthSquared = dot(tangent, tangent);
    if (lengthSquared < 1e-12) return N;
    tangent *= inversesqrt(lengthSquared);
    float handedness = dot(cross(N, tangent), bitangent) < 0.0 ? -1.0 : 1.0;
    bitangent = cross(N, tangent) * handedness;
    vec3 sampleNormal = textureGrad(normalMap, uv, ux, uy).xyz * 2.0 - 1.0;
    sampleNormal.xy *= strength;
    sampleNormal.y *= flipY;
    vec3 mapped = tangent * sampleNormal.x + bitangent * sampleNormal.y + N * sampleNormal.z;
    float mappedLength = dot(mapped, mapped);
    return mappedLength > 1e-12 ? mapped * inversesqrt(mappedLength) : N;
}
