#include <Math/Time/Time.hpp>

namespace SF::Engine
{
    enum SceneTimeMode
    {
        Reatime = 0,
        x2Speed = 1,
        x4Speed = 2,
        x8Speed = 3,
        x16Speed = 4
    };
    struct SceneTime
    {
    public:
        SceneTimeMode mode;
    };
}