#include <Graphics/Commands/CommandBuffer.hpp>
#include <UtilityClasses/NoCopy.hpp>
namespace SF::Engine
{
    // an example would be imgui
    class InjectablePass : NoCopy
    {
    public:
        virtual ~InjectablePass() = default;

        /// Optional - compute/barriers before rendering anything
        virtual void PreInject(const CommandBuffer &cmd) {}

        /// Required - perform the actual injected render work
        virtual void RenderInject(const CommandBuffer &cmd) = 0;

        bool IsEnabled() const { return enabled; }
        void SetEnabled(bool e) { enabled = e; }

    private:
        bool enabled = true;
    };
}