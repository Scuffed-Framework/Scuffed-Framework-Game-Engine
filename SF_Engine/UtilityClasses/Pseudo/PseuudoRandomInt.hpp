#include <chrono> // for seeding
#include <iostream>
#include <random>

namespace SF::Engine
{
    template <typename T>
    class PseudoRandomInt
    {
    public:
        unsigned seed;
        static T generate()
        {
            seed = std::chrono::system_clock::now().time_since_epoch().count();
            std::mt19937 generator(
                seed);
            std::uniform_int_distribution<int> distribution(1, 100); // Range [1, 100]

            T random_num = distribution(generator);
        }
    };
}