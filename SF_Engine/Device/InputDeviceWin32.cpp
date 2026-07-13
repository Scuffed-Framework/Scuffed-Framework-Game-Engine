#ifdef _PLATFORM_WINDOWS
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define DIRECTINPUT_VERSION 0x0800
#include "InputDevice.hpp"
#include <dinput.h>
#include <wbemidl.h>
#include <oleauto.h>
#include <windows.h>
#include <hidsdi.h>
#include <setupapi.h>

namespace SF::Engine
{
    static DeviceType ClassifyDIDevice(const DIDEVICEINSTANCE &inst,
                                       uint32_t axisCount,
                                       uint32_t buttonCount)
    {
        const DWORD sub = GET_DIDEVICE_SUBTYPE(inst.dwDevType);
        const DWORD cat = GET_DIDEVICE_TYPE(inst.dwDevType);

        if (cat == DI8DEVTYPE_FLIGHT)
        {
            // These subtypes DO exist in dinput.h
            if (sub == DI8DEVTYPEFLIGHT_YOKE)
                return DeviceType::ControlYoke;
            if (sub == DI8DEVTYPEFLIGHT_STICK)
                return DeviceType::FlightStick;
            if (sub == DI8DEVTYPEFLIGHT_RC)
                return DeviceType::FlightStick;
            return DeviceType::FlightStick; // DI8DEVTYPEFLIGHT_UNKNOWN fallback
        }
        if (cat == DI8DEVTYPE_SUPPLEMENTAL)
        {
            if (sub == DI8DEVTYPESUPPLEMENTAL_THROTTLE)
                return DeviceType::Throttle;
            if (sub == DI8DEVTYPESUPPLEMENTAL_RUDDERPEDALS)
                return DeviceType::Rudder;
        }

        if (axisCount >= 4)
            return DeviceType::FlightStick;
    }

    struct EnumCtx
    {
        IDirectInput8 *di8 = nullptr;
        std::vector<Device_T> *out = nullptr;
    };

    struct CapCtx
    {
        uint32_t axisCount = 0;
        uint32_t buttonCount = 0;
    };

    static BOOL CALLBACK CountAxesCallback(const DIDEVICEOBJECTINSTANCE *obj, void *ctx)
    {
        auto *c = static_cast<CapCtx *>(ctx);
        if (obj->dwType & DIDFT_AXIS)
            ++c->axisCount;
        if (obj->dwType & DIDFT_BUTTON)
            ++c->buttonCount;
        return DIENUM_CONTINUE;
    }

    static BOOL CALLBACK DIEnumDevicesCallback(const DIDEVICEINSTANCE *inst, void *ctx)
    {
        auto *ec = static_cast<EnumCtx *>(ctx);

        IDirectInputDevice8 *dev = nullptr;
        if (FAILED(ec->di8->CreateDevice(inst->guidInstance, &dev, nullptr)))
            return DIENUM_CONTINUE;

        // Count axes & buttons
        CapCtx caps{};
        dev->EnumObjects(CountAxesCallback, &caps, DIDFT_AXIS | DIDFT_BUTTON);
        dev->Release();

        // Build a stable product-id string  "VID_045E&PID_028E"
        wchar_t guidStr[64];
        StringFromGUID2(inst->guidProduct, guidStr, 64);
        char pidBuf[64];
        WideCharToMultiByte(CP_UTF8, 0, guidStr, -1, pidBuf, 64, nullptr, nullptr);

        Device_T d;
        char nameBuf[260] = {};
        WideCharToMultiByte(CP_UTF8, 0, inst->tszInstanceName, -1,
                            nameBuf, sizeof(nameBuf), nullptr, nullptr);
        d.m_Name = nameBuf;
        d.m_ProductId = pidBuf;
        d.m_Handle = {reinterpret_cast<uint64_t>(&inst->guidInstance)};
        d.m_AxisCount = caps.axisCount;
        d.m_ButtonCount = caps.buttonCount;
        d.m_Type = ClassifyDIDevice(*inst, caps.axisCount, caps.buttonCount);

        ec->out->push_back(std::move(d));
        return DIENUM_CONTINUE;
    }

    std::vector<Device_T> EnumerateDevices()
    {
        std::vector<Device_T> devices;

        IDirectInput8 *di8 = nullptr;
        HRESULT hr = DirectInput8Create(
            GetModuleHandle(nullptr),
            DIRECTINPUT_VERSION,
            IID_IDirectInput8,
            reinterpret_cast<void **>(&di8),
            nullptr);

        if (FAILED(hr))
            return devices;

        EnumCtx ctx{di8, &devices};

        // Enumerate ALL devices (keyboard, mouse, joystick, flight hardware)
        di8->EnumDevices(DI8DEVCLASS_ALL,
                         DIEnumDevicesCallback,
                         &ctx,
                         DIEDFL_ATTACHEDONLY); // only currently connected devices

        di8->Release();
        return devices;
    }

} // namespace SF::Engine
#endif //_PLATFORM_WINDOWS