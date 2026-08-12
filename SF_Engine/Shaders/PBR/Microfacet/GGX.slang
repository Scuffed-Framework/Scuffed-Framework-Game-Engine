#include <Common/Math.si>
#define GGX_EPSILON 1e-8
float NormalDistributionGGX(const float NdotH, const float roughnessA2)
{
    // Walter equation 33, simplified given that the positive characteristic function is handled elsewhere
    float b = (NdotH * roughnessA2 - NdotH) * NdotH + 1.0;
    return roughnessA2 / (PI_ * b * b);
}

//! (G) Geometric Shadowing and Masking Distribution function for GGX
//! Variant with height correlated Smith
//! Note: float precision needed here. Converting to float results in artifacts.
//!
//! @param N            macrosurface normal
//! @param V            direction to the camera
//! @param L            direction to the light
//! @param roughnessA2  alpha roughness ^ 2 (alpha roughness is the unpacked form of artist authored linear roughness and is what is used for lighting calculations)
//! @return             probability that V and L are not masked.
float GeometricShadowingMaskingGGXCorrelated(const float NdotV, const float NdotL, const float roughnessA2)
{
    // See Frostbite PBR guide (page 12)
    // https://media.contentapi.ea.com/content/dam/eacom/frostbite/files/course-notes-moving-frostbite-to-pbr-v32.pdf
    float ggxV = NdotL * sqrt((-NdotV * roughnessA2 + NdotV) * NdotV + roughnessA2);
    float ggxL = NdotV * sqrt((-NdotL * roughnessA2 + NdotL) * NdotL + roughnessA2);
    return 0.5 / max(ggxV + ggxL, GGX_EPSILON);
}


// ------- Anisotropic GGX -------

//! (D) Normal Distribution function for anisotropic GGX
//!
//! @param h    - half vector viewer to the light, used to approximate the reflection probability for a microfacet normal at this angle
//! @param t    - rotated surface tangent for calculating anisotropic response
//! @param b    - rotated surface bitangent for calculating anisotropic response
//! @param anisotropyFactors - anisotropic tangent and bitangent factor 
float NormalDistibution_AnisotropicGGX(const float NdotH, const float3 h, const float3 t, const float3 b, float2 anisotropyFactors )
{
    const float atFactor = anisotropyFactors.x;
    const float abFactor = anisotropyFactors.y;
    float TdotH = dot(t, h);
    float BdotH = dot(b, h);
    float a2 = atFactor * abFactor;
    float3 v = float3( abFactor * TdotH, atFactor * BdotH, a2 * NdotH);
    float v2 = dot(v, v);
    float w2 = a2 / max(F_EPSILON, v2);
    return a2 * w2 * w2 * (1.0 / PI_);
}

//! (G) Geometric shadowing and masking distribution function for anisotropic GGX
//! @param t    - rotated surface tangent for calculating anisotropic response
//! @param b    - rotated surface bitangent for calculating anisotropic response
//! @param anisotropyFactors - anisotropic tangent and bitangent factor 
float ShadowingMasking_AnisotropicSmithGGXCorrelated( 
    const float3 t, const float3 b, const float3 dirToCamera, const float3 dirToLight, 
    float NdotV, float NdotL, float2 anisotropyFactors ) 
{
    const float atFactor = anisotropyFactors.x;
    const float abFactor = anisotropyFactors.y;
    float TdotV = dot(t, dirToCamera);
    float BdotV = dot(b, dirToCamera);
    float TdotL = dot(t, dirToLight);
    float BdotL = dot(b, dirToLight);
    float lambdaV = NdotL * length(float3(atFactor * TdotV, abFactor * BdotV, NdotV));
    float lambdaL = NdotV * length(float3(atFactor * TdotL, abFactor * BdotL, NdotL));
    float v = 0.5 / (lambdaV + lambdaL);
    return min(v, F16MAX);
}


// ------- Importance Sampling -------

//! Importance-sample a microgeometry normal using GGX distribution.
//!
//! @param xi           random values in range [0,1). (usually a hammersley-generated distribution)
//! @param roughnessA   standard alpha roughness value used for microfacet calculations
//! @param normalWS     normal of the macro surface in world-space
//! @return             micro-surface normal (in world-space) that can be used as input to other micro-surface equations (called "m" in the linked article)
float3 ImportanceSampleGGX(float2 xi, float roughnessA, float3 normalWS)
{
    float roughnessA2 = roughnessA * roughnessA;

    float phi = 2.0 * PI_ * xi.x; 

    float cosTheta = sqrt((1.0 - xi.y) / (1.0 + (roughnessA2 - 1.0) * xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    float3 micronormalTS = float3(
        sinTheta * cos(phi),
        sinTheta * sin(phi),
        cosTheta
    );

    float3 upVector = abs(normalWS.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
    float3 tangentWS = normalize(cross(upVector, normalWS));
    float3 bitangentWS = cross(normalWS, tangentWS);
    float3 micronormalWS = (tangentWS * micronormalTS.x) + (bitangentWS * micronormalTS.y) + (normalWS * micronormalTS.z);

    return micronormalWS;
}
