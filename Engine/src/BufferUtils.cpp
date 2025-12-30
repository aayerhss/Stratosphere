#include "utils/BufferUtils.h"
#include <cstring>
#include <stdexcept>

namespace Engine
{
    static uint32_t findMemoryType(VkPhysicalDevice phys, uint32_t typeFilter, VkMemoryPropertyFlags properties)
    {
        VkPhysicalDeviceMemoryProperties memProps;
        vkGetPhysicalDeviceMemoryProperties(phys, &memProps);
        for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
        {
            if ((typeFilter & (1u << i)) && (memProps.memoryTypes[i].propertyFlags & properties) == properties)
            {
                return i;
            }
        }
        throw std::runtime_error("Failed to find suitable memory type");
    }

    VkResult CreateOrUpdateVertexBuffer(
        VkDevice device,
        VkPhysicalDevice physicalDevice,
        const void *vertexData,
        VkDeviceSize dataSize,
        VertexBufferHandle &handle)
    {
        if (!vertexData || dataSize == 0)
        {
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        bool needCreate = (handle.buffer == VK_NULL_HANDLE);
        VkMemoryRequirements req{};
        if (!needCreate)
        {
            vkGetBufferMemoryRequirements(device, handle.buffer, &req);
            if (req.size < dataSize)
            {
                vkDestroyBuffer(device, handle.buffer, nullptr);
                handle.buffer = VK_NULL_HANDLE;
                vkFreeMemory(device, handle.memory, nullptr);
                handle.memory = VK_NULL_HANDLE;
                needCreate = true;
            }
        }

        if (needCreate)
        {
            VkBufferCreateInfo bi{};
            bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bi.size = dataSize;
            // Include TRANSFER_SRC so this host-visible buffer can be the staging source.
            bi.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            VkBuffer buffer = VK_NULL_HANDLE;
            VkResult r = vkCreateBuffer(device, &bi, nullptr, &buffer);
            if (r != VK_SUCCESS)
                return r;

            vkGetBufferMemoryRequirements(device, buffer, &req);

            VkMemoryAllocateInfo ai{};
            ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            ai.allocationSize = req.size;
            ai.memoryTypeIndex = findMemoryType(physicalDevice, req.memoryTypeBits,
                                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

            VkDeviceMemory mem = VK_NULL_HANDLE;
            r = vkAllocateMemory(device, &ai, nullptr, &mem);
            if (r != VK_SUCCESS)
            {
                vkDestroyBuffer(device, buffer, nullptr);
                return r;
            }

            r = vkBindBufferMemory(device, buffer, mem, 0);
            if (r != VK_SUCCESS)
            {
                vkDestroyBuffer(device, buffer, nullptr);
                vkFreeMemory(device, mem, nullptr);
                return r;
            }

            handle.buffer = buffer;
            handle.memory = mem;
        }

        void *mapped = nullptr;
        VkResult r = vkMapMemory(device, handle.memory, 0, dataSize, 0, &mapped);
        if (r != VK_SUCCESS)
            return r;
        std::memcpy(mapped, vertexData, static_cast<size_t>(dataSize));
        vkUnmapMemory(device, handle.memory);

        return VK_SUCCESS;
    }

    void DestroyVertexBuffer(VkDevice device, VertexBufferHandle &handle)
    {
        if (handle.buffer != VK_NULL_HANDLE)
        {
            vkDestroyBuffer(device, handle.buffer, nullptr);
            handle.buffer = VK_NULL_HANDLE;
        }
        if (handle.memory != VK_NULL_HANDLE)
        {
            vkFreeMemory(device, handle.memory, nullptr);
            handle.memory = VK_NULL_HANDLE;
        }
    }

    // Index buffer (host-visible, also a staging source)
    VkResult CreateOrUpdateIndexBuffer(
        VkDevice device,
        VkPhysicalDevice physicalDevice,
        const void *indexData,
        VkDeviceSize dataSize,
        IndexBufferHandle &handle)
    {
        if (!indexData || dataSize == 0)
        {
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        bool needCreate = (handle.buffer == VK_NULL_HANDLE);
        VkMemoryRequirements req{};
        if (!needCreate)
        {
            vkGetBufferMemoryRequirements(device, handle.buffer, &req);
            if (req.size < dataSize)
            {
                vkDestroyBuffer(device, handle.buffer, nullptr);
                handle.buffer = VK_NULL_HANDLE;
                vkFreeMemory(device, handle.memory, nullptr);
                handle.memory = VK_NULL_HANDLE;
                needCreate = true;
            }
        }

        if (needCreate)
        {
            VkBufferCreateInfo bi{};
            bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bi.size = dataSize;
            // Include TRANSFER_SRC so this host-visible buffer can be the staging source.
            bi.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            VkBuffer buffer = VK_NULL_HANDLE;
            VkResult r = vkCreateBuffer(device, &bi, nullptr, &buffer);
            if (r != VK_SUCCESS)
                return r;

            vkGetBufferMemoryRequirements(device, buffer, &req);

            VkMemoryAllocateInfo ai{};
            ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            ai.allocationSize = req.size;
            ai.memoryTypeIndex = findMemoryType(physicalDevice, req.memoryTypeBits,
                                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

            VkDeviceMemory mem = VK_NULL_HANDLE;
            r = vkAllocateMemory(device, &ai, nullptr, &mem);
            if (r != VK_SUCCESS)
            {
                vkDestroyBuffer(device, buffer, nullptr);
                return r;
            }

            r = vkBindBufferMemory(device, buffer, mem, 0);
            if (r != VK_SUCCESS)
            {
                vkDestroyBuffer(device, buffer, nullptr);
                vkFreeMemory(device, mem, nullptr);
                return r;
            }

            handle.buffer = buffer;
            handle.memory = mem;
        }

        void *mapped = nullptr;
        VkResult r = vkMapMemory(device, handle.memory, 0, dataSize, 0, &mapped);
        if (r != VK_SUCCESS)
            return r;
        std::memcpy(mapped, indexData, static_cast<size_t>(dataSize));
        vkUnmapMemory(device, handle.memory);

        return VK_SUCCESS;
    }

    void DestroyIndexBuffer(VkDevice device, IndexBufferHandle &handle)
    {
        if (handle.buffer != VK_NULL_HANDLE)
        {
            vkDestroyBuffer(device, handle.buffer, nullptr);
            handle.buffer = VK_NULL_HANDLE;
        }
        if (handle.memory != VK_NULL_HANDLE)
        {
            vkFreeMemory(device, handle.memory, nullptr);
            handle.memory = VK_NULL_HANDLE;
        }
    }

    // Device-local buffer creation (not mappable)
    VkResult CreateDeviceLocalBuffer(
        VkDevice device,
        VkPhysicalDevice physicalDevice,
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkBuffer &outBuffer,
        VkDeviceMemory &outMemory)
    {
        VkBufferCreateInfo bi{};
        bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bi.size = size;
        bi.usage = usage; // caller must include VK_BUFFER_USAGE_TRANSFER_DST_BIT
        bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VkBuffer buffer = VK_NULL_HANDLE;
        VkResult r = vkCreateBuffer(device, &bi, nullptr, &buffer);
        if (r != VK_SUCCESS)
            return r;

        VkMemoryRequirements req{};
        vkGetBufferMemoryRequirements(device, buffer, &req);

        VkMemoryAllocateInfo ai{};
        ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        ai.allocationSize = req.size;
        ai.memoryTypeIndex = findMemoryType(physicalDevice, req.memoryTypeBits,
                                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VkDeviceMemory mem = VK_NULL_HANDLE;
        r = vkAllocateMemory(device, &ai, nullptr, &mem);
        if (r != VK_SUCCESS)
        {
            vkDestroyBuffer(device, buffer, nullptr);
            return r;
        }

        r = vkBindBufferMemory(device, buffer, mem, 0);
        if (r != VK_SUCCESS)
        {
            vkDestroyBuffer(device, buffer, nullptr);
            vkFreeMemory(device, mem, nullptr);
            return r;
        }

        outBuffer = buffer;
        outMemory = mem;
        return VK_SUCCESS;
    }

    // One-shot buffer copy (submit and wait idle)
    VkResult CopyBuffer(
        VkDevice device,
        VkCommandPool commandPool,
        VkQueue queue,
        VkBuffer src,
        VkBuffer dst,
        VkDeviceSize size)
    {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer cmd = VK_NULL_HANDLE;
        VkResult r = vkAllocateCommandBuffers(device, &allocInfo, &cmd);
        if (r != VK_SUCCESS)
            return r;

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        r = vkBeginCommandBuffer(cmd, &beginInfo);
        if (r != VK_SUCCESS)
        {
            vkFreeCommandBuffers(device, commandPool, 1, &cmd);
            return r;
        }

        VkBufferCopy copyRegion{};
        copyRegion.srcOffset = 0;
        copyRegion.dstOffset = 0;
        copyRegion.size = size;
        vkCmdCopyBuffer(cmd, src, dst, 1, &copyRegion);

        r = vkEndCommandBuffer(cmd);
        if (r != VK_SUCCESS)
        {
            vkFreeCommandBuffers(device, commandPool, 1, &cmd);
            return r;
        }

        VkSubmitInfo submit{};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &cmd;
        r = vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE);
        if (r == VK_SUCCESS)
        {
            vkQueueWaitIdle(queue);
        }

        vkFreeCommandBuffers(device, commandPool, 1, &cmd);
        return r;
    }

} // namespace Engine