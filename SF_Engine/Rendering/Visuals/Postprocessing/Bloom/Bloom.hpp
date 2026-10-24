#pragma once
#include <Rendering/RHI/Images/Image.hpp>
#include <Rendering/RHI/Pipelines/ComputePipeline.hpp>

namespace SF::Engine
{
    // inject into frame graph and output to the hdr image
    using namespace std;
    class Bloom
    {
        unique_ptr<Image2d> HDR;
        vector<unique_ptr<ComputePipeline>> DownsamplePasses;
        unique_ptr<ComputePipeline> CompositePass;

    public:
    };
} // namespace SF::Engine
