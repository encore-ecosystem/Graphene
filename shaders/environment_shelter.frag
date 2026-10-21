#version 450
layout(location=0) in float height;
layout(location=0) out vec4 outputColor;
void main(){outputColor=vec4(height,0,0,1);}
