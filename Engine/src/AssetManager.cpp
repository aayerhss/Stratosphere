#include "assets/AssetManager.h"
#include <utility>

namespace Engine
{

    AssetManager::AssetManager(VkDevice device, VkPhysicalDevice phys)
        : m_device(device), m_phys(phys) {}

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
        // Check path cache disctionary first
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
        auto asset = std::make_unique<MeshAsset>();
        if (!asset->upload(m_device, m_phys, data))
        {
            return MeshHandle{}; // upload failure
        }

        const uint64_t id = m_nextID++;
        MeshEntry entry;
        entry.asset = std::move(asset); // moveing the unique ptr
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

} // namespace Engine