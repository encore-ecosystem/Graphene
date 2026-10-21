#version 450
layout(set = 0, binding = 0) uniform Outline { vec4 viewport; } outline;
layout(set = 0, binding = 1) uniform sampler2D selection_silhouette;
layout(set = 0, binding = 2) uniform sampler2D visible_selection;
layout(location = 0) out vec4 output_color;
vec4 mask(sampler2D image, ivec2 point) {
    if (any(lessThan(point, ivec2(0))) || any(greaterThanEqual(point, ivec2(outline.viewport.zw)))) return vec4(0.0, 1.0, 1.0, 1.0);
    return texelFetch(image, point, 0);
}
float depth(vec4 sample_value) {
    vec3 bytes = round(sample_value.gba * 255.0);
    return dot(bytes, vec3(65536.0, 256.0, 1.0)) / 16777215.0;
}
void main() {
    ivec2 p = ivec2(gl_FragCoord.xy - outline.viewport.xy);
    // Find the object's own silhouette BEFORE applying visibility. An
    // occluder must cut the line, never introduce its curved boundary into it.
    if (mask(selection_silhouette, p).r > 0.5) discard;
    float foreground_depth = depth(mask(visible_selection, p));
    float edge = 0.0;
    for (int y = -2; y <= 2; ++y) {
        for (int x = -2; x <= 2; ++x) {
            if (x*x + y*y > 5) continue;
            ivec2 q = p + ivec2(x, y);
            vec4 selected = mask(selection_silhouette, q);
            if (selected.r > 0.5 && mask(visible_selection, q).r > 0.5 &&
                foreground_depth + 2.0 / 16777215.0 >= depth(selected)) edge = 1.0;
        }
    }
    if (edge < 0.5) discard;
    output_color = vec4(1.0, 0.53, 0.08, 1.0);
}
