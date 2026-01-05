#pragma once
#include "Engine/Renderer.h"
#include "Engine/Pipeline.h"
#include "utils/BufferUtils.h"
#include <vulkan/vulkan.h>
#include <vector>

namespace Engine
{

    // Simple module to draw N triangles (3*N vertices) from a vertex buffer.
    // Vertex format: location 0 = vec2 position, location 1 = vec3 color (optional).
    class TrianglesRenderPassModule : public RenderPassModule
    {
    public:
        struct VertexBinding
        {
            VkBuffer vertexBuffer = VK_NULL_HANDLE;
            VkDeviceSize offset = 0;
            uint32_t vertexCount = 0; // must be multiple of 3 for triangles
        };

        // Per-instance data (binding 1):
        //  - location 2 = vec2 offset
        //  - location 3 = vec3 color
        struct InstanceBinding
        {
            VkBuffer instanceBuffer = VK_NULL_HANDLE;
            VkDeviceSize offset = 0;
            uint32_t instanceCount = 1;
        };

        TrianglesRenderPassModule() = default;
        ~TrianglesRenderPassModule() override;

        // Provide/Update vertex buffer binding
        void setVertexBinding(const VertexBinding &binding);

        // Provide/Update instance buffer binding (optional).
        // If instanceBuffer is VK_NULL_HANDLE, a default (0,0) instance will be used.
        void setInstanceBinding(const InstanceBinding &binding)
        {
            m_instances = binding;
        }

        void setOffset(float x, float y)
        {
            m_offset[0] = x;
            m_offset[1] = y;
        }

        // RenderPassModule interface
        void onCreate(VulkanContext &ctx, VkRenderPass pass, const std::vector<VkFramebuffer> &fbs) override;
        void record(FrameContext &frameCtx, VkCommandBuffer cmd) override;
        void onResize(VulkanContext &ctx, VkExtent2D newExtent) override;
        void onDestroy(VulkanContext &ctx) override;

    private:
        void destroyResources();
        void createPipeline(VulkanContext &ctx, VkRenderPass pass);

    private:
        VkDevice m_device = VK_NULL_HANDLE;
        VkExtent2D m_extent{};
        Pipeline m_pipeline;
        VertexBinding m_binding;
        InstanceBinding m_instances;
        VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
        float m_offset[2] = {0.0f, 0.0f};

        // Default instance buffer so the shader always has binding 1.
        VertexBufferHandle m_defaultInstanceVB{};
    };

} // namespace Engine