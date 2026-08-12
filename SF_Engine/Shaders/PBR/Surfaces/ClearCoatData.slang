struct ClearCoatSurfaceData
{
    float factor;
    float roughness;
    float3 normal;

    [mutating]
    void InitializeToZero()
    {
        factor = 0.0;
        roughness = 0.0;
        normal = float3(0.0, 0.0, 0.0);
    }
};