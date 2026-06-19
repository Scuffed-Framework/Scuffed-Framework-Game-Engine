#include "AerialPerspectiveLUT.hpp"
#include "MultiScatterLUT.hpp"
#include "SkyViewLUT.hpp"
#include "TransmittanceLUT.hpp"

namespace SF::Engine
{
    struct AtmosphereFrameUBO;
    class AtmoLUTs
    {
    public:
        struct NoInit
        {
        };
        explicit AtmoLUTs(NoInit) {}

        AtmoLUTs()
        {
            transmittanceLUT_ = std::make_unique<TransmittanceLUT>(256, 64);
            {
                CommandBuffer cmd(true); // begin = true
                transmittanceLUT_->Bake(cmd);
                cmd.SubmitIdle(); // blocks until compute is done
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

        TransmittanceLUT *GetTransmittanceLUT() { return transmittanceLUT_.get(); }
        MultiScatterLUT *GetMultiScatterLUT() { return multiScatterLUT_.get(); }
        SkyViewLUT *GetSkyViewLUT() { return skyViewLUT_.get(); }
        AerialPerspectiveLUT *GetAerialPerspectiveLUT() { return aerialPerspectiveLUT_.get(); }
        AtmoLUTs(const AtmoLUTs &) = delete;
        AtmoLUTs &operator=(const AtmoLUTs &) = delete;

    private:
        std::unique_ptr<TransmittanceLUT> transmittanceLUT_;
        std::unique_ptr<MultiScatterLUT> multiScatterLUT_;
        std::unique_ptr<SkyViewLUT> skyViewLUT_;
        std::unique_ptr<AerialPerspectiveLUT> aerialPerspectiveLUT_;
    };
}