struct AnisotropicSurfaceData
{
    float3 tangent;
    float3 bitangent;
    float2 anisotropyFactors;

    [mutating]
    void Init(
        float3 normal,
        float3 inTangent,
        float3 inBitangent,
        float anisotropyAngle,
        float anisotropyFactor,
        float roughnessA)
    {
        anisotropyFactors = max(
            float2(0.01, 0.01),
            float2(
                roughnessA * (1.0 + anisotropyFactor),
                roughnessA * (1.0 - anisotropyFactor)
            )
        );

        float3 finalTangent = inTangent;

        if (anisotropyAngle > 0.01)
        {
            float aniSin;
            float aniCos;
            sincos(anisotropyAngle, aniSin, aniCos);

            finalTangent =
                aniCos * inTangent -
                aniSin * inBitangent;
        }

        bitangent = normalize(cross(normal, finalTangent));
        tangent = normalize(cross(bitangent, normal));
    }
};