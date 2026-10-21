#version 460
layout(location=2) flat in vec4 material_base_color;
layout(location=0) out vec4 output_color;
void main() {
    float selected=material_base_color.x;
    if(material_base_color.y>0.5 && selected<0.5) discard;
    uint depth=uint(round(clamp(gl_FragCoord.z,0.0,1.0)*16777215.0));
    output_color=vec4(selected,float((depth>>16u)&255u)/255.0,float((depth>>8u)&255u)/255.0,float(depth&255u)/255.0);
}
