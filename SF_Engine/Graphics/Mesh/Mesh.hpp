#pragma once

#include <Graphics/Buffers/Buffer.hpp>
#include <Graphics/Commands/CommandBuffer.hpp>
#include <Graphics/Mesh/Vertex.hpp>
#include <span>
#include <vector>

namespace SF::Engine
{
    /**
     * @brief GPU mesh : owns vertex and index buffers, knows how to bind and draw itself.
     */
    class Mesh
    {
    public:
        /**
         * Upload mesh data to the GPU.
         * @param vertices  Vertex data.
         * @param indices   Index data (uint32). Empty = non-indexed draw.
         */
        Mesh(std::span<const Vertex> vertices,
             std::span<const uint32_t> indices = {});

        ~Mesh() = default;

        Mesh(const Mesh &) = delete;
        Mesh &operator=(const Mesh &) = delete;
        Mesh(Mesh &&) = default;
        Mesh &operator=(Mesh &&) = default;

        /**
         * Bind vertex (and optionally index) buffers and issue the draw call.
         * Call this inside an active render pass.
         */
        void Draw(const CommandBuffer &commandBuffer, uint32_t instanceCount = 1) const;

        uint32_t GetVertexCount() const { return vertexCount_; }
        uint32_t GetIndexCount() const { return indexCount_; }
        bool IsIndexed() const { return indexCount_ > 0; }

    private:
        std::unique_ptr<Buffer> vertexBuffer_;
        std::unique_ptr<Buffer> indexBuffer_;
        uint32_t vertexCount_ = 0;
        uint32_t indexCount_ = 0;
    };
}
