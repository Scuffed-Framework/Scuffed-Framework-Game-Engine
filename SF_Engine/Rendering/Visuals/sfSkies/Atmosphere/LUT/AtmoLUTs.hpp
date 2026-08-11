#pragma once

#include "AerialPerspectiveLUT.hpp"
#include "MultiScatterLUT.hpp"
#include "SkyViewLUT.hpp"
#include "TransmittanceLUT.hpp"
#include <memory>
#include <cassert>

namespace SF::Engine
{
    struct AtmosphereFrameUBO;

    class AtmoLUTs
    {
    public:
        struct NoInit {};
        explicit AtmoLUTs(NoInit) {}

        AtmoLUTs()
        {
            transmittanceLUT_ = std::make_unique<TransmittanceLUT>(256, 64);
            {
                CommandBuffer cmd(true);
                transmittanceLUT_->Bake(cmd);
                cmd.SubmitIdle();
            }
            multiScatterLUT_ = std::make_unique<MultiScatterLUT>(transmittanceLUT_->GetTexture(), 32, 32);
            {
                CommandBuffer cmd(true);
                multiScatterLUT_->Bake(cmd);
                cmd.SubmitIdle();
            }
            skyViewLUT_ = std::make_unique<SkyViewLUT>(transmittanceLUT_->GetTexture(), multiScatterLUT_->GetTexture(), 128, 128);

            aerialPerspectiveLUT_ = std::make_unique<AerialPerspectiveLUT>(transmittanceLUT_->GetTexture(), multiScatterLUT_->GetTexture());
            {
                CommandBuffer cmd(true);
                aerialPerspectiveLUT_->Bake(cmd, AtmosphereFrameUBO());
                cmd.SubmitIdle();
            }
        }

        ~AtmoLUTs() = default;

        AtmoLUTs(const AtmoLUTs &) = delete;
        AtmoLUTs &operator=(const AtmoLUTs &) = delete;
        AtmoLUTs(AtmoLUTs &&) = delete;
        AtmoLUTs &operator=(AtmoLUTs &&) = delete;

        TransmittanceLUT *GetTransmittanceLUT() { return transmittanceLUT_.get(); }
        MultiScatterLUT *GetMultiScatterLUT() { return multiScatterLUT_.get(); }
        SkyViewLUT *GetSkyViewLUT() { return skyViewLUT_.get(); }
        AerialPerspectiveLUT *GetAerialPerspectiveLUT() { return aerialPerspectiveLUT_.get(); }

        // --- Shared-instance access -------------------------------------------------
        // Explicit lifecycle instead of a function-local static: these own Vulkan
        // handles, and the ctor does blocking GPU submits, so both the timing of
        // creation and the timing of teardown (relative to device destruction) need
        // to be caller-controlled, not implicit-first-call / static-deinit-order.

        static void Init()
        {
            assert(!s_Instance && "AtmoLUTs::Init called twice");
            s_Instance = std::make_unique<AtmoLUTs>();
        }

        static void Shutdown()
        {
            // Call before device/instance teardown.
            s_Instance.reset();
        }

        static AtmoLUTs &Get()
        {
            assert(s_Instance && "AtmoLUTs::Get called before Init (or after Shutdown)");
            return *s_Instance;
        }

        static bool IsInitialized() { return s_Instance != nullptr; }

    private:
        std::unique_ptr<TransmittanceLUT> transmittanceLUT_;
        std::unique_ptr<MultiScatterLUT> multiScatterLUT_;
        std::unique_ptr<SkyViewLUT> skyViewLUT_;
        std::unique_ptr<AerialPerspectiveLUT> aerialPerspectiveLUT_;

        static inline std::unique_ptr<AtmoLUTs> s_Instance;
    };
}