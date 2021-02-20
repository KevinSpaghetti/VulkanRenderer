#version 450 core
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec2 inUv;
layout(location = 2) in vec3 cameraPos;
layout(location = 3) in vec3 inWorldPos;
layout(location = 4) in vec3 inPos;
layout(location = 5) in mat4 inViewMatrix;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler2D albedoTexture;
layout(set = 1, binding = 1) uniform sampler2D normalTexture;
layout(set = 1, binding = 2) uniform sampler2D specularTexture;
layout(set = 1, binding = 3) uniform sampler2D emissiveTexture;
layout(set = 1, binding = 4) uniform samplerCube envmap;

// constant light position, only one light source for testing (treated as point light)
const vec3 lightPosition = vec3(0, 3, 2);
const float lightIntensity = 1.0;

//Values of textures at point
vec3 pointDiffuse;
float pointSpecular;
float pointRoughness;
vec3 pointNormal;
vec3 pointEmission;

const float PI = 3.14159;

vec3 FSchlick(vec3 l, vec3 h){
    float lDoth = max(dot(l,h), 0.000001);
    return (pointSpecular + (vec3(1.0) - pointSpecular) * pow(1.0 - lDoth, 5.0));
}
float DGGX(vec3 n, vec3 h, float alpha){
    float nDoth = max(dot(n,h), 0.000001);
    float alpha2 = alpha * alpha;
    float d = nDoth * nDoth * (alpha2 - 1.0)+1.0;
    return (alpha2 / (PI*d*d));
}
float G1(vec3 a, vec3 b, float k){
    float aDotb = max(dot(a,b), 0.000001);
    return (aDotb / (aDotb * (1.0 - k) + k) );
}
float GSmith(vec3 n, vec3 v, vec3 l){
    float k = pointRoughness * pointRoughness;
    return G1(n, l, k) * G1(n, v, k);
}
vec3 perturbNormal2Arb( vec3 eye_pos, vec3 surf_norm ) {
    vec3 q0 = dFdx( eye_pos.xyz );
    vec3 q1 = dFdy( eye_pos.xyz );
    vec2 st0 = dFdx( inUv.st );
    vec2 st1 = dFdy( inUv.st );
    vec3 S = normalize(  q0 * st1.t - q1 * st0.t );
    vec3 T = normalize( -q0 * st1.s + q1 * st0.s );
    vec3 N =  surf_norm ;
    vec3 mapN = normalize(texture( normalTexture, inUv ).xyz * 2.0 - 1.0);
    mat3 tsn = mat3( S, T, N );
    return normalize( tsn * mapN );
}
vec3 inverseTransformDirection( in vec3 dir, in mat4 matrix ) {
    return normalize( ( vec4( dir, 0.0 ) * matrix ).xyz );
}

void main(){

    vec3 n = normalize(inNormal);  // interpolation destroys normalization, so we have to normalize
    vec3 v = normalize(-inPos);
    vec3 worldN = inverseTransformDirection(n, inViewMatrix );
    vec3 worldV = cameraPos - inWorldPos;
    vec3 r = normalize(reflect(-worldV,worldN));
    float nDotv = max(dot( n, v ),0.000001);

    pointDiffuse = texture(albedoTexture, inUv).rgb;
    pointSpecular = texture(specularTexture, inUv).g;
    pointRoughness = texture(specularTexture, inUv).b;

    pointDiffuse = pow(pointDiffuse, vec3(2.2));

    float lightDistance = distance(lightPosition, inPos);

    vec4 lPosition = vec4( lightPosition, 1.0 );
    vec3 l = normalize(lPosition.xyz - inPos.xyz);
    vec3 h = normalize( v + l);
    // small quantity to prevent divisions by 0
    float nDotl = max(dot( n, l ),0.000001);
    float lDoth = max(dot( l, h ),0.000001);
    float nDoth = max(dot( n, h ),0.000001);
    float vDoth = max(dot( v, h ),0.000001);

    vec3 fresnel = FSchlick(l, h);
    float geometryFactor = GSmith(n, v, l);
    float normalDistribution = DGGX(n, h, pointRoughness*pointRoughness);

    vec3 diffuse = pointDiffuse/PI * nDotl;
    vec3 specularBRDF = (fresnel * normalDistribution * geometryFactor) / (4.0 * nDotl * nDotv);

    vec3 outRadiance = texture(emissiveTexture, inUv).rgb + (1.0 - fresnel) * diffuse + specularBRDF;

    vec4 color = vec4(pow(vec3(outRadiance), vec3(1.0/2.2)),1.0);
    outColor = color;
}