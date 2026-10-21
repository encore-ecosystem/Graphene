#version 450
layout(location=0)in vec2 uv;
layout(location=0)out vec4 outputColor;
layout(location=1)out vec4 outputMotion;
layout(set=0,binding=2)uniform sampler2D clouds;
layout(set=0,binding=6)uniform sampler2D cloudDepth;
void main(){
    vec2 size=vec2(textureSize(clouds,0)),position=uv*size-.5,base=floor(position),fraction=fract(position);
    float centerDepth=texture(cloudDepth,uv).x,total=0.;vec4 result=vec4(0.);
    // Bilateral reconstruction preserves cloud silhouettes at depth discontinuities.
    for(int y=0;y<2;++y)for(int x=0;x<2;++x){
        vec2 coord=(base+vec2(x,y)+.5)/size;
        float spatial=(x==0?1.-fraction.x:fraction.x)*(y==0?1.-fraction.y:fraction.y);
        float weight=spatial*exp(-abs(texture(cloudDepth,coord).x-centerDepth)*100.);
        result+=texture(clouds,coord)*weight;total+=weight;
    }
    result/=max(total,.00001);
    outputColor=vec4(result.rgb/max(result.a,.0001),result.a);
    outputMotion=vec4(0,0,1,0);
}
