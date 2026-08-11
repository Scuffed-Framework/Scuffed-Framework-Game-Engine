#include <Math/BasicMath.hpp>
namespace SF::Engine
{
    struct AtmosphereSettings
    {
        float timeOfDay;        // 0–24 hrs
        float dayLengthMinutes; // world time speed

        float sunIntensity;       // lux-ish scale
        float sunAngularDiameter; // usually ~0.53 degrees
        Vec3 sunDirection;   // normalized vector

        float moonIntensity;
        Vec3 moonDirection;

        // Rayleigh scattering parameters
        Vec3 rayleighScattering;
        float rayleighHeight;

        // Mie scattering
        Vec3 mieScattering;
        float mieHeight;
        float mieAnisotropy; // 0–1 (forward scattering)

        float ozoneAbsorption;
    };
}