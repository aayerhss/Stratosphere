#include "Engine/Application.h"
#include "Engine/TrianglesRenderPassModule.h"
#include "Engine/MeshRenderPassModule.h"
#include "assets/AssetManager.h"
#include "assets/MeshAsset.h"
#include "assets/MeshFormats.h"
#include "Engine/VulkanContext.h"

#include "ECS/Prefab.h"
#include "ECS/PrefabSpawner.h"
#include "ECS/ECSContext.h"

#include "utils/BufferUtils.h" // CreateOrUpdateVertexBuffer + DestroyVertexBuffer
#include "systems/MovementSystem.h"
#include <iostream>
#include <memory>
#include <random>
#include <vector>

class MySampleApp : public Engine::Application
{
public:
    MySampleApp() : Engine::Application()
    { // Create the AssetManager (uses Vulkan device & physical device)

        m_assets = std::make_unique<Engine::AssetManager>(
            GetVulkanContext().GetDevice(),
            GetVulkanContext().GetPhysicalDevice(),
            GetVulkanContext().GetGraphicsQueue(),
            GetVulkanContext().GetGraphicsQueueFamilyIndex());

        // Render tanks/turrets as instanced triangles driven by ECS
        setupTriangleRenderer();
        setupECSFromPrefabs();
    }

    ~MySampleApp() {}

    void Close() override
    {
        vkDeviceWaitIdle(GetVulkanContext().GetDevice());

        // Release mesh handle and collect unused assets
        m_assets->release(m_bugattiHandle);
        m_assets->garbageCollect();

        // Destroy triangle vertex buffer
        Engine::DestroyVertexBuffer(GetVulkanContext().GetDevice(), m_triangleVB);

        // Destroy triangle instance buffer
        Engine::DestroyVertexBuffer(GetVulkanContext().GetDevice(), m_triangleInstancesVB);

        // Release passes
        m_meshPass.reset();
        m_trianglesPass.reset();

        Engine::Application::Close();
    }

    void OnUpdate(Engine::TimeStep ts) override
    {
        updateECS(ts);
    }

    void OnRender() override
    {
        // Rendering handled by Renderer/Engine; no manual draw calls here
    }

private:
    void setupTriangleRenderer()
    {
        // Interleaved vertex data: vec2 position, vec3 color (matches your triangle pipeline)
        const float vertices[] = {
            // x,    y,    r, g, b
            0.0f,
            -0.1f,
            1.0f,
            1.0f,
            1.0f,
            0.1f,
            0.1f,
            1.0f,
            1.0f,
            1.0f,
            -0.1f,
            0.1f,
            1.0f,
            1.0f,
            1.0f,
        };

        VkDevice device = GetVulkanContext().GetDevice();
        VkPhysicalDevice phys = GetVulkanContext().GetPhysicalDevice();

        // Create/upload triangle vertex buffer
        VkDeviceSize dataSize = sizeof(vertices);
        VkResult r = Engine::CreateOrUpdateVertexBuffer(device, phys, vertices, dataSize, m_triangleVB);
        if (r != VK_SUCCESS)
        {
            std::cerr << "Failed to create triangle vertex buffer" << std::endl;
            return;
        }

        // Create triangles pass and bind vertex buffer
        m_trianglesPass = std::make_shared<Engine::TrianglesRenderPassModule>();
        m_triangleBinding.vertexBuffer = m_triangleVB.buffer;
        m_triangleBinding.offset = 0;
        m_triangleBinding.vertexCount = 3; // base triangle (instanced)
        m_trianglesPass->setVertexBinding(m_triangleBinding);

        // Create a placeholder instance buffer; we'll stream real ECS instances every frame.
        const float oneInstance[5] = {0.0f, 0.0f, 1.0f, 1.0f, 1.0f};
        VkDeviceSize instSize = sizeof(oneInstance);
        r = Engine::CreateOrUpdateVertexBuffer(device, phys, oneInstance, instSize, m_triangleInstancesVB);
        if (r != VK_SUCCESS)
            std::cerr << "Failed to create triangle instance buffer" << std::endl;

        Engine::TrianglesRenderPassModule::InstanceBinding inst{};
        inst.instanceBuffer = m_triangleInstancesVB.buffer;
        inst.offset = 0;
        inst.instanceCount = 1;
        m_trianglesPass->setInstanceBinding(inst);

        // Register pass to renderer
        GetRenderer().registerPass(m_trianglesPass);

        // Initial offset (push constants) if supported by your module
        m_trianglesPass->setOffset(0.0f, 0.0f);
    }

    void setupECSFromPrefabs()
    {
        auto &ecs = GetECS();

        // Load prefab definitions from JSON copied next to executable.
        // (CMake copies Sample/entities/*.json -> <build>/Sample/entities/)
        const std::string tankJson = Engine::ECS::readFileText("entities/Tank.json");
        const std::string turretJson = Engine::ECS::readFileText("entities/Turret.json");
        if (tankJson.empty() || turretJson.empty())
        {
            std::cerr << "Failed to read prefab JSON. Expected entities/Tank.json and entities/Turret.json next to executable." << std::endl;
            return;
        }

        Engine::ECS::Prefab tankPrefab = Engine::ECS::loadPrefabFromJson(tankJson, ecs.components, ecs.archetypes);
        Engine::ECS::Prefab turretPrefab = Engine::ECS::loadPrefabFromJson(turretJson, ecs.components, ecs.archetypes);
        ecs.prefabs.add(tankPrefab);
        ecs.prefabs.add(turretPrefab);

        // Systems: build masks once after components are ensured by prefab load.
        m_movementSystem.buildMasks(ecs.components);

        // Spawn entities
        spawnEntities();
    }

    void spawnEntities()
    {
        auto &ecs = GetECS();
        const Engine::ECS::Prefab *tankPrefab = ecs.prefabs.get("Tank");
        const Engine::ECS::Prefab *turretPrefab = ecs.prefabs.get("Turret");
        if (!tankPrefab || !turretPrefab)
        {
            std::cerr << "Prefabs missing (Tank/Turret)" << std::endl;
            return;
        }

        // Deterministic RNG for repeatable behavior
        std::mt19937 rng(42);
        std::uniform_real_distribution<float> posDist(-0.8f, 0.8f);
        std::uniform_real_distribution<float> velDist(0.05f, 0.18f);

        // 10 tanks
        for (int i = 0; i < 10; ++i)
        {
            Engine::ECS::SpawnResult res = Engine::ECS::spawnFromPrefab(*tankPrefab, ecs.components, ecs.archetypes, ecs.stores, ecs.entities);
            Engine::ECS::ArchetypeStore *store = ecs.stores.get(res.archetypeId);
            if (!store)
                continue;

            // Override defaults with spread positions / velocities
            if (store->hasPosition())
            {
                auto &pos = store->positions()[res.row];
                pos.x = posDist(rng);
                pos.y = posDist(rng);
                pos.z = 0.0f;
            }
            if (store->hasVelocity())
            {
                auto &vel = store->velocities()[res.row];
                vel.x = (i % 2 == 0 ? 1.0f : -1.0f) * velDist(rng);
                vel.y = (i % 3 == 0 ? 1.0f : -1.0f) * (velDist(rng) * 0.6f);
                vel.z = 0.0f;
            }
        }

        // 5 turrets
        for (int i = 0; i < 5; ++i)
        {
            Engine::ECS::SpawnResult res = Engine::ECS::spawnFromPrefab(*turretPrefab, ecs.components, ecs.archetypes, ecs.stores, ecs.entities);
            Engine::ECS::ArchetypeStore *store = ecs.stores.get(res.archetypeId);
            if (!store)
                continue;

            if (store->hasPosition())
            {
                auto &pos = store->positions()[res.row];
                pos.x = posDist(rng);
                pos.y = posDist(rng);
                pos.z = 0.0f;
            }
        }
    }

    void updateECS(Engine::TimeStep ts)
    {
        auto &ecs = GetECS();

        // Move entities with Position+Velocity
        m_movementSystem.update(ecs.stores, static_cast<float>(ts.DeltaSeconds));

        // Bounce tanks at the viewport border: when trying to move out of NDC, flip velocity.
        // Tanks are the archetypes that include Velocity.
        constexpr float kMin = -1.0f;
        constexpr float kMax = 1.0f;
        for (const auto &ptr : ecs.stores.stores())
        {
            if (!ptr)
                continue;
            auto &store = *ptr;
            if (!store.hasPosition())
                continue;

            if (!store.hasVelocity())
                continue; // turrets

            auto &positions = store.positions();
            auto &velocities = store.velocities();
            const uint32_t n = store.size();
            for (uint32_t i = 0; i < n; ++i)
            {
                auto &p = positions[i];
                auto &v = velocities[i];

                if (p.x < kMin)
                {
                    p.x = kMin;
                    v.x = -v.x;
                }
                else if (p.x > kMax)
                {
                    p.x = kMax;
                    v.x = -v.x;
                }

                if (p.y < kMin)
                {
                    p.y = kMin;
                    v.y = -v.y;
                }
                else if (p.y > kMax)
                {
                    p.y = kMax;
                    v.y = -v.y;
                }
            }
        }

        // From time to time, tweak tank velocities slightly (keeps the ECS "alive").
        m_velocityJitterAccum += ts.DeltaSeconds;
        if (m_velocityJitterAccum >= 2.0)
        {
            m_velocityJitterAccum = 0.0;
            std::mt19937 rng(static_cast<uint32_t>(m_velocitySeed++));
            std::uniform_real_distribution<float> mulDist(0.75f, 1.25f);

            for (const auto &ptr : ecs.stores.stores())
            {
                if (!ptr)
                    continue;
                auto &store = *ptr;
                if (!store.hasVelocity())
                    continue;

                auto &velocities = store.velocities();
                for (auto &v : velocities)
                {
                    const float m = mulDist(rng);
                    v.x *= m;
                    v.y *= m;
                }
            }
        }

        // Stream instance buffer for rendering: {x,y,r,g,b} per entity
        uploadInstancesFromECS();
    }

    void uploadInstancesFromECS()
    {
        auto &ecs = GetECS();
        if (!m_trianglesPass)
            return;

        std::vector<float> instances;

        // Gather entities from each archetype store
        for (const auto &ptr : ecs.stores.stores())
        {
            if (!ptr)
                continue;
            const auto &store = *ptr;
            if (!store.hasPosition())
                continue;

            const bool isTank = store.hasVelocity();
            const auto &positions = store.positions();
            const uint32_t n = store.size();

            for (uint32_t i = 0; i < n; ++i)
            {
                const auto &p = positions[i];

                // Shades: tanks = greens, turrets = reds
                float r = 0.10f, g = 0.10f, b = 0.10f;
                if (isTank)
                {
                    const float shade = 0.55f + 0.45f * (static_cast<float>((i % 10)) / 9.0f);
                    r = 0.10f;
                    g = shade;
                    b = 0.10f;
                }
                else
                {
                    const float shade = 0.55f + 0.45f * (static_cast<float>((i % 5)) / 4.0f);
                    r = shade;
                    g = 0.10f;
                    b = 0.10f;
                }

                instances.push_back(p.x);
                instances.push_back(p.y);
                instances.push_back(r);
                instances.push_back(g);
                instances.push_back(b);
            }
        }

        if (instances.empty())
        {
            Engine::TrianglesRenderPassModule::InstanceBinding inst{};
            inst.instanceBuffer = m_triangleInstancesVB.buffer;
            inst.offset = 0;
            inst.instanceCount = 1;
            m_trianglesPass->setInstanceBinding(inst);
            return;
        }

        VkDevice device = GetVulkanContext().GetDevice();
        VkPhysicalDevice phys = GetVulkanContext().GetPhysicalDevice();
        VkDeviceSize bytes = sizeof(float) * instances.size();
        VkResult r = Engine::CreateOrUpdateVertexBuffer(device, phys, instances.data(), bytes, m_triangleInstancesVB);
        if (r != VK_SUCCESS)
        {
            std::cerr << "Failed to update instance buffer" << std::endl;
            return;
        }

        Engine::TrianglesRenderPassModule::InstanceBinding inst{};
        inst.instanceBuffer = m_triangleInstancesVB.buffer;
        inst.offset = 0;
        inst.instanceCount = static_cast<uint32_t>(instances.size() / 5);
        m_trianglesPass->setInstanceBinding(inst);
    }

    void setupMeshFromAssets()
    {
        // Load cooked mesh via AssetManager
        const char *path = "assets/ObjModels/male.smesh";
        m_bugattiHandle = m_assets->loadMesh(path);
        Engine::MeshAsset *asset = m_assets->getMesh(m_bugattiHandle);
        if (!asset)
        {
            std::cerr << "Failed to load/get mesh asset: " << path << std::endl;
            return;
        }

        // Create & register mesh pass
        m_meshPass = std::make_shared<Engine::MeshRenderPassModule>();
        Engine::MeshRenderPassModule::MeshBinding binding{};
        binding.vertexBuffer = asset->getVertexBuffer();
        binding.vertexOffset = 0;
        binding.indexBuffer = asset->getIndexBuffer();
        binding.indexOffset = 0;
        binding.indexCount = asset->getIndexCount();
        binding.indexType = asset->getIndexType();
        m_meshPass->setMesh(binding);

        GetRenderer().registerPass(m_meshPass);
    }

    void updateTriangleVisibility()
    {
        if (!m_trianglesPass)
            return;
        // When mesh is visible, hide triangle by setting vertexCount=0
        Engine::TrianglesRenderPassModule::VertexBinding binding = m_triangleBinding;
        binding.vertexCount = m_showMesh ? 0 : 3;
        m_trianglesPass->setVertexBinding(binding);
    }

private:
    // Asset management
    std::unique_ptr<Engine::AssetManager> m_assets;
    Engine::MeshHandle m_bugattiHandle{};

    // Triangle state
    Engine::VertexBufferHandle m_triangleVB{};
    Engine::VertexBufferHandle m_triangleInstancesVB{};
    std::shared_ptr<Engine::TrianglesRenderPassModule> m_trianglesPass;
    Engine::TrianglesRenderPassModule::VertexBinding m_triangleBinding{};
    bool m_showMesh = false;
    double m_timeAccum = 0.0;

    // ECS systems / behavior
    MovementSystem m_movementSystem{};
    double m_velocityJitterAccum = 0.0;
    uint64_t m_velocitySeed = 1337;

    // Mesh state
    std::shared_ptr<Engine::MeshRenderPassModule> m_meshPass;
};

int main()
{
    try
    {
        MySampleApp app;
        app.Run();
    }
    catch (const std::exception &e)
    {
        std::cerr << "Unhandled exception: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}