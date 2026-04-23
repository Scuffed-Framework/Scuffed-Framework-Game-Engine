#include <thread>
#include <ID/GUID.hpp>

namespace SF::Engine
{
    class Job
    {
    public:
        GUID guid;
        virtual void Execute();
        virtual GUID getGUID() { return guid; }
    };
}