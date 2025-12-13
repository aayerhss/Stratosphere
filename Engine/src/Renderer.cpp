#include <stdexcept>
#include <iostream>
#include <cstring>
#include "Renderer.h"
#include "Engine/VulkanContext.h"
#include "Engine/SwapChain.h"

namespace Engine
{
    Renderer::Renderer(VulkanContext *ctx, SwapChain *swapchain, uint32_t maxFramesInFlight)
        : m_ctx(ctx), m_swapchain(swapchain), m_maxFrames(maxFramesInFlight)
    {
        if (!m_ctx || !m_swapchain)
        {
            throw std::runtime_error("Renderer: VulkanContext and Swapchain must be non-null");
        }

        // Extract commonly used handles (adjust if your context uses getters)
        m_device = m_ctx->GetDevice();
        m_graphicsQueue = m_ctx->GetGraphicsQueue();
        m_presentQueue = m_ctx->GetPresentQueue();

        m_swapchainImageFormat = m_swapchain->GetImageFormat();
        m_extent = m_swapchain->GetExtent();
    }

    Renderer::~Renderer()
    {
        // Ensure cleanup was called
        if (m_initialized)
        {
            try
            {
                cleanup();
            }
            catch (...)
            {
                // Avoid throwing from destructor
            }
        }
    }

    void Renderer::init()
    {
        if (m_initialized)
            return;

        // prepare per-frame slots
        m_frames.resize(m_maxFrames);

        createMainRenderPass();
        createFramebuffers();
        createSyncObjects();
        createCommandPoolsAndBuffers();

        // notify registered passes so they can create pipelines/resources that depend on renderpass/framebuffers
        for (auto &p : m_passes)
        {
            if (p)
                p->onCreate(*m_ctx, m_mainRenderPass, m_framebuffers);
        }

        m_initialized = true;
    }

    void Renderer::cleanup()
    {
        if (!m_initialized)
            return;

        // Wait for GPU to finish using resources before destroying them
        vkDeviceWaitIdle(m_device);

        // Optionally notify passes about resize/teardown so they can free resources.
        // Here we send an empty extent; adapt if your module API has a dedicated destroy hook.
        for (auto &p : m_passes)
        {
            if (p)
                p->onResize(*m_ctx, {0, 0});
        }

        destroyCommandPoolsAndBuffers();
        destroySyncObjects();

        // Destroy framebuffers
        for (auto fb : m_framebuffers)
        {
            if (fb != VK_NULL_HANDLE)
            {
                vkDestroyFramebuffer(m_device, fb, nullptr);
            }
        }
        m_framebuffers.clear();

        // Destroy main render pass
        if (m_mainRenderPass != VK_NULL_HANDLE)
        {
            vkDestroyRenderPass(m_device, m_mainRenderPass, nullptr);
            m_mainRenderPass = VK_NULL_HANDLE;
        }

        m_initialized = false;
    }

    void Renderer::registerPass(std::shared_ptr<RenderPassModule> pass)
    {
        if (!pass)
            return;
        m_passes.push_back(pass);
        if (m_initialized)
        {
            // Immediately call onCreate so the pass can create pipelines that depend on renderpass/framebuffers.
            pass->onCreate(*m_ctx, m_mainRenderPass, m_framebuffers);
        }
    }

    void Renderer::createMainRenderPass()
    {
        // Color attachment tied to swapchain image format
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = m_swapchainImageFormat;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;

        // Subpass dependency from external -> subpass 0
        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo rpInfo{};
        rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rpInfo.attachmentCount = 1;
        rpInfo.pAttachments = &colorAttachment;
        rpInfo.subpassCount = 1;
        rpInfo.pSubpasses = &subpass;
        rpInfo.dependencyCount = 1;
        rpInfo.pDependencies = &dependency;

        if (vkCreateRenderPass(m_device, &rpInfo, nullptr, &m_mainRenderPass) != VK_SUCCESS)
        {
            throw std::runtime_error("Renderer::createMainRenderPass - failed to create render pass");
        }
    }

    void Renderer::createFramebuffers()
    {
        const auto &imageViews = m_swapchain->GetImageViews();
        m_framebuffers.resize(imageViews.size());

        for (size_t i = 0; i < imageViews.size(); ++i)
        {
            VkImageView attachments[1] = {imageViews[i]};

            VkFramebufferCreateInfo fbInfo{};
            fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            fbInfo.renderPass = m_mainRenderPass;
            fbInfo.attachmentCount = 1;
            fbInfo.pAttachments = attachments;
            fbInfo.width = m_extent.width;
            fbInfo.height = m_extent.height;
            fbInfo.layers = 1;

            if (vkCreateFramebuffer(m_device, &fbInfo, nullptr, &m_framebuffers[i]) != VK_SUCCESS)
            {
                throw std::runtime_error("Renderer::createFramebuffers - failed to create framebuffer");
            }
        }
    }

    void Renderer::createSyncObjects()
    {
        // For now we create sync objects per frame in flight to support only single threaded rendering
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // start signaled so the first frame can be submitted immediately

        for (uint32_t i = 0; i < m_maxFrames; ++i)
        {
            FrameContext &f = m_frames[i];

            if (vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &f.imageAcquiredSemaphore) != VK_SUCCESS ||
                vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &f.renderFinishedSemaphore) != VK_SUCCESS)
            {
                throw std::runtime_error("Renderer::createSyncObjects - failed to create semaphores");
            }

            if (vkCreateFence(m_device, &fenceInfo, nullptr, &f.inFlightFence) != VK_SUCCESS)
            {
                throw std::runtime_error("Renderer::createSyncObjects - failed to create fence");
            }
        }
    }

    void Renderer::createCommandPoolsAndBuffers()
    {
        // For now we create one command pool and buffer per frame in flight to support only single threaded rendering
        for (uint32_t i = 0; i < m_maxFrames; ++i)
        {
            FrameContext &f = m_frames[i];

            VkCommandPoolCreateInfo poolInfo{};
            poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            poolInfo.queueFamilyIndex = m_ctx->GetGraphicsQueueFamilyIndex(); // adjust if different in your context

            if (vkCreateCommandPool(m_device, &poolInfo, nullptr, &f.commandPool) != VK_SUCCESS)
            {
                throw std::runtime_error("Renderer::createCommandPoolsAndBuffers - failed to create command pool");
            }

            VkCommandBufferAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            allocInfo.commandPool = f.commandPool;
            allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocInfo.commandBufferCount = 1;

            if (vkAllocateCommandBuffers(m_device, &allocInfo, &f.commandBuffer) != VK_SUCCESS)
            {
                throw std::runtime_error("Renderer::createCommandPoolsAndBuffers - failed to allocate command buffer");
            }
        }
    }

    void Renderer::destroySyncObjects()
    {
        for (auto &f : m_frames)
        {
            if (f.imageAcquiredSemaphore != VK_NULL_HANDLE)
            {
                vkDestroySemaphore(m_device, f.imageAcquiredSemaphore, nullptr);
                f.imageAcquiredSemaphore = VK_NULL_HANDLE;
            }
            if (f.renderFinishedSemaphore != VK_NULL_HANDLE)
            {
                vkDestroySemaphore(m_device, f.renderFinishedSemaphore, nullptr);
                f.renderFinishedSemaphore = VK_NULL_HANDLE;
            }
            if (f.inFlightFence != VK_NULL_HANDLE)
            {
                vkDestroyFence(m_device, f.inFlightFence, nullptr);
                f.inFlightFence = VK_NULL_HANDLE;
            }
        }
    }

    void Renderer::destroyCommandPoolsAndBuffers()
    {
        for (auto &f : m_frames)
        {
            if (f.commandPool != VK_NULL_HANDLE)
            {
                vkDestroyCommandPool(m_device, f.commandPool, nullptr);
                f.commandPool = VK_NULL_HANDLE;
                f.commandBuffer = VK_NULL_HANDLE;
            }
        }
    }
}