#include <stdexcept>
#include <vector>
#include <array>
#include <iostream>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

#include "../../engine/ui/editor/EditorUI.hpp"
#if defined(__APPLE__)
// MoltenVK on macOS requires Metal surface + portability extensions
#ifndef VK_USE_PLATFORM_METAL_EXT
#define VK_USE_PLATFORM_METAL_EXT
#endif
#endif

using namespace dancore::ui;

static void VK_CHECK(VkResult r, const char* where){ if(r!=VK_SUCCESS){ std::cerr<<"Vulkan error "<<r<<" at "<<where<<"\n"; throw std::runtime_error("Vulkan error"); }}

 // --- globals (bootstrap-only) ---
GLFWwindow* gWin{};
VkInstance gInst{};
VkSurfaceKHR gSurf{};
VkPhysicalDevice gGPU{};
uint32_t gQFam{};
VkDevice gDev{};
VkQueue gQ{};
VkSwapchainKHR gSwap{};
VkFormat gFmt = VK_FORMAT_B8G8R8A8_UNORM;
VkColorSpaceKHR gCS = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
VkExtent2D gExt{};
std::vector<VkImage> gImgs;
std::vector<VkImageView> gViews;
VkRenderPass gRP{};
std::vector<VkFramebuffer> gFBs;
VkCommandPool gPool{};
std::vector<VkCommandBuffer> gCmds;
VkSemaphore gSemImg{}, gSemDraw{};
VkFence gFence{};
VkDescriptorPool gImGuiPool{};

static void CreateInstance(){
    {
        VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
        app.pApplicationName = "Dancore Editor";
        app.apiVersion = VK_API_VERSION_1_2;

        uint32_t n = 0;
        const char** ex = glfwGetRequiredInstanceExtensions(&n);
        std::vector<const char*> exts(ex, ex + n);

#if defined(__APPLE__)
        // Required for MoltenVK portability on macOS
        exts.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
#endif

        VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        ci.pApplicationInfo = &app;
        ci.enabledExtensionCount = static_cast<uint32_t>(exts.size());
        ci.ppEnabledExtensionNames = exts.data();

#if defined(__APPLE__)
        // Allow vkEnumeratePhysicalDevices to return portability devices
        ci.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

        VK_CHECK(vkCreateInstance(&ci, nullptr, &gInst), "vkCreateInstance");
    }
}
static void CreateSurface(){
    if(glfwCreateWindowSurface(gInst,gWin,nullptr,&gSurf)!=VK_SUCCESS) throw std::runtime_error("glfwCreateWindowSurface");
}
static void PickGPU(){
    uint32_t c=0; vkEnumeratePhysicalDevices(gInst,&c,nullptr); if(!c) throw std::runtime_error("No GPU");
    std::vector<VkPhysicalDevice> d(c); vkEnumeratePhysicalDevices(gInst,&c,d.data());
    for(auto dev: d){
        uint32_t qn=0; vkGetPhysicalDeviceQueueFamilyProperties(dev,&qn,nullptr);
        std::vector<VkQueueFamilyProperties> qp(qn); vkGetPhysicalDeviceQueueFamilyProperties(dev,&qn,qp.data());
        for(uint32_t i=0;i<qn;i++){
            VkBool32 present=VK_FALSE; vkGetPhysicalDeviceSurfaceSupportKHR(dev,i,gSurf,&present);
            if((qp[i].queueFlags&VK_QUEUE_GRAPHICS_BIT) && present){ gGPU=dev; gQFam=i; return; }
        }
    }
    throw std::runtime_error("No suitable GPU");
}
static void CreateDevice(){
    {
        float pr = 1.f;
        VkDeviceQueueCreateInfo q{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        q.queueFamilyIndex = gQFam;
        q.queueCount = 1;
        q.pQueuePriorities = &pr;

        std::vector<const char*> devExts = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
#if defined(__APPLE__)
        // MoltenVK portability subset extension
        devExts.push_back("VK_KHR_portability_subset");
#endif

        VkDeviceCreateInfo ci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        ci.queueCreateInfoCount = 1;
        ci.pQueueCreateInfos = &q;
        ci.enabledExtensionCount = static_cast<uint32_t>(devExts.size());
        ci.ppEnabledExtensionNames = devExts.data();

        VK_CHECK(vkCreateDevice(gGPU, &ci, nullptr, &gDev), "vkCreateDevice");
        vkGetDeviceQueue(gDev, gQFam, 0, &gQ);
    }
}
static void CreateSwapchain(int w,int h){
    gExt={ (uint32_t)w,(uint32_t)h };
    VkSwapchainCreateInfoKHR ci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    ci.surface=gSurf; ci.minImageCount=2; ci.imageFormat=gFmt; ci.imageColorSpace=gCS; ci.imageExtent=gExt;
    ci.imageArrayLayers=1; ci.imageUsage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; ci.imageSharingMode=VK_SHARING_MODE_EXCLUSIVE;
    ci.preTransform=VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR; ci.compositeAlpha=VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode=VK_PRESENT_MODE_FIFO_KHR; ci.clipped=VK_TRUE;
    VK_CHECK(vkCreateSwapchainKHR(gDev,&ci,nullptr,&gSwap),"vkCreateSwapchainKHR");
    uint32_t n=0; vkGetSwapchainImagesKHR(gDev,gSwap,&n,nullptr); gImgs.resize(n);
    vkGetSwapchainImagesKHR(gDev,gSwap,&n,gImgs.data());
    gViews.resize(n);
    for(uint32_t i=0;i<n;i++){
        VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        vi.image=gImgs[i]; vi.viewType=VK_IMAGE_VIEW_TYPE_2D; vi.format=gFmt;
        vi.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT; vi.subresourceRange.levelCount=1; vi.subresourceRange.layerCount=1;
        VK_CHECK(vkCreateImageView(gDev,&vi,nullptr,&gViews[i]),"vkCreateImageView");
    }
}
static void CreateRenderPass(){
    VkAttachmentDescription col{}; col.format=gFmt; col.samples=VK_SAMPLE_COUNT_1_BIT;
    col.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR; col.storeOp=VK_ATTACHMENT_STORE_OP_STORE;
    col.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED; col.finalLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    VkAttachmentReference cref{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription sub{}; sub.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS; sub.colorAttachmentCount=1; sub.pColorAttachments=&cref;
    VkRenderPassCreateInfo ci{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO}; ci.attachmentCount=1; ci.pAttachments=&col; ci.subpassCount=1; ci.pSubpasses=&sub;
    VK_CHECK(vkCreateRenderPass(gDev,&ci,nullptr,&gRP),"vkCreateRenderPass");
}
static void CreateFramebuffers(){
    gFBs.resize(gViews.size());
    for(size_t i=0;i<gViews.size();++i){
        VkImageView atts[]{ gViews[i] };
        VkFramebufferCreateInfo ci{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        ci.renderPass=gRP; ci.attachmentCount=1; ci.pAttachments=atts; ci.width=gExt.width; ci.height=gExt.height; ci.layers=1;
        VK_CHECK(vkCreateFramebuffer(gDev,&ci,nullptr,&gFBs[i]),"vkCreateFramebuffer");
    }
}
static void CreateCommandsSync(){
    VkCommandPoolCreateInfo pi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pi.queueFamilyIndex=gQFam; pi.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    VK_CHECK(vkCreateCommandPool(gDev,&pi,nullptr,&gPool),"vkCreateCommandPool");
    gCmds.resize(gFBs.size());
    VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    ai.commandPool=gPool; ai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY; ai.commandBufferCount=(uint32_t)gCmds.size();
    VK_CHECK(vkAllocateCommandBuffers(gDev,&ai,gCmds.data()),"vkAllocateCommandBuffers");
    VkSemaphoreCreateInfo si{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VK_CHECK(vkCreateSemaphore(gDev,&si,nullptr,&gSemImg),"vkCreateSemaphore");
    VK_CHECK(vkCreateSemaphore(gDev,&si,nullptr,&gSemDraw),"vkCreateSemaphore");
    VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO}; fi.flags=VK_FENCE_CREATE_SIGNALED_BIT;
    VK_CHECK(vkCreateFence(gDev,&fi,nullptr,&gFence),"vkCreateFence");
}
static void CreateImGuiPool(){
    std::array<VkDescriptorPoolSize,11> sizes = {
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLER,1000},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1000},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,1000},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1000},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,1000},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,1000},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1000},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1000},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,1000},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,1000},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,1000}
    };
    VkDescriptorPoolCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    ci.flags=VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    ci.maxSets=1000*(uint32_t)sizes.size();
    ci.poolSizeCount=(uint32_t)sizes.size(); ci.pPoolSizes=sizes.data();
    VK_CHECK(vkCreateDescriptorPool(gDev,&ci,nullptr,&gImGuiPool),"vkCreateDescriptorPool");
}
static void InitImGui(){
    IMGUI_CHECKVERSION(); ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForVulkan(gWin,true);
    ImGui_ImplVulkan_InitInfo ii{};
    ii.Instance=gInst; ii.PhysicalDevice=gGPU; ii.Device=gDev; ii.QueueFamily=gQFam; ii.Queue=gQ;
    ii.DescriptorPool=gImGuiPool; ii.MinImageCount=(uint32_t)gImgs.size(); ii.ImageCount=(uint32_t)gImgs.size();
    ii.MSAASamples=VK_SAMPLE_COUNT_1_BIT; ii.CheckVkResultFn = [](VkResult r){ VK_CHECK(r,"ImGui"); };
    ImGui_ImplVulkan_Init(&ii, gRP);

    // upload fonts
    VkCommandBuffer cmd;
    VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    ai.commandPool=gPool; ai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY; ai.commandBufferCount=1;
    VK_CHECK(vkAllocateCommandBuffers(gDev,&ai,&cmd),"alloc font cmd");
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd,&bi);
    ImGui_ImplVulkan_CreateFontsTexture(cmd);
    vkEndCommandBuffer(cmd);
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO}; si.commandBufferCount=1; si.pCommandBuffers=&cmd;
    vkQueueSubmit(gQ,1,&si,VK_NULL_HANDLE); vkQueueWaitIdle(gQ);
    ImGui_ImplVulkan_DestroyFontUploadObjects();
    vkFreeCommandBuffers(gDev,gPool,1,&cmd);
}
static void Record(VkCommandBuffer cmd, uint32_t idx){
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    VK_CHECK(vkBeginCommandBuffer(cmd,&bi),"vkBeginCommandBuffer");
    VkClearValue clear{}; clear.color={{0.10f,0.11f,0.12f,1.0f}};
    VkRenderPassBeginInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rp.renderPass=gRP; rp.framebuffer=gFBs[idx]; rp.renderArea.extent=gExt; rp.clearValueCount=1; rp.pClearValues=&clear;
    vkCmdBeginRenderPass(cmd,&rp,VK_SUBPASS_CONTENTS_INLINE);

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    static EditorState state{};
    dancore::ui::DrawEditorUI(state);

    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

    vkCmdEndRenderPass(cmd);
    VK_CHECK(vkEndCommandBuffer(cmd),"vkEndCommandBuffer");
}
static void Cleanup(){
    vkDeviceWaitIdle(gDev);
    ImGui_ImplVulkan_Shutdown(); ImGui_ImplGlfw_Shutdown(); ImGui::DestroyContext();
    vkDestroyDescriptorPool(gDev,gImGuiPool,nullptr);
    vkDestroyFence(gDev,gFence,nullptr);
    vkDestroySemaphore(gDev,gSemDraw,nullptr); vkDestroySemaphore(gDev,gSemImg,nullptr);
    for(auto fb:gFBs) vkDestroyFramebuffer(gDev,fb,nullptr);
    vkDestroyRenderPass(gDev,gRP,nullptr);
    for(auto v:gViews) vkDestroyImageView(gDev,v,nullptr);
    vkDestroySwapchainKHR(gDev,gSwap,nullptr);
    vkDestroyCommandPool(gDev,gPool,nullptr);
    vkDestroyDevice(gDev,nullptr);
    vkDestroySurfaceKHR(gInst,gSurf,nullptr);
    vkDestroyInstance(gInst,nullptr);
    glfwDestroyWindow(gWin); glfwTerminate();
}

int main(){
    if(!glfwInit()) return -1;
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    gWin = glfwCreateWindow(1280,720,"Dancore Editor (Vulkan)",nullptr,nullptr);

    CreateInstance(); CreateSurface(); PickGPU(); CreateDevice();
    int w,h; glfwGetFramebufferSize(gWin,&w,&h);
    CreateSwapchain(w,h); CreateRenderPass(); CreateFramebuffers(); CreateCommandsSync();
    CreateImGuiPool(); InitImGui();

    while(!glfwWindowShouldClose(gWin)){
        glfwPollEvents();
        vkWaitForFences(gDev,1,&gFence,VK_TRUE,UINT64_MAX);
        vkResetFences(gDev,1,&gFence);
        uint32_t idx=0;
        VK_CHECK(vkAcquireNextImageKHR(gDev,gSwap,UINT64_MAX,gSemImg,VK_NULL_HANDLE,&idx),"acquire");
        VkCommandBuffer cmd=gCmds[idx]; vkResetCommandBuffer(cmd,0); Record(cmd,idx);
        VkPipelineStageFlags wait=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        si.waitSemaphoreCount=1; si.pWaitSemaphores=&gSemImg; si.pWaitDstStageMask=&wait;
        si.commandBufferCount=1; si.pCommandBuffers=&cmd;
        si.signalSemaphoreCount=1; si.pSignalSemaphores=&gSemDraw;
        VK_CHECK(vkQueueSubmit(gQ,1,&si,gFence),"submit");
        VkPresentInfoKHR pi{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        pi.waitSemaphoreCount=1; pi.pWaitSemaphores=&gSemDraw; pi.swapchainCount=1; pi.pSwapchains=&gSwap; pi.pImageIndices=&idx;
        VK_CHECK(vkQueuePresentKHR(gQ,&pi),"present");
    }
    Cleanup();
    return 0;
}
