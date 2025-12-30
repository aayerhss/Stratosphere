#pragma once
#include <vulkan/vulkan.h>
#include <cstdint>
#include "assets/MeshFormats.h"
#include "utils/BufferUtils.h"

// This class is responsible for uploading mesh data to GPU and managing the associated resources.
namespace Engine
{

    // GPU-backed mesh asset: owns vertex/index buffers and basic metadata (AABB, counts)
    class MeshAsset
    {
    public:
        MeshAsset() = default;
        ~MeshAsset() = default;

        // Upload MeshData into GPU buffers (host-visible for now)
        bool upload(VkDevice device, VkPhysicalDevice phys, const MeshData &data);

        // Destroy GPU resources
        void destroy(VkDevice device);

        // Accessors for rendering
        VkBuffer getVertexBuffer() const { return m_vb.buffer; }
        VkBuffer getIndexBuffer() const { return m_ib.buffer; }
        uint32_t getIndexCount() const { return m_indexCount; }
        VkIndexType getIndexType() const { return m_indexType; }
        const float *getAABBMin() const { return m_aabbMin; }
        const float *getAABBMax() const { return m_aabbMax; }

    private:
        VertexBufferHandle m_vb{}; // host-visible buffer for simplicity
        IndexBufferHandle m_ib{};  // host-visible buffer for simplicity
        uint32_t m_indexCount = 0;
        VkIndexType m_indexType = VK_INDEX_TYPE_UINT32;
        float m_aabbMin[3]{};
        float m_aabbMax[3]{};
    };

} // namespace Engine