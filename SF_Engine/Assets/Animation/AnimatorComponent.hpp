#pragma once
#include <EntityComponentSystem/Component.hpp>
#include <map>
#include "Animation.hpp"
#include "Joint/AnimJoint.hpp"

namespace SF::Engine::Animation
{
    struct Keyframe;
    using namespace std;
    class AnimatorComponent : public Component::Registrar<AnimatorComponent>
    {
        bool playing = false;
        ApplicationTime animTime;
        unique_ptr<Animation> animation = nullptr;

    public:
        void Reset() override;

        void Update() override
        {
            // if (playing)
            //     PlayAnimation();
        }

        [[nodiscard]] map<std::string, Mat4> CalculateCurrentAnimationPose() const;
        [[nodiscard]] pair<Keyframe, Keyframe> GetSurroundingKeyframes() const;
        [[nodiscard]] float CalculateProgression(const Keyframe &previousFrame, const Keyframe &nextFrame) const;

        // void PlayAnimation(const Joint &Root, vector<Mat4> &Matrices) {}
    };
} // namespace SF::Engine::Animation
