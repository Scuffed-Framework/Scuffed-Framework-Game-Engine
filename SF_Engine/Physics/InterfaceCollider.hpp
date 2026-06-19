#pragma once

#include <btBulletDynamicsCommon.h>
#include <BulletSoftBody/btSoftRigidDynamicsWorld.h>
#include <BulletSoftBody/btSoftBody.h>
#include <memory>

//
//  InterfaceCollider  (world-agnostic abstract root)
//
//  Full hierarchy
//
//  InterfaceCollider
//   ├ InterfaceRigidCollider      (InterfaceCollider.h)
//   │    ├ MeshCollider           (MeshCollider.h)
//   │    └ ConvexCollider         (ConvexCollider.h)
//   └ InterfaceSoftCollider       (InterfaceCollider.h)
//        ├ SoftMeshCollider       (SoftMeshCollider.h)
//        └ SoftRopeCollider       (SoftRopeCollider.h)
//
//  The world is always btSoftRigidDynamicsWorld* so both branches share one
//  world instance.  Rigid bodies use the base btDynamicsWorld interface;
//  soft bodies use the extended soft-body API.
//
class InterfaceCollider
{
public:
    explicit InterfaceCollider(float mass = 0.0f) : m_mass(mass) {}
    virtual ~InterfaceCollider() = default;

    InterfaceCollider(const InterfaceCollider &) = delete;
    InterfaceCollider &operator=(const InterfaceCollider &) = delete;

    //  Pure-virtual interface

    [[nodiscard]] virtual const char *getType() const = 0;

    virtual void initialise(btSoftRigidDynamicsWorld *world,
                            const btTransform &startTransform = btTransform::getIdentity()) = 0;

    [[nodiscard]] float getMass() const { return m_mass; }

protected:
    float m_mass{0.0f};
    btSoftRigidDynamicsWorld *m_world{nullptr};
};

//
//  InterfaceRigidCollider
//
//  Base for btRigidBody-backed colliders.
//  Subclasses only need to implement buildShape() + getType().
//
class InterfaceRigidCollider : public InterfaceCollider
{
public:
    explicit InterfaceRigidCollider(float mass = 0.0f)
        : InterfaceCollider(mass) {}

    ~InterfaceRigidCollider() override
    {
        if (m_rigidBody && m_world)
            m_world->removeRigidBody(m_rigidBody.get());
    }

    //  Subclass must implement
    [[nodiscard]] virtual std::unique_ptr<btCollisionShape> buildShape() = 0;

    //  InterfaceCollider
    void initialise(btSoftRigidDynamicsWorld *world,
                    const btTransform &startTransform = btTransform::getIdentity()) override
    {
        m_world = world;
        m_shape = buildShape();

        btVector3 inertia(0, 0, 0);
        if (m_mass != 0.0f)
            m_shape->calculateLocalInertia(m_mass, inertia);

        m_motionState = std::make_unique<btDefaultMotionState>(startTransform);
        btRigidBody::btRigidBodyConstructionInfo ci(
            m_mass, m_motionState.get(), m_shape.get(), inertia);
        m_rigidBody = std::make_unique<btRigidBody>(ci);

        world->addRigidBody(m_rigidBody.get());
    }

    //  Accessors
    [[nodiscard]] btRigidBody *getRigidBody() { return m_rigidBody.get(); }
    [[nodiscard]] const btRigidBody *getRigidBody() const { return m_rigidBody.get(); }
    [[nodiscard]] btCollisionShape *getShape() { return m_shape.get(); }

    [[nodiscard]] btTransform getWorldTransform() const
    {
        btTransform t;
        t.setIdentity();
        if (m_rigidBody)
            m_rigidBody->getMotionState()->getWorldTransform(t);
        return t;
    }

    void setWorldTransform(const btTransform &t)
    {
        if (!m_rigidBody)
            return;
        m_rigidBody->getMotionState()->setWorldTransform(t);
        m_rigidBody->setWorldTransform(t);
        m_rigidBody->activate(true);
    }

    void applyImpulse(const btVector3 &impulse)
    {
        if (m_rigidBody)
        {
            m_rigidBody->activate(true);
            m_rigidBody->applyCentralImpulse(impulse);
        }
    }

    void setRestitution(float r)
    {
        if (m_rigidBody)
            m_rigidBody->setRestitution(r);
    }
    void setFriction(float f)
    {
        if (m_rigidBody)
            m_rigidBody->setFriction(f);
    }
    void setDamping(float lin, float ang)
    {
        if (m_rigidBody)
            m_rigidBody->setDamping(lin, ang);
    }

protected:
    std::unique_ptr<btCollisionShape> m_shape;
    std::unique_ptr<btDefaultMotionState> m_motionState;
    std::unique_ptr<btRigidBody> m_rigidBody;
};

//
//  InterfaceSoftCollider
//
//  Base for btSoftBody-backed colliders.
//  Subclasses implement buildSoftBody() to create and configure the body;
//  the world takes ownership of the raw pointer.
//
class InterfaceSoftCollider : public InterfaceCollider
{
public:
    explicit InterfaceSoftCollider(float mass = 1.0f)
        : InterfaceCollider(mass) {}

    ~InterfaceSoftCollider() override
    {
        if (m_softBody && m_world)
            m_world->removeSoftBody(m_softBody);
        // world owns the pointee, do NOT delete here
    }

    /// Return a newly constructed btSoftBody using worldInfo.
    /// The pointer is given to the world, do NOT wrap it in unique_ptr.
    [[nodiscard]] virtual btSoftBody *buildSoftBody(
        btSoftBodyWorldInfo &worldInfo,
        const btTransform &startTransform) = 0;

    //  InterfaceCollider
    void initialise(btSoftRigidDynamicsWorld *world,
                    const btTransform &startTransform = btTransform::getIdentity()) override
    {
        m_world = world;
        m_softBody = buildSoftBody(world->getWorldInfo(), startTransform);

        m_softBody->setTotalMass(m_mass, /*fromFaces=*/false);
        applyDefaultConfig(m_softBody->m_cfg);

        world->addSoftBody(m_softBody);
    }

    //  Accessors
    [[nodiscard]] btSoftBody *getSoftBody() { return m_softBody; }
    [[nodiscard]] const btSoftBody *getSoftBody() const { return m_softBody; }

    //  Soft-body parameter helpers

    /// Linear / angular / volume stiffness  [0 = floppy … 1 = rigid]
    void setStiffness(float kLST, float kAST = 1.0f, float kVST = 0.5f)
    {
        if (!m_softBody)
            return;
        for (int i = 0; i < m_softBody->m_materials.size(); ++i)
        {
            m_softBody->m_materials[i]->m_kLST = kLST;
            m_softBody->m_materials[i]->m_kAST = kAST;
            m_softBody->m_materials[i]->m_kVST = kVST;
        }
    }

    /// Common config knobs (all clamped to [0,1] by Bullet internally)
    /// kDP = damping  kDG = drag  kPR = pressure  kDF = friction
    void setConfig(float kDP = 0.01f, float kDG = 0.0f,
                   float kPR = 0.00f, float kDF = 0.2f)
    {
        if (!m_softBody)
            return;
        m_softBody->m_cfg.kDP = kDP;
        m_softBody->m_cfg.kDG = kDG;
        m_softBody->m_cfg.kPR = kPR;
        m_softBody->m_cfg.kDF = kDF;
    }

    /// Add a uniform force to every node.
    void applyForce(const btVector3 &force)
    {
        if (m_softBody)
            m_softBody->addForce(force);
    }

    /// Pin node by index (zero inverse mass = immovable anchor).
    void pinNode(int nodeIndex)
    {
        if (m_softBody && nodeIndex < m_softBody->m_nodes.size())
            m_softBody->m_nodes[nodeIndex].m_im = 0.0f;
    }

protected:
    btSoftBody *m_softBody{nullptr}; // owned by the world

private:
    static void applyDefaultConfig(btSoftBody::Config &cfg)
    {
        cfg.kDP = 0.01f;
        cfg.kDG = 0.00f;
        cfg.kPR = 0.00f;
        cfg.kDF = 0.20f;
        cfg.piterations = 10;
        cfg.viterations = 0;
        cfg.diterations = 0;
    }
};