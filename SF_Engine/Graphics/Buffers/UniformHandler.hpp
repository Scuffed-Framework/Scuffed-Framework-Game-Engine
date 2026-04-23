#pragma once

#include <algorithm>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include "UniformBuffer.hpp"

namespace SF::Engine
{
    /**
     * @brief class that handles a uniform buffer.
     */
    class UniformHandler
    {
    public:
        explicit UniformHandler(bool multipipeline = false);
        explicit UniformHandler(const Shader::UniformBlock& uniformBlock,
                                bool multipipeline = false);

        template <TriviallyCopiable T>
        void Push(const T& object, std::size_t offset, std::size_t size)
        {
            if (!uniformBlock_ || !uniformBuffer_) [[unlikely]]
                return;

            if (!bound_)
            {
                uniformBuffer_->MapMemory(&data_);
                bound_ = true;
            }

            auto* dest = static_cast<std::byte*>(data_) + offset;
            auto src = std::as_bytes(std::span{&object, 1});
            auto copySize = std::min(size, src.size());

            // If already changed, skip comparison
            if (handlerStatus_ == Buffer::Status::Changed ||
                !std::ranges::equal(std::span{dest, copySize}, src.first(copySize)))
            {
                std::ranges::copy_n(src.begin(), copySize, dest);
                handlerStatus_ = Buffer::Status::Changed;
            }
        }

        template <TriviallyCopiable T>
        void Push(std::string_view uniformName, const T& object, std::size_t size = 0)
        {
            if (!uniformBlock_ || !uniformBuffer_) [[unlikely]]
                return;

            auto uniform = uniformBlock_->GetUniform(std::string(uniformName));
            if (!uniform) [[unlikely]]
                return;

            auto realSize = size;
            if (realSize == 0)
            {
                realSize = std::min(sizeof(object), static_cast<std::size_t>(uniform->size));
            }

            Push(object, static_cast<std::size_t>(uniform->offset), realSize);
        }

        [[nodiscard]] bool Update(const std::optional<Shader::UniformBlock>& uniformBlock);

        [[nodiscard]] const UniformBuffer* GetUniformBuffer() const noexcept
        {
            return uniformBuffer_.get();
        }

    private:
        bool multipipeline_;
        std::optional<Shader::UniformBlock> uniformBlock_;
        uint32_t size_ = 0;
        void* data_ = nullptr;
        bool bound_ = false;
        std::unique_ptr<UniformBuffer> uniformBuffer_;
        Buffer::Status handlerStatus_;
    };
}
