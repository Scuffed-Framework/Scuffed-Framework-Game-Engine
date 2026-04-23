#pragma once

#include <memory>
#include <Graphics/Images/Image2d.hpp>
#include <Graphics/Images/Image3d.hpp>
#include <Graphics/Pipelines/ComputePipeline.hpp>
#include <Graphics/Descriptors/DescriptorSet.hpp>
#include <Graphics/Commands/CommandBuffer.hpp>

namespace SF::Engine
{
    // =========================================================================
    // BlueNoiseLUT  (2-D, R16_SFLOAT, 128x128 default)
    // Computed by BlueNoiseLUT.shader via IGN layering.
    // =========================================================================
    class BlueNoiseLUT
    {
    public:
        explicit BlueNoiseLUT(uint32_t size = 128);
        ~BlueNoiseLUT() = default;

        Image2d *GetTexture() const { return texture_.get(); }
        void Bake(const CommandBuffer &cmd);

    private:
        void createPipeline();

        std::unique_ptr<Image2d>         texture_;
        std::unique_ptr<ComputePipeline> pipeline_;
        std::unique_ptr<DescriptorSet>   descSet_;
        uint32_t size_;
        bool     baked_ = false;
    };

    // =========================================================================
    // WorleyNoiseLUT3D  (3-D, R8_UNORM, size^3)
    // Four-octave Worley FBM for cloud base shape.
    // =========================================================================
    class WorleyNoiseLUT3D
    {
    public:
        explicit WorleyNoiseLUT3D(uint32_t size = 128);
        ~WorleyNoiseLUT3D() = default;

        Image3d *GetTexture() const { return texture_.get(); }
        void Bake(const CommandBuffer &cmd);

    private:
        void createPipeline();

        std::unique_ptr<Image3d>         texture_;
        std::unique_ptr<ComputePipeline> pipeline_;
        std::unique_ptr<DescriptorSet>   descSet_;
        uint32_t size_;
        bool     baked_ = false;
    };

    // =========================================================================
    // CurlNoiseLUT3D  (3-D, R8G8B8A8_UNORM, size^3)
    // Curl-noise 3-D texture for cloud detail erosion.
    // =========================================================================
    class CurlNoiseLUT3D
    {
    public:
        explicit CurlNoiseLUT3D(uint32_t size = 32);
        ~CurlNoiseLUT3D() = default;

        Image3d *GetTexture() const { return texture_.get(); }
        void Bake(const CommandBuffer &cmd);

    private:
        void createPipeline();

        std::unique_ptr<Image3d>         texture_;
        std::unique_ptr<ComputePipeline> pipeline_;
        std::unique_ptr<DescriptorSet>   descSet_;
        uint32_t size_;
        bool     baked_ = false;
    };

    // =========================================================================
    // CoverageLUT  (2-D, R8_UNORM, width x height)
    // Top-down cloud coverage map.  Can be replaced at runtime with an
    // artist-authored texture by calling SetTexture().
    // =========================================================================
    class CoverageLUT
    {
    public:
        explicit CoverageLUT(uint32_t width = 512, uint32_t height = 512);
        ~CoverageLUT() = default;

        Image2d *GetTexture() const { return texture_.get(); }
        void Bake(const CommandBuffer &cmd);

    private:
        void createPipeline();

        std::unique_ptr<Image2d>         texture_;
        std::unique_ptr<ComputePipeline> pipeline_;
        std::unique_ptr<DescriptorSet>   descSet_;
        uint32_t width_;
        uint32_t height_;
        bool     baked_ = false;
    };

    // =========================================================================
    // ShadowLUT  (2-D, R16_SFLOAT, width x height)
    // Pre-integrated cloud self-shadow lookup.
    // =========================================================================
    class ShadowLUT
    {
    public:
        explicit ShadowLUT(uint32_t width = 256, uint32_t height = 256);
        ~ShadowLUT() = default;

        Image2d *GetTexture() const { return texture_.get(); }
        void Bake(const CommandBuffer &cmd);

    private:
        void createPipeline();

        std::unique_ptr<Image2d>         texture_;
        std::unique_ptr<ComputePipeline> pipeline_;
        std::unique_ptr<DescriptorSet>   descSet_;
        uint32_t width_;
        uint32_t height_;
        bool     baked_ = false;
    };

} // namespace SF::Engine
