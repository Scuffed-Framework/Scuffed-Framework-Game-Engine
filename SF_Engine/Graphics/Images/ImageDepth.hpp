#pragma once

#include "Image.hpp"

namespace SF::Engine
{
    /**
     * @brief Resource that represents a depth stencil image.
     */
    class ImageDepth : public Image
    {
    public:
        explicit ImageDepth(const Vector2Uint &extent, VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT);
    };
}