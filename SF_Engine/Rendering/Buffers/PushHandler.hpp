#pragma once

#include <algorithm>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include "Buffer.hpp"
#include "Rendering/Pipelines/Pipeline.hpp"

namespace SF::Engine
{
    /**
     * @brief Class that handles a pipeline push constant.
     */
    class PushHandler
    {
    public:
        explicit PushHandler(bool multipipeline = false);
        explicit PushHandler(const Shader::UniformBlock& uniformBlock, bool multipipeline = false);

        template <TriviallyCopiable T>
        void Push(const T& object, std::size_t offset, std::size_t size)
        {
            auto src = std::as_bytes(std::span{&object, 1});
            std::ranges::copy_n(src.begin(), size, data_.get() + offset);
        }

        template <TriviallyCopiable T>
        void Push(std::string_view uniformName, const T& object, std::size_t size = 0)
        {
            if (!uniformBlock_) [[unlikely]]
                return;

            auto uniform = uniformBlock_->GetUniform(std::string(uniformName));
            if (!uniform) [[unlikely]]
                return;

            auto realSize = size;
            if (realSize == 0)
            {
                realSize =
                    std::min(sizeof(object),
                             static_cast<std::size_t>(uniform->size));  // Changed from GetSize()
            }

            Push(object, static_cast<std::size_t>(uniform->offset),
                 realSize);  // Changed from GetOffset()
        }

        [[nodiscard]] bool Update(const std::optional<Shader::UniformBlock>& uniformBlock);

        void BindPush(const CommandBuffer& commandBuffer, const Pipeline& pipeline);

    private:
        bool multipipeline_;
        std::optional<Shader::UniformBlock> uniformBlock_;
        std::unique_ptr<std::byte[]> data_;
    };
}