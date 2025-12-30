#pragma once
#include <vulkan/vulkan.h>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <memory>

#include "assets/MeshFormats.h" // MeshData + LoadSMeshV0FromFile
#include "assets/MeshAsset.h"   // GPU-backed mesh asset

namespace Engine
{

    // Strongly-typed handle for mesh assets
    struct MeshHandle
    {
        uint64_t id = 0;
        uint32_t generation = 0;
        bool isValid() const { return id != 0; }
    };

    // Central manager for loading, caching, and destroying assets.
    // Phase 1: Mesh-only (expand to textures/materials later).
    class AssetManager
    {
    public:
        // Updated: requires graphics queue + its family index.
        // AssetManager will create a transient command pool per upload.
        AssetManager(VkDevice device,
                     VkPhysicalDevice phys,
                     VkQueue graphicsQueue,
                     uint32_t graphicsQueueFamilyIndex);
        ~AssetManager();

        // Synchronous load: returns a handle and caches by path. Adds one reference to the asset.
        MeshHandle loadMesh(const std::string &cookedMeshPath);

        // Access raw asset pointer (nullptr if invalid/stale).
        MeshAsset *getMesh(MeshHandle h);

        // Reference counting for lifetime management.
        void addRef(MeshHandle h);
        void release(MeshHandle h);

        // Destroy assets with refCount == 0 and remove from caches.
        void garbageCollect();

    private:
        MeshHandle createMeshFromData(const MeshData &data, const std::string &path);

    private:
        VkDevice m_device = VK_NULL_HANDLE;
        VkPhysicalDevice m_phys = VK_NULL_HANDLE;
        VkQueue m_graphicsQueue = VK_NULL_HANDLE;
        uint32_t m_graphicsQueueFamilyIndex = 0;

        uint64_t m_nextID = 1;

        struct MeshEntry
        {
            std::unique_ptr<MeshAsset> asset;
            uint32_t generation = 1;
            uint32_t refCount = 0;
            std::string path;
        };

        // Storage by ID
        std::unordered_map<uint64_t, MeshEntry> m_meshes;

        // Path → handle cache to avoid duplicate loads
        std::unordered_map<std::string, MeshHandle> m_meshPathCache;
    };

} // namespace Engine