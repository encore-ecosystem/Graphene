// The same stable priority ordering and box feather as EnvironmentSystem.sample().
struct EnvironmentWeather {vec4 sky;vec4 wind;vec4 surface;};
EnvironmentWeather environmentWeather(vec3 position) {
    EnvironmentWeather result=EnvironmentWeather(ENV(1),ENV(2),ENV(3));
    int count=min(int(ENV(9).x),64);
    for(int i=0;i<count;++i) {
        int base=10+i*8;vec3 delta=position-ENV(base).xyz;float edge=1e20;
        for(int a=0;a<3;++a) {
            vec3 axis=ENV(base+1+a).xyz;float size=dot(axis,axis);
            float coordinate=dot(delta,axis);
            edge=min(edge,(1.-abs(coordinate))/max(sqrt(size),1e-10));
        }
        float weight=ENV(base).w<=0.?(edge>=0.?1.:0.):smoothstep(0.,ENV(base).w,edge);
        result.sky=mix(result.sky,ENV(base+4),weight);
        result.wind=mix(result.wind,ENV(base+5),weight);
        result.surface.xy=mix(result.surface.xy,ENV(base+6).xy,weight);
    }
    return result;
}
