#pragma once
#include <vulkan/vulkan.h>
#include <cstdint>

namespace Engine
{

    struct VertexBufferHandle
    {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
    };

    struct IndexBufferHandle
    {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
    };

    // Host-visible vertex buffer (used as a staging source).
    // Implementation should create with usage:
    //   VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT
    VkResult CreateOrUpdateVertexBuffer(
        VkDevice device,
        VkPhysicalDevice physicalDevice,
        const void *vertexData,
        VkDeviceSize dataSize,
        VertexBufferHandle &handle);

    // Destroy buffer and memory held by VertexBufferHandle
    void DestroyVertexBuffer(VkDevice device, VertexBufferHandle &handle);

    // Host-visible index buffer (used as a staging source).
    // Implementation should create with usage:
    //   VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT
    VkResult CreateOrUpdateIndexBuffer(
        VkDevice device,
        VkPhysicalDevice physicalDevice,
        const void *indexData,
        VkDeviceSize dataSize,
        IndexBufferHandle &handle);

    void DestroyIndexBuffer(VkDevice device, IndexBufferHandle &handle);

    // Create a device-local buffer (not mappable) for fast GPU reads.
    // Caller must include the final role bit(s) and VK_BUFFER_USAGE_TRANSFER_DST_BIT in 'usage',
    // e.g., VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
    // or     VK_BUFFER_USAGE_INDEX_BUFFER_BIT  | VK_BUFFER_USAGE_TRANSFER_DST_BIT
    VkResult CreateDeviceLocalBuffer(
        VkDevice device,
        VkPhysicalDevice physicalDevice,
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkBuffer &outBuffer,
        VkDeviceMemory &outMemory);

    // Copy bytes from src to dst using a one-time command buffer.
    // Requirements:
    //  - src must have VK_BUFFER_USAGE_TRANSFER_SRC_BIT
    //  - dst must have VK_BUFFER_USAGE_TRANSFER_DST_BIT
    VkResult CopyBuffer(
        VkDevice device,
        VkCommandPool commandPool,
        VkQueue queue,
        VkBuffer src,
        VkBuffer dst,
        VkDeviceSize size);

} // namespace Engine