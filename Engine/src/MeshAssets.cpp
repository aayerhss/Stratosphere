#include "assets/MeshAsset.h"
#include <cstring>

namespace Engine
{

    bool MeshAsset::upload(VkDevice device, VkPhysicalDevice phys, const MeshData &data)
    {
        // Create/update vertex buffer
        VkResult rv = CreateOrUpdateVertexBuffer(
            device, phys,
            data.vertexBytes.data(),
            static_cast<VkDeviceSize>(data.vertexBytes.size()),
            m_vb);
        if (rv != VK_SUCCESS)
            return false;

        // Create/update index buffer (choose type from header)
        VkResult ri = VK_SUCCESS;
        if (data.indexFormat == 1)
        {
            m_indexType = VK_INDEX_TYPE_UINT32;
            m_indexCount = data.indexCount;
            ri = CreateOrUpdateIndexBuffer(
                device, phys,
                data.indices32.data(),
                static_cast<VkDeviceSize>(data.indices32.size() * sizeof(uint32_t)),
                m_ib);
        }
        else
        {
            m_indexType = VK_INDEX_TYPE_UINT16;
            m_indexCount = data.indexCount;
            ri = CreateOrUpdateIndexBuffer(
                device, phys,
                data.indices16.data(),
                static_cast<VkDeviceSize>(data.indices16.size() * sizeof(uint16_t)),
                m_ib);
        }
        if (ri != VK_SUCCESS)
            return false;

        // Copy AABB
        std::memcpy(m_aabbMin, data.aabbMin, sizeof(m_aabbMin));
        std::memcpy(m_aabbMax, data.aabbMax, sizeof(m_aabbMax));

        return true;
    }

    void MeshAsset::destroy(VkDevice device)
    {
        DestroyVertexBuffer(device, m_vb);
        DestroyIndexBuffer(device, m_ib);
        m_indexCount = 0;
    }

} // namespace Engine