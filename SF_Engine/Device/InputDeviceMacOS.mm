#ifdef _PLATFORM_MACOS
#include "InputDevice.h"
#import <IOKit/hid/IOHIDManager.h>
#include <CoreFoundation/CoreFoundation.h>

namespace SF::Engine
{
    static int32_t HIDIntProp(IOHIDDeviceRef dev, CFStringRef key)
    {
        CFTypeRef ref = IOHIDDeviceGetProperty(dev, key);
        int32_t val = 0;
        if (ref && CFGetTypeID(ref) == CFNumberGetTypeID())
            CFNumberGetValue((CFNumberRef)ref, kCFNumberSInt32Type, &val);
        return val;
    }

    static std::string HIDStringProp(IOHIDDeviceRef dev, CFStringRef key)
    {
        CFTypeRef ref = IOHIDDeviceGetProperty(dev, key);
        if (!ref || CFGetTypeID(ref) != CFStringGetTypeID())
            return {};
        char buf[256] = {};
        CFStringGetCString((CFStringRef)ref, buf, sizeof(buf), kCFStringEncodingUTF8);
        return buf;
    }

    // IOKit usage pages we care about
    //   kHIDPage_GenericDesktop / kHIDUsage_GD_Joystick  = 0x04
    //   kHIDPage_GenericDesktop / kHIDUsage_GD_GamePad   = 0x05
    static DeviceType ClassifyHIDDevice(IOHIDDeviceRef dev)
    {
        int32_t usagePage = HIDIntProp(dev, CFSTR(kIOHIDPrimaryUsagePageKey));
        int32_t usage = HIDIntProp(dev, CFSTR(kIOHIDPrimaryUsageKey));

        if (usagePage == kHIDPage_GenericDesktop)
        {
            switch (usage)
            {
            case kHIDUsage_GD_Joystick:
            {
                // Count axes to differentiate yoke vs stick
                CFArrayRef elements = IOHIDDeviceCopyMatchingElements(
                    dev, nullptr, kIOHIDOptionsTypeNone);
                if (!elements)
                    return DeviceType::FlightStick;

                uint32_t axisCount = 0;
                for (CFIndex i = 0; i < CFArrayGetCount(elements); ++i)
                {
                    auto *el = (IOHIDElementRef)CFArrayGetValueAtIndex(elements, i);
                    if (IOHIDElementGetType(el) == kIOHIDElementTypeInput_Axis)
                        ++axisCount;
                }
                CFRelease(elements);
                return (axisCount >= 5) ? DeviceType::ControlYoke
                                        : DeviceType::FlightStick;
            }
            default:
                break;
            }
        }
        return DeviceType::Unknown;
    }

    static void CountHIDCaps(IOHIDDeviceRef dev,
                             uint32_t &outAxes, uint32_t &outButtons)
    {
        CFArrayRef elements = IOHIDDeviceCopyMatchingElements(
            dev, nullptr, kIOHIDOptionsTypeNone);
        if (!elements)
            return;

        for (CFIndex i = 0; i < CFArrayGetCount(elements); ++i)
        {
            auto *el = (IOHIDElementRef)CFArrayGetValueAtIndex(elements, i);
            IOHIDElementType t = IOHIDElementGetType(el);
            if (t == kIOHIDElementTypeInput_Axis)
                ++outAxes;
            if (t == kIOHIDElementTypeInput_Button)
                ++outButtons;
        }
        CFRelease(elements);
    }

    std::vector<Device_T> EnumerateDevices()
    {
        std::vector<Device_T> devices;

        IOHIDManagerRef mgr = IOHIDManagerCreate(kCFAllocatorDefault,
                                                 kIOHIDOptionsTypeNone);
        if (!mgr)
            return devices;

        // Null matching dict = match everything
        IOHIDManagerSetDeviceMatching(mgr, nullptr);
        IOHIDManagerOpen(mgr, kIOHIDOptionsTypeNone);

        CFSetRef deviceSet = IOHIDManagerCopyDevices(mgr);
        if (!deviceSet)
        {
            CFRelease(mgr);
            return devices;
        }

        CFIndex count = CFSetGetCount(deviceSet);
        std::vector<const void *> rawDevs(count);
        CFSetGetValues(deviceSet, rawDevs.data());

        for (CFIndex i = 0; i < count; ++i)
        {
            auto *dev = (IOHIDDeviceRef)rawDevs[i];

            std::string name = HIDStringProp(dev, CFSTR(kIOHIDProductKey));
            int32_t vid = HIDIntProp(dev, CFSTR(kIOHIDVendorIDKey));
            int32_t pid = HIDIntProp(dev, CFSTR(kIOHIDProductIDKey));

            char pidBuf[32];
            snprintf(pidBuf, sizeof(pidBuf), "%04X:%04X", vid, pid);

            uint32_t axes = 0, buttons = 0;
            CountHIDCaps(dev, axes, buttons);

            Device_T d;
            d.m_Name = name.empty() ? "Unknown HID Device" : name;
            d.m_ProductId = pidBuf;
            d.m_Handle = {reinterpret_cast<uint64_t>(dev)};
            d.m_AxisCount = axes;
            d.m_ButtonCount = buttons;
            d.m_Type = ClassifyHIDDevice(dev);

            devices.push_back(std::move(d));
        }

        CFRelease(deviceSet);
        IOHIDManagerClose(mgr, kIOHIDOptionsTypeNone);
        CFRelease(mgr);
        return devices;
    }

} // namespace SF::Engine
#endif // _PLATFORM_MACOS