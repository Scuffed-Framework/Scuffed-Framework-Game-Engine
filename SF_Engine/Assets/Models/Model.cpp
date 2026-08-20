#include "Model.hpp"
#include <Engine/Log/Log.hpp>
#include <Scene/SceneManager.hpp>

namespace SF::Engine
{
    bool Model::CmdRender(const CommandBuffer &commandBuffer, uint32_t instances) const
    {
        if (vertexBuffer && indexBuffer)
        {
            VkBuffer vertexBuffers[1] = {vertexBuffer->GetBuffer()};
            VkDeviceSize offsets[1] = {0};
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
            vkCmdBindIndexBuffer(commandBuffer, indexBuffer->GetBuffer(), 0, GetIndexType());
            vkCmdDrawIndexed(commandBuffer, indexCount, instances, 0, 0, 0);
        }
        else if (vertexBuffer && !indexBuffer)
        {
            VkBuffer vertexBuffers[1] = {vertexBuffer->GetBuffer()};
            VkDeviceSize offsets[1] = {0};
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
            vkCmdDraw(commandBuffer, vertexCount, instances, 0, 0);
        }
        else
        {
            Log::Error("Model with no buffers can't be rendered");
            return false;
        }

        return true;
    }

    std::vector<Vertex> Model::GetVertices(std::size_t offset) const
    {
        if (!vertexBuffer || vertexCount == 0)
            return {};

        // vertexBuffer is device-local (see SetVertices), so it can't be
        // mapped directly — copy it into a host-visible staging buffer first.
        Buffer stagingBuffer(vertexBuffer->GetSize(), VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_AUTO,
                             VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);

        CommandBuffer commandBuffer(true);

        VkBufferCopy copyRegion{};
        copyRegion.size = vertexBuffer->GetSize();
        vkCmdCopyBuffer(commandBuffer, vertexBuffer->GetBuffer(), stagingBuffer.GetBuffer(), 1, &copyRegion);

        commandBuffer.SubmitIdle();

        void *mapped = nullptr;
        stagingBuffer.MapMemory(&mapped);

        std::vector<Vertex> vertices(vertexCount);
        std::memcpy(vertices.data(), static_cast<const std::byte *>(mapped) + offset, vertexCount * sizeof(Vertex));

        stagingBuffer.UnmapMemory();

        return vertices;
    }

    void Model::SetVertices(std::vector<Vertex> &vertices)
    {
        vertexBuffer = nullptr;
        vertexCount = static_cast<uint32_t>(vertices.size());

        if (vertices.empty())
            return;

        std::span<const std::byte> data(reinterpret_cast<const std::byte *>(vertices.data()),
                                        vertices.size() * sizeof(Vertex));

        vertexBuffer = std::make_unique<Buffer>(
            VkDeviceSize(sizeof(Vertex) * vertices.size()),
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VMA_MEMORY_USAGE_AUTO, 0U, data);
    }

    std::vector<uint32_t> Model::GetIndices(std::size_t offset) const
    {
        if (!indexBuffer || indexCount == 0)
            return {};

        Buffer stagingBuffer(indexBuffer->GetSize(), VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_AUTO,
                             VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);

        CommandBuffer commandBuffer(true);

        VkBufferCopy copyRegion{};
        copyRegion.size = indexBuffer->GetSize();
        vkCmdCopyBuffer(commandBuffer, indexBuffer->GetBuffer(), stagingBuffer.GetBuffer(), 1, &copyRegion);

        commandBuffer.SubmitIdle();

        void *mapped = nullptr;
        stagingBuffer.MapMemory(&mapped);

        std::vector<uint32_t> indices(indexCount);
        std::memcpy(indices.data(), static_cast<const std::byte *>(mapped) + offset, indexCount * sizeof(uint32_t));

        stagingBuffer.UnmapMemory();

        return indices;
    }

    void Model::SetIndices(std::vector<uint32_t> &indices)
    {
        indexBuffer = nullptr;
        indexCount = static_cast<uint32_t>(indices.size());

        if (indices.empty())
            return;

        std::span<const std::byte> data(reinterpret_cast<const std::byte *>(indices.data()),
                                        indices.size() * sizeof(uint32_t));

        indexBuffer = std::make_unique<Buffer>(
            VkDeviceSize(sizeof(uint32_t) * indices.size()),
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VMA_MEMORY_USAGE_AUTO, 0U, data);
    }

    void Model::Initialize(std::vector<Vertex> &vertices, std::vector<uint32_t> &indices)
    {
        SetVertices(vertices);
        SetIndices(indices);

        // tmp?
        minExtents = Vec3(10000, 10000, 10000);
        maxExtents = -Vec3(10000, 10000, 10000);

        for (const auto &vertex : vertices)
        {
            Vec3 position(vertex.position);
            minExtents = min(minExtents, position);
            maxExtents = max(maxExtents, position);
        }

        radius = std::max(minExtents.length(), maxExtents.length());
    }

    std::vector<float> Model::GetPointCloud() const
    {
        if (!vertexBuffer)
            return {};

        auto indices = GetIndices();
        auto vertices = GetVertices();

        std::vector<float> pointCloud;
        pointCloud.reserve(indices.size() * 3);

        for (const auto &index : indices)
        {
            const auto &vertex = vertices[index];
            pointCloud.emplace_back(vertex.position.x);
            pointCloud.emplace_back(vertex.position.y);
            pointCloud.emplace_back(vertex.position.z);
        }

        return pointCloud;
    }
}