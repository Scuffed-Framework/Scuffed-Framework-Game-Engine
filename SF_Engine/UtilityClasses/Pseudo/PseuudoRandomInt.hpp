#include <chrono>  // for seeding
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
                seed);  // Mersenne Twister engine is a good general-purpose choice

            // 2. Define the desired distribution
            std::uniform_int_distribution<int> distribution(1, 100);  // Range [1, 100]

            // 3. Generate a number
            T random_num = distribution(generator);
        }
    };
}