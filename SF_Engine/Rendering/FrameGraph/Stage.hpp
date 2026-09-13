#pragma once

#include <map>
#include "../Material/Color/Color.hpp"
#include "../RHI/Images/ImageDepth.hpp"
#include "../RHI/Renderpass/FrameBuffer.hpp"
#include "../RHI/Renderpass/RhiRenderpass.hpp"
#include "../RHI/Renderpass/RhiSwapchain.hpp"
#include "Math/Vectors/Vector.hpp"

namespace SF::Engine
{
    using namespace std;
    /**
     * @brief Class that represents an attachment in a renderpass.
     */
    class RhiAttachment
    {
    public:
        enum class Type
        {
            Image,
            Depth,
            Swapchain,
            RenderPass // For user-specified fullscreen effects in the future
        };

        /**
         * Creates a new attachment that represents a object in the render pipeline.
         * @param binding The index the attachment is bound to in the renderpass.
         * @param name The unique name given to the object for all renderpasses.
         * @param type The attachment type this represents.
         * @param multisampled If this attachment is multisampled.
         * @param format The format that will be created (only applies to type ATTACHMENT_IMAGE).
         * @param clearColor The Color to clear to before rendering to it.
         */
        RhiAttachment(uint32_t binding, string name, Type type, bool multisampled = false,
                      VkFormat format = VK_FORMAT_R8G8B8A8_UNORM, const Color &clearColor = Color::Black) :
            binding(binding), name(std::move(name)), type(type), multisampled(multisampled), format(format),
            clearColor(clearColor)
        {
        }

        [[nodiscard]] uint32_t GetBinding() const { return binding; }
        [[nodiscard]] const string &GetName() const { return name; }
        [[nodiscard]] Type GetType() const { return type; }
        [[nodiscard]] bool IsMultisampled() const { return multisampled; }
        [[nodiscard]] VkFormat GetFormat() const { return format; }
        [[nodiscard]] const Color &GetClearColor() const { return clearColor; }

    private:
        uint32_t binding;
        string name;
        Type type;
        bool multisampled;
        VkFormat format;
        Color clearColor;
    };

    class RhiSubpassType
    {
    public:
        RhiSubpassType(uint32_t binding, vector<uint32_t> attachmentBindings) :
            binding(binding), attachmentBindings(move(attachmentBindings))
        {
        }

        [[nodiscard]] uint32_t GetBinding() const { return binding; }
        [[nodiscard]] const vector<uint32_t> &GetAttachmentBindings() const { return attachmentBindings; }

    private:
        uint32_t binding;
        vector<uint32_t> attachmentBindings;
    };

    class RhiRenderArea
    {
    public:
        explicit RhiRenderArea(const UVec2 &extent = {}, const IVec2 &offset = {}) : extent(extent), offset(offset) {}

        bool operator==(const RhiRenderArea &rhs) const { return extent == rhs.extent && offset == rhs.offset; }

        bool operator!=(const RhiRenderArea &rhs) const { return !operator==(rhs); }

        [[nodiscard]] const UVec2 &GetExtent() const { return extent; }
        void SetExtent(const UVec2 &extent) { this->extent = extent; }

        [[nodiscard]] const IVec2 &GetOffset() const { return offset; }
        void SetOffset(const IVec2 &offset) { this->offset = offset; }

        /**
         * Gets the aspect ratio between the render stages width and height.
         * @return The aspect ratio.
         */
        [[nodiscard]] float GetAspectRatio() const { return aspectRatio; }
        void SetAspectRatio(float aspectRatio) { this->aspectRatio = aspectRatio; }

    private:
        UVec2 extent;
        IVec2 offset;
        float aspectRatio = 1.0f;
    };

    class RhiViewport
    {
    public:
        RhiViewport() = default;

        explicit RhiViewport(const UVec2 &size) : size(size) {}

        [[nodiscard]] const Vec2 &GetScale() const { return scale; }
        void SetScale(const Vec2 &scale) { this->scale = scale; }

        [[nodiscard]] const optional<UVec2> &GetSize() const { return size; }
        void SetSize(const optional<UVec2> &size) { this->size = size; }

        [[nodiscard]] const IVec2 &GetOffset() const { return offset; }
        void SetOffset(const IVec2 &offset) { this->offset = offset; }

    private:
        Vec2 scale = {1.0f, 1.0f};
        optional<UVec2> size;
        IVec2 offset{};
    };

    class RhiRenderStage
    {
        friend class RenderSystem;

    public:
        explicit RhiRenderStage(vector<RhiAttachment> images = {}, vector<RhiSubpassType> subpasses = {},
                                const RhiViewport &viewport = RhiViewport());

        void Update();
        void Rebuild(const RhiSwapchain &swapchain);

        [[nodiscard]] optional<RhiAttachment> GetAttachment(const string &name) const;
        [[nodiscard]] optional<RhiAttachment> GetAttachment(uint32_t binding) const;
        [[nodiscard]] const Descriptor *GetDescriptor(const string &name) const;
        [[nodiscard]] const VkFramebuffer &GetActiveFramebuffer(uint32_t activeSwapchainImage) const;
        [[nodiscard]] const vector<RhiAttachment> &GetAttachments() const { return attachments; }
        [[nodiscard]] const vector<RhiSubpassType> &GetSubpasses() const { return subpasses; }

        RhiViewport &GetViewport() { return viewport; }
        void SetViewport(const RhiViewport &viewport) { this->viewport = viewport; }

        /**
         * Gets the render stage viewport.
         * @return The the render stage viewport.
         */
        [[nodiscard]] const RhiRenderArea &GetRenderArea() const { return renderArea; }

        /**
         * Gets if the width or height has changed between the last update and now.
         * @return If the width or height has changed.
         */
        [[nodiscard]] bool IsOutOfDate() const { return outOfDate; }

        [[nodiscard]] const RhiRenderpass *GetRenderpass() const { return renderpass.get(); }
        [[nodiscard]] const ImageDepth *GetDepthStencil() const { return depthStencil.get(); }
        [[nodiscard]] const Framebuffer *GetFramebuffer() const { return framebuffer.get(); }
        [[nodiscard]] const vector<VkClearValue> &GetClearValues() const { return clearValues; }
        [[nodiscard]] uint32_t GetAttachmentCount(uint32_t subpass) const { return subpassAttachmentCount[subpass]; }
        [[nodiscard]] bool HasDepth() const { return depthAttachment.has_value(); }
        [[nodiscard]] bool HasSwapchain() const { return swapchainAttachment.has_value(); }
        [[nodiscard]] bool IsMultisampled(uint32_t subpass) const { return subpassMultisampled[subpass]; }

    private:
        vector<RhiAttachment> attachments;
        vector<RhiSubpassType> subpasses;

        RhiViewport viewport;

        unique_ptr<RhiRenderpass> renderpass;
        unique_ptr<ImageDepth> depthStencil;
        unique_ptr<Framebuffer> framebuffer;

        map<string, const Descriptor *> descriptors;

        vector<VkClearValue> clearValues;
        vector<uint32_t> subpassAttachmentCount;
        optional<RhiAttachment> depthAttachment;
        optional<RhiAttachment> swapchainAttachment;
        vector<bool> subpassMultisampled;

        RhiRenderArea renderArea;
        bool outOfDate = false;
    };
} // namespace SF::Engine
