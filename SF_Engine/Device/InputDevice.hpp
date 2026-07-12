#pragma once
#include <cstdint>
#include <functional>
#include <vector>
#include <string>

// Just for specialized harware, like flight controls.
namespace SF::Engine
{
    // Platform implementations store a native pointer/ID cast to uint64_t.
    struct Handle
    {
        uint64_t value = 0;

        bool IsValid() const noexcept { return value != 0; }

        bool operator==(const Handle &o) const noexcept { return value == o.value; }
        bool operator!=(const Handle &o) const noexcept { return value != o.value; }
        bool operator<(const Handle &o) const noexcept { return value < o.value; }

        static Handle Invalid() noexcept { return {0}; }
    };

    // we only care about specialized hardware, like flight controls. We don't care about generic gamepads or joysticks. (XInput or DirectInput for those)
    enum class DeviceType : uint8_t
    {
        FlightStick, // Joystick with few axes
        ControlYoke, // Full yoke: pitch/roll/throttle + many buttons
        Throttle,    // Standalone throttle quadrant (HOTAS)
        Rudder,      // Standalone rudder pedals
    };

    struct Device_T
    {
        std::string m_Name;
        std::string m_ProductId; // "VID_XXXX&PID_XXXX" on Win, evdev path on Linux
        Handle m_Handle;
        DeviceType m_Type;
        uint32_t m_AxisCount = 0;
        uint32_t m_ButtonCount = 0;
    };

    std::vector<Device_T> EnumerateDevices();
}

// Allow Handle as a std::unordered_map key
namespace std
{
    template <>
    struct hash<SF::Engine::Handle>
    {
        std::size_t operator()(const SF::Engine::Handle &h) const noexcept
        {
            return std::hash<uint64_t>{}(h.value);
        }
    };
}
