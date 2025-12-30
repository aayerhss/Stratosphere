#include "assets/AssetManager.h"
#include <utility>

namespace Engine
{

    AssetManager::AssetManager(VkDevice device,
                               VkPhysicalDevice phys,
                               VkQueue graphicsQueue,
                               uint32_t graphicsQueueFamilyIndex)
        : m_device(device),
          m_phys(phys),
          m_graphicsQueue(graphicsQueue),
          m_graphicsQueueFamilyIndex(graphicsQueueFamilyIndex) {}

    AssetManager::~AssetManager()
    {
        // Destroy all GPU resources on shutdown
        for (auto &kv : m_meshes)
        {
            if (kv.second.asset)
            {
                kv.second.asset->destroy(m_device);
            }
        }
        m_meshes.clear();
        m_meshPathCache.clear();
    }

    MeshHandle AssetManager::loadMesh(const std::string &cookedMeshPath)
    {
        // Check path cache first
        auto it = m_meshPathCache.find(cookedMeshPath);
        if (it != m_meshPathCache.end())
        {
            addRef(it->second);
            return it->second;
        }

        // Load CPU-side mesh data (.smesh)
        MeshData data;
        if (!LoadSMeshV0FromFile(cookedMeshPath, data))
        {
            return MeshHandle{}; // invalid
        }

        // Upload to GPU and register
        MeshHandle h = createMeshFromData(data, cookedMeshPath);
        if (h.isValid())
        {
            m_meshPathCache.emplace(cookedMeshPath, h);
        }
        return h;
    }

    MeshHandle AssetManager::createMeshFromData(const MeshData &data, const std::string &path)
    {
        // Create a transient command pool for one-shot staging uploads
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = m_graphicsQueueFamilyIndex;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

        VkCommandPool uploadPool = VK_NULL_HANDLE;
        VkResult pr = vkCreateCommandPool(m_device, &poolInfo, nullptr, &uploadPool);
        if (pr != VK_SUCCESS)
        {
            return MeshHandle{};
        }

        auto asset = std::make_unique<MeshAsset>();
        const bool ok = asset->upload(
            m_device, m_phys, uploadPool, m_graphicsQueue, data);

        // We can destroy the transient pool immediately after upload
        vkDestroyCommandPool(m_device, uploadPool, nullptr);

        if (!ok)
        {
            return MeshHandle{}; // upload failure
        }

        const uint64_t id = m_nextID++;
        MeshEntry entry;
        entry.asset = std::move(asset);
        entry.generation = 1;
        entry.refCount = 1; // caller gets an initial reference
        entry.path = path;

        m_meshes.emplace(id, std::move(entry));

        MeshHandle h;
        h.id = id;
        h.generation = 1;
        return h;
    }

    MeshAsset *AssetManager::getMesh(MeshHandle h)
    {
        auto it = m_meshes.find(h.id);
        if (it == m_meshes.end())
            return nullptr;
        if (it->second.generation != h.generation)
            return nullptr;
        return it->second.asset.get();
    }

    void AssetManager::addRef(MeshHandle h)
    {
        auto it = m_meshes.find(h.id);
        if (it != m_meshes.end() && it->second.generation == h.generation)
        {
            it->second.refCount++;
        }
    }

    void AssetManager::release(MeshHandle h)
    {
        auto it = m_meshes.find(h.id);
        if (it != m_meshes.end() && it->second.generation == h.generation)
        {
            if (it->second.refCount > 0)
                it->second.refCount--;
        }
    }

    void AssetManager::garbageCollect()
    {
        // Destroy assets with zero references
        for (auto it = m_meshes.begin(); it != m_meshes.end();)
        {
            if (it->second.refCount == 0)
            {
                if (it->second.asset)
                {
                    it->second.asset->destroy(m_device);
                }
                m_meshPathCache.erase(it->second.path);
                it = m_meshes.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

}