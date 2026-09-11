#pragma once

#include <algorithm>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include "StorageBuffer.hpp"

namespace SF::Engine
{
    /**
     * @brief Class that handles a storage buffer.
     */
    class StorageHandler
    {
    public:
        explicit StorageHandler(bool multipipeline = false);
        explicit StorageHandler(const Shader::UniformBlock& uniformBlock,
                                bool multipipeline = false);

        void Push(std::span<const std::byte> data)
        {
            if (size_ != data.size())
            {
                size_ = static_cast<uint32_t>(data.size());
                handlerStatus_ = Buffer::Status::Reset;
                return;
            }

            if (!uniformBlock_ || !storageBuffer_) [[unlikely]]
                return;

            if (!bound_)
            {
                storageBuffer_->MapMemory(&data_);
                bound_ = true;
            }

            auto* dest = static_cast<std::byte*>(data_);

            // If already changed, skip comparison and just copy
            if (handlerStatus_ == Buffer::Status::Changed ||
                !std::ranges::equal(std::span{dest, data.size()}, data))
            {
                std::ranges::copy(data, dest);
                handlerStatus_ = Buffer::Status::Changed;
            }
        }

        template <TriviallyCopiable T>
        void Push(const T& object, std::size_t offset, std::size_t size)
        {
            if (!uniformBlock_ || !storageBuffer_) [[unlikely]]
                return;

            if (!bound_)
            {
                storageBuffer_->MapMemory(&data_);
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
            if (!uniformBlock_) [[unlikely]]
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

        [[nodiscard]] const StorageBuffer* GetStorageBuffer() const noexcept
        {
            return storageBuffer_.get();
        }

    private:
        bool multipipeline_;
        std::optional<Shader::UniformBlock> uniformBlock_;
        uint32_t size_ = 0;
        void* data_ = nullptr;
        bool bound_ = false;
        std::unique_ptr<StorageBuffer> storageBuffer_;
        Buffer::Status handlerStatus_;
    };
}