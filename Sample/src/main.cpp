#include "Engine/Application.h"
#include "Engine/TrianglesRenderPassModule.h"
#include "Engine/MeshRenderPassModule.h"
#include "assets/AssetManager.h"
#include "assets/MeshAsset.h"
#include "assets/MeshFormats.h"
#include "Engine/VulkanContext.h"

#include "utils/BufferUtils.h" // CreateOrUpdateVertexBuffer + DestroyVertexBuffer
#include <iostream>
#include <memory>

class MySampleApp : public Engine::Application
{
public:
    MySampleApp() : Engine::Application()
    { // Create the AssetManager (uses Vulkan device & physical device)

        m_assets = std::make_unique<Engine::AssetManager>(
            GetVulkanContext().GetDevice(),
            GetVulkanContext().GetPhysicalDevice());

        // 1) Setup triangle (existing sample)
        setupTriangle();

        // 2) Load mesh via AssetManager and bind to MeshRenderPassModule
        setupMeshFromAssets();

        // Start with triangle visible; mesh hidden
        m_showMesh = false;
        if (m_meshPass)
            m_meshPass->setEnabled(false);
        // Triangle: visible initially
        updateTriangleVisibility();
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

        // Release passes
        m_meshPass.reset();
        m_trianglesPass.reset();

        Engine::Application::Close();
    }

    void OnUpdate(Engine::TimeStep ts) override
    {
        // Toggle every 10 seconds
        m_timeAccum += ts.DeltaSeconds;
        if (m_timeAccum >= 10.0)
        {
            m_timeAccum = 0.0;
            m_showMesh = !m_showMesh;

            // Toggle mesh visibility
            if (m_meshPass)
                m_meshPass->setEnabled(m_showMesh);

            // Toggle triangle visibility (hide by setting vertexCount to 0)
            updateTriangleVisibility();
        }
    }

    void OnRender() override
    {
        // Rendering handled by Renderer/Engine; no manual draw calls here
    }

private:
    void setupTriangle()
    {
        // Interleaved vertex data: vec2 position, vec3 color (matches your triangle pipeline)
        const float vertices[] = {
            // x,    y,    r, g, b
            0.0f,
            -0.1f,
            1.0f,
            0.0f,
            0.0f,
            0.1f,
            0.1f,
            0.0f,
            1.0f,
            0.0f,
            -0.1f,
            0.1f,
            0.0f,
            0.0f,
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
        m_triangleBinding.vertexCount = 3; // visible initially
        m_trianglesPass->setVertexBinding(m_triangleBinding);

        // Register pass to renderer
        GetRenderer().registerPass(m_trianglesPass);

        // Initial offset (push constants) if supported by your module
        m_trianglesPass->setOffset(0.0f, 0.0f);
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
    std::shared_ptr<Engine::TrianglesRenderPassModule> m_trianglesPass;
    Engine::TrianglesRenderPassModule::VertexBinding m_triangleBinding{};
    bool m_showMesh = false;
    double m_timeAccum = 0.0;

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