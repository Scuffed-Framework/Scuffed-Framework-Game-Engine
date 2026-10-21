#include "Mesh.hpp"

#include <stdexcept>

namespace SF::Engine
{
    Mesh::Mesh(std::span<const Vertex> vertices, std::span<const uint32_t> indices)
        : vertexCount_(static_cast<uint32_t>(vertices.size())),
          indexCount_ (static_cast<uint32_t>(indices.size()))
    {
        if (vertices.empty())
            throw std::runtime_error("Mesh: vertex data is empty");

        ID = UUID::Generate();
        
        vertexBuffer_ = std::make_unique<Buffer>(
            sizeof(Vertex) * vertices.size(),
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VMA_MEMORY_USAGE_AUTO,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            std::as_bytes(vertices));

        if (!indices.empty())
        {
            indexBuffer_ = std::make_unique<Buffer>(
                sizeof(uint32_t) * indices.size(),
                VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                VMA_MEMORY_USAGE_AUTO,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                std::as_bytes(indices));
        }
    }

    void Mesh::Draw(const CommandBuffer& commandBuffer, uint32_t instanceCount) const
    {
        VkBuffer     vb  = vertexBuffer_->GetBuffer();
        VkDeviceSize off = 0;
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vb, &off);

        if (IsIndexed())
        {
            vkCmdBindIndexBuffer(commandBuffer, indexBuffer_->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(commandBuffer, indexCount_, instanceCount, 0, 0, 0);
        }
        else
        {
            vkCmdDraw(commandBuffer, vertexCount_, instanceCount, 0, 0);
        }
    }
}
