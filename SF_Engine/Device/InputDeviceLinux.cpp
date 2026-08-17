#ifdef _PLATFORM_LINUX
#include "InputDevice.hpp"
#include <libudev.h>           
#include <libevdev-1.0/libevdev/libevdev.h> 
#include <fcntl.h>
#include <unistd.h>
#include <cstring>

namespace SF::Engine
{
    static void ProbeEvdevCaps(const char *devNode,
                               uint32_t &outAxes,
                               uint32_t &outButtons)
    {
        int fd = open(devNode, O_RDONLY | O_NONBLOCK);
        if (fd < 0)
            return;

        struct libevdev *ev = nullptr;
        if (libevdev_new_from_fd(fd, &ev) < 0)
        {
            close(fd);
            return;
        }

        for (int code = ABS_X; code <= ABS_MAX; ++code)
            if (libevdev_has_event_code(ev, EV_ABS, code))
                ++outAxes;

        for (int code = BTN_JOYSTICK; code < BTN_DIGI; ++code)
            if (libevdev_has_event_code(ev, EV_KEY, code))
                ++outButtons;

        libevdev_free(ev);
        close(fd);
    }

    static DeviceType ClassifyUdevDevice(struct udev_device *dev,
                                         uint32_t axisCount)
    {
        // udev sets these boolean properties on the input device
        auto prop = [&](const char *key) -> bool
        {
            const char *v = udev_device_get_property_value(dev, key);
            return v && strcmp(v, "1") == 0;
        };

        if (prop("ID_INPUT_JOYSTICK"))
        {
            // Yokes typically have >=4 axes (X, Y, Z, Rz at minimum)
            // and more buttons than a simple stick
            return (axisCount >= 5) ? DeviceType::ControlYoke
                                    : DeviceType::FlightStick;
        }
        if (prop("ID_INPUT_TABLET"))
            return DeviceType::INVALID;

        return DeviceType::INVALID;
    }

    std::vector<Device_T> EnumerateDevices()
    {
        std::vector<Device_T> devices;

        udev *ud = udev_new();
        if (!ud)
            return devices;

        udev_enumerate *en = udev_enumerate_new(ud);
        udev_enumerate_add_match_subsystem(en, "input");
        udev_enumerate_scan_devices(en);

        udev_list_entry *entry = nullptr;
        udev_list_entry_foreach(entry, udev_enumerate_get_list_entry(en))
        {
            const char *path = udev_list_entry_get_name(entry);
            udev_device *dev = udev_device_new_from_syspath(ud, path);
            if (!dev)
                continue;

            const char *devNode = udev_device_get_devnode(dev);
            // Only care about /dev/input/eventN nodes (not /dev/input/jsN)
            if (!devNode || strncmp(devNode, "/dev/input/event", 16) != 0)
            {
                udev_device_unref(dev);
                continue;
            }

            // Walk up to the parent USB/HID device for friendly name
            udev_device *parent = udev_device_get_parent_with_subsystem_devtype(
                dev, "usb", "usb_device");

            const char *name = udev_device_get_property_value(dev, "ID_MODEL");
            if (!name && parent)
                name = udev_device_get_sysattr_value(parent, "product");
            if (!name)
                name = devNode; // fallback to path

            const char *vid = udev_device_get_property_value(dev, "ID_VENDOR_ID");
            const char *pid = udev_device_get_property_value(dev, "ID_MODEL_ID");

            uint32_t axes = 0, buttons = 0;
            ProbeEvdevCaps(devNode, axes, buttons);

            Device_T d;
            d.m_Name = name;
            d.m_ProductId = std::string(vid ? vid : "0000") + ":" +
                            std::string(pid ? pid : "0000");
            d.m_Handle = {static_cast<uint64_t>(
                std::hash<std::string>{}(devNode))};
            d.m_AxisCount = axes;
            d.m_ButtonCount = buttons;
            d.m_Type = ClassifyUdevDevice(dev, axes);

            devices.push_back(std::move(d));
            udev_device_unref(dev);
        }

        udev_enumerate_unref(en);
        udev_unref(ud);
        return devices;
    }

} // namespace SF::Engine
#endif // _PLATFORM_LINUX