//-----------------------------------------------------------------------------
// Mesh rendering shader
//
// Copyright 2016 Aleksey Egorov
//-----------------------------------------------------------------------------
uniform vec3 lightDir0;
uniform vec3 lightDir1;
uniform float lightInt0;
uniform float lightInt1;
uniform float ambient;
uniform float matcap;

varying vec3 fragNormal;
varying vec4 fragColor;

// A studio light fixed to the view (a matcap, #19): soft light from above left, a sky above
// and a floor below, a highlight, and the rim brighter where the surface turns away.
vec3 Matcap(vec3 base, vec3 n) {
    vec3  key   = normalize(vec3(-0.4, 0.6, 0.7));
    float wrap  = clamp((dot(n, key) + 0.3) / 1.3, 0.0, 1.0);
    float sky   = 0.5 + 0.5 * n.y;
    vec3  half_ = normalize(key + vec3(0.0, 0.0, 1.0));
    float spec  = pow(clamp(dot(n, half_), 0.0, 1.0), 48.0);
    float rim   = pow(1.0 - clamp(abs(n.z), 0.0, 1.0), 3.0);
    vec3  c     = base * (0.3 + 0.5 * wrap + 0.2 * sky);
    return c + vec3(0.35) * spec + vec3(0.18) * rim;
}

void main() {
    vec3 normal = normalize(fragNormal);
    if(matcap > 0.5) {
        // The view looks along +z here (the lights' z is turned the same way).
        gl_FragColor = vec4(Matcap(fragColor.rgb, normal * vec3(1.0, 1.0, -1.0)), fragColor.a);
        return;
    }
    vec3 result = fragColor.xyz * ambient;

    float light0 = clamp(dot(lightDir0, normal), 0.0, 1.0) * lightInt0 * (1.0 - ambient);
    result += fragColor.rgb * light0;

    float light1 = clamp(dot(lightDir1, normal), 0.0, 1.0) * lightInt1 * (1.0 - ambient);
    result += fragColor.rgb * light1;

    gl_FragColor = vec4(result, fragColor.a);
}
