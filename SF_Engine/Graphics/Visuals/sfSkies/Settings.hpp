#include <glm/glm.hpp>
namespace SF::Engine
{
    struct AtmosphereSettings
    {
        float timeOfDay;        // 0–24 hrs
        float dayLengthMinutes; // world time speed

        float sunIntensity;       // lux-ish scale
        float sunAngularDiameter; // usually ~0.53 degrees
        glm::vec3 sunDirection;   // normalized vector

        float moonIntensity;
        glm::vec3 moonDirection;

        // Rayleigh scattering parameters
        glm::vec3 rayleighScattering;
        float rayleighHeight;

        // Mie scattering
        glm::vec3 mieScattering;
        float mieHeight;
        float mieAnisotropy; // 0–1 (forward scattering)

        float ozoneAbsorption;
    };
}