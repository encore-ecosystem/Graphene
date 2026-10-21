#version 450
layout(location = 0) flat in float selected;
layout(location = 0) out vec4 output_color;
void main() {
    // Preserve depth in the remaining 24 bits of the existing UNORM target.
    // The outline pass can reject foreground geometry at the output pixel.
    uint depth = uint(round(clamp(gl_FragCoord.z, 0.0, 1.0) * 16777215.0));
    output_color = vec4(selected,
        float((depth >> 16u) & 255u) / 255.0,
        float((depth >> 8u) & 255u) / 255.0,
        float(depth & 255u) / 255.0);
}
