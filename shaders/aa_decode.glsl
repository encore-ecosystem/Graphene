vec3 aaDecode(vec3 encoded) {
    return mix(encoded/12.92,pow(max((encoded+0.055)/1.055,vec3(0)),vec3(2.4)),greaterThan(encoded,vec3(0.04045)));
}
