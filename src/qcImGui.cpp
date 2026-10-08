#include "qcImGui.h"

#include "editor/editor_theme.h"
#include "imgui.h"
#if defined(_WIN32)
#include "imgui_impl_dx11.h"
#endif
#include "imgui_impl_opengl3.h"
#include "imgui_impl_vulkan.h"
#include "imgui_impl_sdl3.h"

#include <cmath>
#include <utility>
namespace
{

class CImGuiHost
{
public:
    enum class EBackend
    {
        NONE,
        OPENGL,
        VULKAN,
        D3D11
    };

    bool Setup(bool darkTheme);

    void Shutdown();

    void Begin();
    void End();
    void ProcessEvent(const SDL_Event* pEvent);

    bool IsUsable() const
    {
        return m_IsInitialized;
    }

    bool IsOpenGL() const
    {
        return m_IsInitialized && m_Backend == EBackend::OPENGL;
    }

    bool IsVulkan() const
    {
        return m_IsInitialized && m_Backend == EBackend::VULKAN;
    }

    bool IsD3D11() const
    {
        return m_IsInitialized && m_Backend == EBackend::D3D11;
    }

private:
    bool m_IsInitialized = false;
    EBackend m_Backend = EBackend::NONE;
};

CImGuiHost s_Host;

void QcImGuiEventBridge(const SDL_Event* pEvent)
{
    QcImGuiProcessEvent(pEvent);
}

void QcImGuiVulkanRenderCallback(VkCommandBuffer commandBuffer)
{
    if (!s_Host.IsVulkan())
    {
        return;
    }

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
}

#if defined(_WIN32)
void QcImGuiD3D11RenderCallback(ID3D11DeviceContext* pDeviceContext)
{
    if (!s_Host.IsD3D11() || pDeviceContext == nullptr)
        {
        return;
    }

    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}
#endif

ImTextureID QcImGuiTextureIdFor(const Texture2D* pTexture)
{
    if (pTexture == nullptr || pTexture->id == 0)
    {
        return 0;
    }

    if (GetCurrentBackend() == RendererType::Vulkan)
    {
        const VkDescriptorSet descriptor = GetVulkanTextureDescriptorSet(pTexture->id);
        if (descriptor == VK_NULL_HANDLE)
        {
            return 0;
        }
        return static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(descriptor));
    }

#if defined(_WIN32)
    if (GetCurrentBackend() == RendererType::D3D11)
    {
        ID3D11ShaderResourceView* pShaderResourceView =
            GetD3D11TextureShaderResourceView(pTexture->id);
        return static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(pShaderResourceView));
    }
#endif

    return static_cast<ImTextureID>(static_cast<uintptr_t>(pTexture->id));
}

bool CImGuiHost::Setup(bool darkTheme)
{
    if (m_IsInitialized)
    {
        return true;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    if (darkTheme)
    {
        ImGui::StyleColorsDark();
    }
    else
    {
        ImGui::StyleColorsClassic();
    }

    SDL_Window* pWindow = GetNativeWindow();
    if (pWindow == nullptr)
    {
        ImGui::DestroyContext();
        return false;
    }

    m_Backend = EBackend::NONE;

    const RendererType backend = GetCurrentBackend();
    bool initOk = false;

    if (backend == RendererType::OpenGL)
    {
        SDL_GLContext context = GetNativeContext();
        if (context == nullptr)
        {
            ImGui::DestroyContext();
            return false;
        }

        if (!ImGui_ImplSDL3_InitForOpenGL(pWindow, context))
        {
            ImGui::DestroyContext();
            return false;
        }
        if (!ImGui_ImplOpenGL3_Init("#version 330 core"))
        {
            ImGui_ImplSDL3_Shutdown();
            ImGui::DestroyContext();
            return false;
        }

        m_Backend = EBackend::OPENGL;
        initOk = true;
    }
    else if (backend == RendererType::Vulkan)
    {
        const VkInstance instance = GetVulkanInstance();
        const VkPhysicalDevice physicalDevice = GetVulkanPhysicalDevice();
        const VkDevice device = GetVulkanDevice();
        const VkQueue queue = GetVulkanGraphicsQueue();
        const uint32_t queueFamily = GetVulkanGraphicsQueueFamily();
        const VkDescriptorPool descriptorPool = GetVulkanDescriptorPool();
        const VkRenderPass renderPass = GetVulkanMainRenderPass();
        const uint32_t minImageCount = GetVulkanMinImageCount();
        const uint32_t imageCount = GetVulkanImageCount();

        if (instance == VK_NULL_HANDLE || physicalDevice == VK_NULL_HANDLE || device == VK_NULL_HANDLE ||
            queue == VK_NULL_HANDLE || queueFamily == UINT32_MAX || descriptorPool == VK_NULL_HANDLE ||
            renderPass == VK_NULL_HANDLE || minImageCount == 0 || imageCount == 0)
        {
            ImGui::DestroyContext();
            return false;
        }

        if (!ImGui_ImplSDL3_InitForVulkan(pWindow))
        {
            ImGui::DestroyContext();
            return false;
        }

        ImGui_ImplVulkan_InitInfo initInfo{};
        initInfo.ApiVersion = VK_API_VERSION_1_2;
        initInfo.Instance = instance;
        initInfo.PhysicalDevice = physicalDevice;
        initInfo.Device = device;
        initInfo.QueueFamily = queueFamily;
        initInfo.Queue = queue;
        initInfo.DescriptorPool = descriptorPool;
        initInfo.DescriptorPoolSize = 0;
        initInfo.MinImageCount = minImageCount;
        initInfo.ImageCount = imageCount;
        initInfo.PipelineInfoMain.RenderPass = renderPass;
        initInfo.PipelineInfoMain.Subpass = 0;
        initInfo.PipelineInfoMain.MSAASamples = GetVulkanMSAASamples();
        initInfo.UseDynamicRendering = false;
        initInfo.MinAllocationSize = 1024 * 1024;

        initOk = ImGui_ImplVulkan_Init(&initInfo);
        if (initOk)
        {
            SetVulkanRenderCallback(QcImGuiVulkanRenderCallback);
            m_Backend = EBackend::VULKAN;
        }
        else
        {
            ImGui_ImplSDL3_Shutdown();
        }
#if defined(_WIN32)
    }
    else if (backend == RendererType::D3D11)
    {
        ID3D11Device* pDevice = GetD3D11Device();
        ID3D11DeviceContext* pDeviceContext = GetD3D11ImmediateContext();
        if (!ImGui_ImplSDL3_InitForD3D(pWindow))
        {
            ImGui::DestroyContext();
            return false;
        }
        if (pDevice == nullptr || pDeviceContext == nullptr ||
            !ImGui_ImplDX11_Init(pDevice, pDeviceContext))
            {
            ImGui_ImplSDL3_Shutdown();
            ImGui::DestroyContext();
            return false;
        }

        SetD3D11RenderCallback(QcImGuiD3D11RenderCallback);
        m_Backend = EBackend::D3D11;
        initOk = true;
#endif
    }

    if (!initOk)
    {
        ImGui::DestroyContext();
        return false;
    }

    SetNativeEventCallback(QcImGuiEventBridge);
    m_IsInitialized = true;
    return true;
}

void CImGuiHost::Shutdown()
{
    if (!m_IsInitialized)
    {
        return;
    }

    SetNativeEventCallback(nullptr);
    SetVulkanRenderCallback(nullptr);
#if defined(_WIN32)
    SetD3D11RenderCallback(nullptr);
#endif

    if (m_Backend == EBackend::OPENGL)
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
    }
    else if (m_Backend == EBackend::VULKAN)
    {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplSDL3_Shutdown();
#if defined(_WIN32)
    }
    else if (m_Backend == EBackend::D3D11)
    {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplSDL3_Shutdown();
#endif
    }
    ImGui::DestroyContext();
    m_IsInitialized = false;
    m_Backend = EBackend::NONE;
}

void CImGuiHost::Begin()
{
    if (!m_IsInitialized)
    {
        return;
    }

    CThemeManager::ProcessPendingFonts();
    ImGui_ImplSDL3_NewFrame();
    if (m_Backend == EBackend::OPENGL)
    {
        ImGui_ImplOpenGL3_NewFrame();
    }
    else if (m_Backend == EBackend::VULKAN)
    {
        ImGui_ImplVulkan_NewFrame();
#if defined(_WIN32)
    }
    else if (m_Backend == EBackend::D3D11)
    {
        ImGui_ImplDX11_NewFrame();
#endif
    }
    ImGui::NewFrame();
}

void CImGuiHost::End()
{
    if (!m_IsInitialized)
    {
        return;
    }

    ImGui::Render();
    if (m_Backend == EBackend::OPENGL)
    {
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }
}

void CImGuiHost::ProcessEvent(const SDL_Event* pEvent)
{
    if (!m_IsInitialized || pEvent == nullptr)
    {
        return;
    }

    ImGui_ImplSDL3_ProcessEvent(pEvent);
}

} // anonymous

bool QcImGuiSetup(bool darkTheme)
{
    return s_Host.Setup(darkTheme);
}

void QcImGuiShutdown()
{
    s_Host.Shutdown();
}

void QcImGuiBegin()
{
    s_Host.Begin();
}

void QcImGuiEnd()
{
    s_Host.End();
}

void QcImGuiProcessEvent(const SDL_Event* pEvent)
{
    s_Host.ProcessEvent(pEvent);
}

ImTextureID QcImGuiGetTextureId(const Texture2D* pTexture)
{
    return QcImGuiTextureIdFor(pTexture);
}

void QcImGuiImage(const Texture2D* pTexture, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1)
{
    const ImTextureID textureId = QcImGuiTextureIdFor(pTexture);
    if (textureId == 0)
    {
        return;
    }
    ImGui::Image(textureId, size, uv0, uv1);
}

void QcImGuiAddImage(ImDrawList* pDrawList, const Texture2D* pTexture, const ImVec2& min, const ImVec2& max, const ImVec2& uv0, const ImVec2& uv1, ImU32 color)
{
    if (pDrawList == nullptr)
    {
        return;
    }

    const ImTextureID textureId = QcImGuiTextureIdFor(pTexture);
    if (textureId == 0)
    {
        return;
    }

    pDrawList->AddImage(textureId, min, max, uv0, uv1, color);
}

void QcImGuiImageRect(const Texture2D* pTexture, int width, int height, Rectangle sourceRect)
{
    const ImTextureID textureId = QcImGuiTextureIdFor(pTexture);
    if (textureId == 0)
    {
        return;
    }

    const float texWidth = static_cast<float>(pTexture->width);
    const float texHeight = static_cast<float>(pTexture->height);

    const float srcW = fabsf(sourceRect.width);
    const float srcH = fabsf(sourceRect.height);

    float u0 = sourceRect.x / texWidth;
    float v0 = sourceRect.y / texHeight;
    float u1 = (sourceRect.x + srcW) / texWidth;
    float v1 = (sourceRect.y + srcH) / texHeight;

    if (sourceRect.width < 0.0f)
    {
        std::swap(u0, u1);
    }
    if (sourceRect.height < 0.0f)
    {
        std::swap(v0, v1);
    }

    ImGui::Image(
        textureId,
        ImVec2(static_cast<float>(width), static_cast<float>(height)),
        ImVec2(u0, v0),
        ImVec2(u1, v1)
    );
}
