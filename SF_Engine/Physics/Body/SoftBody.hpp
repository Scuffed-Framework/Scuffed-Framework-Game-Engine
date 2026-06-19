#pragma once

#include "InterfaceCollider.h"
#include <BulletSoftBody/btSoftBodyHelpers.h>
#include <BulletSoftBody/btSoftBody.h>
#include <span>
#include <vector>
#include <stdexcept>
#include <cmath>

class SoftMeshCollider : public InterfaceSoftCollider
{
private:
    std::vector<btVector3> m_vertices;
    std::vector<int> m_indices;

    static void applyTransformToNodes(btSoftBody *body, const btTransform &xf)
    {
        if (xf == btTransform::getIdentity())
            return;
        for (int i = 0; i < body->m_nodes.size(); ++i)
        {
            body->m_nodes[i].m_x = xf * body->m_nodes[i].m_x;
            body->m_nodes[i].m_q = body->m_nodes[i].m_x;
            body->m_nodes[i].m_n = xf.getBasis() * body->m_nodes[i].m_n;
        }
    }

public:
    SoftMeshCollider(std::span<const btVector3> vertices,
                     std::span<const int> indices,
                     float mass = 1.0f)
        : InterfaceSoftCollider(mass), m_vertices(vertices.begin(), vertices.end()), m_indices(indices.begin(), indices.end())
    {
        if (m_indices.size() % 3 != 0)
            throw std::invalid_argument("SoftMeshCollider: index count must be multiple of 3.");
    }

    [[nodiscard]] btSoftBody *buildSoftBody(btSoftBodyWorldInfo &info,
                                            const btTransform &xf) override
    {
        std::vector<btScalar> coords;
        coords.reserve(m_vertices.size() * 3);
        for (const auto &v : m_vertices)
        {
            coords.push_back(v.x());
            coords.push_back(v.y());
            coords.push_back(v.z());
        }

        btSoftBody *body = btSoftBodyHelpers::CreateFromTriMesh(
            info,
            coords.data(),
            m_indices.data(),
            static_cast<int>(m_indices.size() / 3),
            /*randomizeConstraints=*/true);

        applyTransformToNodes(body, xf);

        body->generateBendingConstraints(2, body->m_materials[0]);

        return body;
    }

    [[nodiscard]] const char *getType() const override { return "SoftBody/Mesh"; }

    static std::unique_ptr<SoftMeshCollider>
    makeCloth(const btVector3 &topLeft, const btVector3 &topRight,
              const btVector3 &bottomLeft, const btVector3 &bottomRight,
              int resX, int resY, float mass = 1.0f)
    {
        if (resX < 2 || resY < 2)
            throw std::invalid_argument("makeCloth: resolution must be >= 2 in each axis.");

        std::vector<btVector3> verts;
        std::vector<int> idx;
        verts.reserve(static_cast<size_t>(resX * resY));

        for (int iy = 0; iy < resY; ++iy)
        {
            const float ty = static_cast<float>(iy) / static_cast<float>(resY - 1);
            const btVector3 left = topLeft.lerp(bottomLeft, ty);
            const btVector3 right = topRight.lerp(bottomRight, ty);

            for (int ix = 0; ix < resX; ++ix)
            {
                const float tx = static_cast<float>(ix) / static_cast<float>(resX - 1);
                verts.push_back(left.lerp(right, tx));
            }
        }

        // Two triangles per quad cell
        for (int iy = 0; iy < resY - 1; ++iy)
        {
            for (int ix = 0; ix < resX - 1; ++ix)
            {
                const int a = iy * resX + ix;
                const int b = a + 1;
                const int c = a + resX;
                const int d = c + 1;
                idx.insert(idx.end(), {a, b, c, b, d, c});
            }
        }

        return std::make_unique<SoftMeshCollider>(verts, idx, mass);
    }
};

class SoftRopeCollider : public InterfaceSoftCollider
{
public:
    /// @param from       World position of the first (start) node.
    /// @param to         World position of the last (end) node.
    /// @param nodeCount  Resolution of the rope (>= 2).
    /// @param mass       Total mass distributed across all nodes.
    SoftRopeCollider(const btVector3 &from,
                     const btVector3 &to,
                     int nodeCount = 16,
                     float mass = 1.0f)
        : InterfaceSoftCollider(mass), m_from(from), m_to(to), m_nodeCount(nodeCount)
    {
        if (nodeCount < 2)
            throw std::invalid_argument("SoftRopeCollider: nodeCount must be >= 2.");
    }

    [[nodiscard]] btSoftBody *buildSoftBody(btSoftBodyWorldInfo &info,
                                            const btTransform & /*xf*/) override
    {
        // btSoftBodyHelpers::CreateRope places nodes from 'from' to 'to'
        // with (nodeCount-1) segments.  The last two args are fixedness flags:
        // 0 = free, 1 = fixed at start, 2 = fixed at end, 3 = both fixed.
        btSoftBody *body = btSoftBodyHelpers::CreateRope(
            info, m_from, m_to,
            m_nodeCount - 1,
            /*fixeds=*/0);

        // Set up a single material with reasonable defaults
        body->m_materials[0]->m_kLST = 0.8f; // linear stiffness

        return body;
    }

    [[nodiscard]] const char *getType() const override { return "SoftBody/Rope"; }

    /// Attach this rope's first node to a rigid body at a given offset.
    void attachStartToRigidBody(btRigidBody *rb,
                                const btVector3 &localPivot = btVector3(0, 0, 0))
    {
        if (!m_softBody)
            return;
        m_softBody->appendAnchor(0, rb, localPivot);
    }

    /// Attach this rope's last node to a rigid body.
    void attachEndToRigidBody(btRigidBody *rb,
                              const btVector3 &localPivot = btVector3(0, 0, 0))
    {
        if (!m_softBody)
            return;
        const int last = m_softBody->m_nodes.size() - 1;
        m_softBody->appendAnchor(last, rb, localPivot);
    }

    [[nodiscard]] int getNodeCount() const { return m_nodeCount; }

private:
    btVector3 m_from;
    btVector3 m_to;
    int m_nodeCount;
};