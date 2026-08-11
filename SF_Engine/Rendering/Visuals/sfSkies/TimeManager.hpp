#pragma once
#include <Math/Time/Time.hpp>
#include <chrono>
namespace SF::Engine
{
    struct SkiesTime
    {
    public:
        static double getJulianDay()
        {
            return 2451545.0;
        }

        static double getGregorianYear()
        {
            return std::chrono::duration<double, std::chrono::years::period>(1).count();
        }

        static double getGregorianMonth()
        {
            return std::chrono::duration<double, std::chrono::months::period>(1).count();
        }

        static double getGregorianDay()
        {
            return std::chrono::duration<double, std::chrono::days::period>(1).count();
        }
    };
}