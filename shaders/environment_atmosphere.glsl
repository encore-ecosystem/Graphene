// Dynamic LUT atmosphere in kilometres: exponential Rayleigh/Mie + ozone.
// Hillaire (2020): transmittance, isotropic multiple scattering and a per-view sky LUT.
const float ATM_PI=3.14159265359;
const float ATM_GROUND=6360.;
const float ATM_TOP=6460.;
vec2 atmosphereInterval(vec3 p,vec3 d,float radius) {
    float b=dot(p,d),c=dot(p,p)-radius*radius,disc=b*b-c;
    if(disc<0.)return vec2(-1.);
    float root=sqrt(disc);return vec2(-b-root,-b+root);
}
float atmosphereDistance(vec3 p,vec3 d) {
    float t=atmosphereInterval(p,d,ATM_TOP).y;
    vec2 ground=atmosphereInterval(p,d,ATM_GROUND);
    if(ground.x>0.)t=min(t,ground.x);
    return max(t,0.);
}
void atmosphereMedium(vec3 p,out vec3 scatteringR,out vec3 scatteringM,out vec3 extinction) {
    float h=max(length(p)-ATM_GROUND,0.);
    scatteringR=vec3(.005802,.013558,.0331)*exp(-h/8.);
    float aerosol=max(env.settings[4].w,1.)*.5;
    scatteringM=vec3(.003996*aerosol)*exp(-h/1.2);
    vec3 absorptionM=vec3(.00044*aerosol)*exp(-h/1.2);
    vec3 ozone=vec3(.00065,.001881,.000085)*max(0.,1.-abs(h-25.)/15.);
    extinction=scatteringR+scatteringM+absorptionM+ozone;
}
vec2 atmosphereUv(vec3 p,vec3 direction) {
    float height=clamp((length(p)-ATM_GROUND)/(ATM_TOP-ATM_GROUND),0.,1.);
    return vec2(dot(normalize(p),direction)*.5+.5,sqrt(height));
}
vec3 atmosphereSun(vec3 p,vec3 sun) {
    if(atmosphereInterval(p,sun,ATM_GROUND).x>0.)return vec3(0.);
    return texture(transmittanceLut,atmosphereUv(p,sun)).rgb;
}
vec3 integrateSky(vec3 p,vec3 ray,vec3 sun) {
    float distance=atmosphereDistance(p,ray),mu=dot(ray,sun);
    float phaseR=3./(16.*ATM_PI)*(1.+mu*mu);
    float g=.8,phaseM=3./(8.*ATM_PI)*(1.-g*g)*(1.+mu*mu)/((2.+g*g)*pow(max(1.+g*g-2.*g*mu,.01),1.5));
    vec3 through=vec3(1.),radiance=vec3(0.);
    int steps=env.settings[6].w<.5?16:32;
    for(int i=0;i<32;++i) {
        if(i>=steps)break;
        // Quadratic distribution resolves the dense atmosphere near the camera.
        float a=float(i)/float(steps),b=float(i+1)/float(steps);
        float dt=(b*b-a*a)*distance;
        vec3 sampleP=p+ray*((a*a+b*b)*.5*distance);
        vec3 r,m,e;atmosphereMedium(sampleP,r,m,e);
        vec3 direct=atmosphereSun(sampleP,sun)*(r*phaseR+m*phaseM);
        vec3 multiple=texture(multipleScatteringLut,atmosphereUv(sampleP,sun)).rgb*(r+m);
        vec3 segment=exp(-e*dt);
        radiance+=through*(direct+multiple)*(1.-segment)/max(e,vec3(1e-6));
        through*=segment;
    }
    return radiance*18.;
}
