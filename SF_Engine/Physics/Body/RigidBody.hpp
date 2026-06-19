#pragma once

#include "InterfaceCollider.h"
#include <BulletCollision/CollisionShapes/btBoxShape.h>
#include <BulletCollision/CollisionShapes/btSphereShape.h>
#include <BulletCollision/CollisionShapes/btCapsuleShape.h>
#include <BulletCollision/CollisionShapes/btCylinderShape.h>
#include <BulletCollision/CollisionShapes/btConeShape.h>
#include <BulletCollision/CollisionShapes/btConvexHullShape.h>
#include <BulletCollision/CollisionShapes/btBvhTriangleMeshShape.h>
#include <BulletCollision/CollisionShapes/btTriangleMesh.h>
#include <span>
#include <variant>
#include <stdexcept>

#include <btBulletDynamicsCommon.h>
#include <UtilityClasses/NoCopy.hpp>
#include <memory>

namespace SF::Engine
{
    class RigidBodyCollider : public InterfaceRigidCollider
    {
    public:
        struct BoxDesc
        {
            btVector3 halfExtents;
        };
        struct SphereDesc
        {
            float radius;
        };
        struct CapsuleDesc
        {
            float radius;
            float height;
        };
        struct CylinderDesc
        {
            btVector3 halfExtents;
        };
        struct ConeDesc
        {
            float radius;
            float height;
        };
        struct ConvexHullDesc
        {
            std::vector<btVector3> points;
        };
        struct TriMeshDesc
        {
            std::vector<btVector3> vertices;
            std::vector<int> indices;
        };

        using ShapeDesc = std::variant<
            BoxDesc, SphereDesc, CapsuleDesc,
            CylinderDesc, ConeDesc,
            ConvexHullDesc, TriMeshDesc>;

        //  Constructor

        RigidBodyCollider(ShapeDesc desc, float mass = 0.0f)
            : InterfaceRigidCollider(mass), m_desc(std::move(desc))
        {
            // Validate: BVH triangle mesh must be static
            if (std::holds_alternative<TriMeshDesc>(m_desc) && mass != 0.0f)
                throw std::invalid_argument(
                    "RigidBodyCollider(TriMesh): BVH mesh must have mass = 0. "
                    "For dynamic concave meshes use btGImpactMeshShape.");
        }

        //  InterfaceRigidCollider

        [[nodiscard]] std::unique_ptr<btCollisionShape> buildShape() override
        {
            return std::visit([this](auto &&d) -> std::unique_ptr<btCollisionShape>
                              {
            using T = std::decay_t<decltype(d)>;

            if constexpr (std::is_same_v<T, BoxDesc>)
                return std::make_unique<btBoxShape>(d.halfExtents);

            else if constexpr (std::is_same_v<T, SphereDesc>)
                return std::make_unique<btSphereShape>(d.radius);

            else if constexpr (std::is_same_v<T, CapsuleDesc>)
                return std::make_unique<btCapsuleShape>(d.radius, d.height);

            else if constexpr (std::is_same_v<T, CylinderDesc>)
                return std::make_unique<btCylinderShape>(d.halfExtents);

            else if constexpr (std::is_same_v<T, ConeDesc>)
                return std::make_unique<btConeShape>(d.radius, d.height);

            else if constexpr (std::is_same_v<T, ConvexHullDesc>)
            {
                auto hull = std::make_unique<btConvexHullShape>();
                for (const auto& p : d.points)
                    hull->addPoint(p, false);
                hull->recalcLocalAabb();
                hull->optimizeConvexHull();
                return hull;
            }
            else if constexpr (std::is_same_v<T, TriMeshDesc>)
            {
                if (d.indices.size() % 3 != 0)
                    throw std::invalid_argument("TriMeshDesc: index count must be multiple of 3.");

                m_triMesh = std::make_unique<btTriangleMesh>();
                for (size_t i = 0; i + 2 < d.indices.size(); i += 3)
                    m_triMesh->addTriangle(
                        d.vertices[d.indices[i    ]],
                        d.vertices[d.indices[i + 1]],
                        d.vertices[d.indices[i + 2]]);

                return std::make_unique<btBvhTriangleMeshShape>(
                    m_triMesh.get(), /*quantizedAabb=*/true);
            } }, m_desc);
        }

        [[nodiscard]] const char *getType() const override
        {
            return std::visit([](auto &&d) -> const char *
                              {
            using T = std::decay_t<decltype(d)>;
            if      constexpr (std::is_same_v<T, BoxDesc>)        return "RigidBody/Box";
            else if constexpr (std::is_same_v<T, SphereDesc>)     return "RigidBody/Sphere";
            else if constexpr (std::is_same_v<T, CapsuleDesc>)    return "RigidBody/Capsule";
            else if constexpr (std::is_same_v<T, CylinderDesc>)   return "RigidBody/Cylinder";
            else if constexpr (std::is_same_v<T, ConeDesc>)       return "RigidBody/Cone";
            else if constexpr (std::is_same_v<T, ConvexHullDesc>) return "RigidBody/ConvexHull";
            else                                                    return "RigidBody/TriMesh"; }, m_desc);
        }

        //  Convenience static factories

        static std::unique_ptr<RigidBodyCollider> makeBox(const btVector3 &half, float mass = 1.f)
        {
            return std::make_unique<RigidBodyCollider>(BoxDesc{half}, mass);
        }

        static std::unique_ptr<RigidBodyCollider> makeSphere(float r, float mass = 1.f)
        {
            return std::make_unique<RigidBodyCollider>(SphereDesc{r}, mass);
        }

        static std::unique_ptr<RigidBodyCollider> makeCapsule(float r, float h, float mass = 1.f)
        {
            return std::make_unique<RigidBodyCollider>(CapsuleDesc{r, h}, mass);
        }

        static std::unique_ptr<RigidBodyCollider> makeCylinder(const btVector3 &half, float mass = 1.f)
        {
            return std::make_unique<RigidBodyCollider>(CylinderDesc{half}, mass);
        }

        static std::unique_ptr<RigidBodyCollider> makeCone(float r, float h, float mass = 1.f)
        {
            return std::make_unique<RigidBodyCollider>(ConeDesc{r, h}, mass);
        }

    private:
        ShapeDesc m_desc;
        std::unique_ptr<btTriangleMesh> m_triMesh; // kept alive alongside BVH shape
    };
}