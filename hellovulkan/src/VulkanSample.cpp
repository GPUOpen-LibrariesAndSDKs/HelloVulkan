//
// Copyright (c) 2016 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#include "VulkanSample.h"

#include <iostream>
#include <algorithm>

#include "Utility.h"
#include "Window.h"

#include <cassert>

#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif

#pragma warning( disable : 4100 ) // disable unreferenced formal parameter warnings

namespace AMD
{
struct VulkanSample::ImportTable
{
#define GET_INSTANCE_ENTRYPOINT(i, w) w = reinterpret_cast<PFN_##w>(vkGetInstanceProcAddr(i, #w))
#define GET_DEVICE_ENTRYPOINT(i, w) w = reinterpret_cast<PFN_##w>(vkGetDeviceProcAddr(i, #w))

    ImportTable() = default;

    ImportTable(VkInstance instance, VkDevice device)
    {
#ifdef _DEBUG
        GET_INSTANCE_ENTRYPOINT(instance, vkCreateDebugReportCallbackEXT);
        GET_INSTANCE_ENTRYPOINT(instance, vkDebugReportMessageEXT);
        GET_INSTANCE_ENTRYPOINT(instance, vkDestroyDebugReportCallbackEXT);
#endif
    }

#undef GET_INSTANCE_ENTRYPOINT
#undef GET_DEVICE_ENTRYPOINT

#ifdef _DEBUG
    PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT = nullptr;
    PFN_vkDebugReportMessageEXT vkDebugReportMessageEXT = nullptr;
    PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT = nullptr;
#endif
};

namespace
{
///////////////////////////////////////////////////////////////////////////////
VKAPI_ATTR VkBool32 VKAPI_CALL DebugReportCallback(
    VkDebugReportFlagsEXT       /*flags*/,
    VkDebugReportObjectTypeEXT  /*objectType*/,
    uint64_t                    /*object*/,
    size_t                      /*location*/,
    int32_t                     /*messageCode*/,
    const char*                 /*pLayerPrefix*/,
    const char*                 pMessage,
    void*                       /*pUserData*/)
{
    OutputDebugStringA(pMessage);
    OutputDebugStringA("\n");
    return VK_FALSE;
}

///////////////////////////////////////////////////////////////////////////////
std::vector<const char*> GetDebugInstanceLayerNames()
{
    uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount,
        nullptr);

    std::vector<VkLayerProperties> instanceLayers{ layerCount };
    vkEnumerateInstanceLayerProperties(&layerCount,
        instanceLayers.data());

    std::vector<const char*> result;
    for (const auto& p : instanceLayers)
    {
        if (strcmp(p.layerName, "VK_LAYER_LUNARG_standard_validation") == 0)
        {
            result.push_back("VK_LAYER_LUNARG_standard_validation");
        }
    }

    return result;
}

///////////////////////////////////////////////////////////////////////////////
std::vector<const char*> GetDebugInstanceExtensionNames()
{
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount,
        nullptr);

    std::vector<VkExtensionProperties> instanceExtensions{ extensionCount };
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount,
        instanceExtensions.data());

    std::vector<const char*> result;
    for (const auto& e : instanceExtensions)
    {
        if (strcmp(e.extensionName, "VK_EXT_debug_report") == 0)
        {
            result.push_back("VK_EXT_debug_report");
        }
    }

    return result;
}

///////////////////////////////////////////////////////////////////////////////
std::vector<const char*> GetDebugDeviceLayerNames(VkPhysicalDevice device)
{
    uint32_t layerCount = 0;
    vkEnumerateDeviceLayerProperties(device, &layerCount, nullptr);

    std::vector<VkLayerProperties> deviceLayers{ layerCount };
    vkEnumerateDeviceLayerProperties(device, &layerCount, deviceLayers.data());

    std::vector<const char*> result;
    for (const auto& p : deviceLayers)
    {
        if (strcmp(p.layerName, "VK_LAYER_LUNARG_standard_validation") == 0)
        {
            result.push_back("VK_LAYER_LUNARG_standard_validation");
        }
    }

    return result;
}

///////////////////////////////////////////////////////////////////////////////
void FindPhysicalDeviceWithGraphicsQueue(const std::vector<VkPhysicalDevice>& physicalDevices,
    VkPhysicalDevice* outputDevice, int* outputGraphicsQueueIndex)
{
    for (auto physicalDevice : physicalDevices)
    {
        uint32_t queueFamilyPropertyCount = 0;

        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice,
            &queueFamilyPropertyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilyProperties{ queueFamilyPropertyCount };
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice,
            &queueFamilyPropertyCount, queueFamilyProperties.data());

        int i = 0;
        for (const auto& queueFamilyProperty : queueFamilyProperties)
        {
            if (queueFamilyProperty.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                if (outputDevice)
                {
                    *outputDevice = physicalDevice;
                }

                if (outputGraphicsQueueIndex)
                {
                    *outputGraphicsQueueIndex = i;
                }

                return;
            }

            ++i;
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
VkInstance CreateInstance()
{
    VkInstanceCreateInfo instanceCreateInfo = {};
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

    std::vector<const char*> instanceExtensions =
    {
        "VK_KHR_surface", "VK_KHR_win32_surface"
    };

#ifdef _DEBUG
    auto debugInstanceExtensionNames = GetDebugInstanceExtensionNames();
    instanceExtensions.insert(instanceExtensions.end(),
        debugInstanceExtensionNames.begin(), debugInstanceExtensionNames.end());
#endif

    instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();
    instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t> (instanceExtensions.size());

    std::vector<const char*> instanceLayers;

#ifdef _DEBUG
    auto debugInstanceLayerNames = GetDebugInstanceLayerNames();
    instanceLayers.insert(instanceLayers.end(),
        debugInstanceLayerNames.begin(), debugInstanceLayerNames.end());
#endif

    instanceCreateInfo.ppEnabledLayerNames = instanceLayers.data();
    instanceCreateInfo.enabledLayerCount = static_cast<uint32_t> (instanceLayers.size());

    VkApplicationInfo applicationInfo = {};
    applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    applicationInfo.apiVersion = VK_API_VERSION_1_0;
    applicationInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    applicationInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    applicationInfo.pApplicationName = "AMD Vulkan Sample application";
    applicationInfo.pEngineName = "AMD Vulkan Sample Engine";

    instanceCreateInfo.pApplicationInfo = &applicationInfo;

    VkInstance instance = VK_NULL_HANDLE;
    vkCreateInstance(&instanceCreateInfo, nullptr, &instance);

    return instance;
}

///////////////////////////////////////////////////////////////////////////////
void CreateDeviceAndQueue(VkInstance instance, VkDevice* outputDevice,
    VkQueue* outputQueue, int* outputQueueIndex,
    VkPhysicalDevice* outputPhysicalDevice)
{
    uint32_t physicalDeviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr);

    std::vector<VkPhysicalDevice> devices{ physicalDeviceCount };
    vkEnumeratePhysicalDevices(instance, &physicalDeviceCount,
        devices.data());

    VkPhysicalDevice physicalDevice = nullptr;
    int graphicsQueueIndex = -1;

    FindPhysicalDeviceWithGraphicsQueue(devices, &physicalDevice, &graphicsQueueIndex);

    assert(physicalDevice);

    VkDeviceQueueCreateInfo deviceQueueCreateInfo = {};
    deviceQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    deviceQueueCreateInfo.queueCount = 1;
    deviceQueueCreateInfo.queueFamilyIndex = graphicsQueueIndex;

    static const float queuePriorities[] = { 1.0f };
    deviceQueueCreateInfo.pQueuePriorities = queuePriorities;

    VkDeviceCreateInfo deviceCreateInfo = {};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &deviceQueueCreateInfo;

    std::vector<const char*> deviceLayers;

#ifdef _DEBUG
    auto debugDeviceLayerNames = GetDebugDeviceLayerNames(physicalDevice);
    deviceLayers.insert(deviceLayers.end(),
        debugDeviceLayerNames.begin(), debugDeviceLayerNames.end());
#endif

    deviceCreateInfo.ppEnabledLayerNames = deviceLayers.data();
    deviceCreateInfo.enabledLayerCount = static_cast<uint32_t> (deviceLayers.size());

	// Our SPIR-V requests this, so we need to enable it here
	VkPhysicalDeviceFeatures enabledFeatures = {};
	enabledFeatures.shaderClipDistance = true;

	deviceCreateInfo.pEnabledFeatures = &enabledFeatures;

    std::vector<const char*> deviceExtensions =
    {
        "VK_KHR_swapchain"
    };

    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
    deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t> (deviceExtensions.size());

    VkDevice device = nullptr;
    vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device);
    assert(device);

    VkQueue queue = nullptr;
    vkGetDeviceQueue(device, graphicsQueueIndex, 0, &queue);
    assert(queue);

    if (outputQueue)
    {
        *outputQueue = queue;
    }

    if (outputDevice)
    {
        *outputDevice = device;
    }

    if (outputQueueIndex)
    {
        *outputQueueIndex = graphicsQueueIndex;
    }

    if (outputPhysicalDevice)
    {
        *outputPhysicalDevice = physicalDevice;
    }
}

struct SwapchainFormatColorSpace
{
    VkFormat format;
    VkColorSpaceKHR colorSpace;
};

///////////////////////////////////////////////////////////////////////////////
SwapchainFormatColorSpace GetSwapchainFormatAndColorspace(VkPhysicalDevice physicalDevice,
    VkSurfaceKHR surface, VulkanSample::ImportTable* importTable)
{
    uint32_t surfaceFormatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice,
        surface, &surfaceFormatCount, nullptr);

    std::vector<VkSurfaceFormatKHR> surfaceFormats{ surfaceFormatCount };
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice,
        surface, &surfaceFormatCount, surfaceFormats.data());

    SwapchainFormatColorSpace result;

    if (surfaceFormatCount == 1 && surfaceFormats.front().format == VK_FORMAT_UNDEFINED)
    {
        result.format = VK_FORMAT_R8G8B8A8_UNORM;
    }
    else
    {
        result.format = surfaceFormats.front().format;
    }

    result.colorSpace = surfaceFormats.front().colorSpace;

    return result;
}

///////////////////////////////////////////////////////////////////////////////
VkRenderPass CreateRenderPass(VkDevice device, VkFormat swapchainFormat)
{
    VkAttachmentDescription attachmentDescription = {};
    attachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
    attachmentDescription.format = swapchainFormat;
    attachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachmentDescription.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    attachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    VkAttachmentReference attachmentReference = {};
    attachmentReference.attachment = 0;
    attachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpassDescription = {};
    subpassDescription.inputAttachmentCount = 0;
    subpassDescription.pColorAttachments = &attachmentReference;
    subpassDescription.colorAttachmentCount = 1;
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

    VkRenderPassCreateInfo renderPassCreateInfo = {};
    renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassCreateInfo.attachmentCount = 1;
    renderPassCreateInfo.subpassCount = 1;
    renderPassCreateInfo.pSubpasses = &subpassDescription;
    renderPassCreateInfo.pAttachments = &attachmentDescription;

    VkRenderPass result = nullptr;
    vkCreateRenderPass(device, &renderPassCreateInfo, nullptr,
        &result);

    return result;
}

///////////////////////////////////////////////////////////////////////////////
void CreateFramebuffers(VkDevice device, VkRenderPass renderPass,
    const int width, const int height,
    const int count, const VkImageView* imageViews, VkFramebuffer* framebuffers)
{
    for (int i = 0; i < count; ++i)
    {
        VkFramebufferCreateInfo framebufferCreateInfo = {};
        framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferCreateInfo.attachmentCount = 1;
        framebufferCreateInfo.pAttachments = &imageViews[i];
        framebufferCreateInfo.height = height;
        framebufferCreateInfo.width = width;
        framebufferCreateInfo.layers = 1;
        framebufferCreateInfo.renderPass = renderPass;

        vkCreateFramebuffer(device, &framebufferCreateInfo, nullptr,
            &framebuffers[i]);
    }
}

///////////////////////////////////////////////////////////////////////////////
void CreateSwapchainImageViews(VkDevice device, VkFormat format,
    const int count, const VkImage* images, VkImageView* imageViews)
{
    for (int i = 0; i < count; ++i)
    {
        VkImageViewCreateInfo imageViewCreateInfo = {};
        imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewCreateInfo.image = images[i];
        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageViewCreateInfo.format = format;
        imageViewCreateInfo.subresourceRange.levelCount = 1;
        imageViewCreateInfo.subresourceRange.layerCount = 1;
        imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

        vkCreateImageView(device, &imageViewCreateInfo, nullptr,
            &imageViews[i]);
    }
}

///////////////////////////////////////////////////////////////////////////////
VkSwapchainKHR CreateSwapchain(VkPhysicalDevice physicalDevice, VkDevice device,
    VkSurfaceKHR surface, const int surfaceWidth, const int surfaceHeight,
    const int backbufferCount, VulkanSample::ImportTable* importTable,
    VkFormat* swapchainFormat)
{
    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice,
        surface, &surfaceCapabilities);

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice,
        surface, &presentModeCount, nullptr);

    std::vector<VkPresentModeKHR> presentModes{ presentModeCount };

    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice,
        surface, &presentModeCount, presentModes.data());

    VkExtent2D swapChainSize = {};
    swapChainSize = surfaceCapabilities.currentExtent;
    assert(static_cast<int> (swapChainSize.width) == surfaceWidth);
    assert(static_cast<int> (swapChainSize.height) == surfaceHeight);

    uint32_t swapChainImageCount = backbufferCount;
    assert(swapChainImageCount >= surfaceCapabilities.minImageCount);

    // 0 indicates unlimited number of images
    if (surfaceCapabilities.maxImageCount != 0)
    {
        assert(swapChainImageCount < surfaceCapabilities.maxImageCount);
    }

    VkSurfaceTransformFlagBitsKHR surfaceTransformFlags;

    if (surfaceCapabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
    {
        surfaceTransformFlags = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    }
    else
    {
        surfaceTransformFlags = surfaceCapabilities.currentTransform;
    }

    auto swapchainFormatColorSpace = GetSwapchainFormatAndColorspace(physicalDevice, surface,
        importTable);

    VkSwapchainCreateInfoKHR swapchainCreateInfo = {};
    swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainCreateInfo.surface = surface;
    swapchainCreateInfo.minImageCount = swapChainImageCount;
    swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchainCreateInfo.preTransform = surfaceTransformFlags;
    swapchainCreateInfo.imageColorSpace = swapchainFormatColorSpace.colorSpace;
    swapchainCreateInfo.imageFormat = swapchainFormatColorSpace.format;
    swapchainCreateInfo.pQueueFamilyIndices = nullptr;
    swapchainCreateInfo.queueFamilyIndexCount = 0;
    swapchainCreateInfo.clipped = VK_TRUE;
    swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;
    swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainCreateInfo.imageExtent = swapChainSize;
    swapchainCreateInfo.imageArrayLayers = 1;
    swapchainCreateInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;

    VkSwapchainKHR swapchain;
    vkCreateSwapchainKHR(device, &swapchainCreateInfo,
        nullptr, &swapchain);

    if (swapchainFormat)
    {
        *swapchainFormat = swapchainFormatColorSpace.format;
    }

    return swapchain;
}

///////////////////////////////////////////////////////////////////////////////
VkSurfaceKHR CreateSurface(VkInstance instance, HWND hwnd)
{
    VkWin32SurfaceCreateInfoKHR win32surfaceCreateInfo = {};
    win32surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    win32surfaceCreateInfo.hwnd = hwnd;
    win32surfaceCreateInfo.hinstance = ::GetModuleHandle(nullptr);

    VkSurfaceKHR surface = nullptr;
    vkCreateWin32SurfaceKHR(instance, &win32surfaceCreateInfo, nullptr,
        &surface);

    return surface;
}

#ifdef _DEBUG
///////////////////////////////////////////////////////////////////////////////
VkDebugReportCallbackEXT SetupDebugCallback(VkInstance instance, VulkanSample::ImportTable* importTable)
{
    if (importTable->vkCreateDebugReportCallbackEXT)
    {
        VkDebugReportCallbackCreateInfoEXT callbackCreateInfo = {};
        callbackCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
        callbackCreateInfo.flags =
            VK_DEBUG_REPORT_ERROR_BIT_EXT |
            VK_DEBUG_REPORT_WARNING_BIT_EXT |
            VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
        callbackCreateInfo.pfnCallback = &DebugReportCallback;

        VkDebugReportCallbackEXT callback;
        importTable->vkCreateDebugReportCallbackEXT(instance, &callbackCreateInfo, nullptr, &callback);
        return callback;
    }
    else
    {
        return VK_NULL_HANDLE;
    }
}

///////////////////////////////////////////////////////////////////////////////
void CleanupDebugCallback(VkInstance instance, VkDebugReportCallbackEXT callback,
    VulkanSample::ImportTable* importTable)
{
    if (importTable->vkDestroyDebugReportCallbackEXT)
    {
        importTable->vkDestroyDebugReportCallbackEXT(instance, callback, nullptr);
    }
}
#endif

}   // namespace

///////////////////////////////////////////////////////////////////////////////
VulkanSample::VulkanSample()
{
    instance_ = CreateInstance();
    if (instance_ == VK_NULL_HANDLE)
    {
        // just bail out if the user does not have a compatible Vulkan driver
        return;
    }

    VkPhysicalDevice physicalDevice;
    CreateDeviceAndQueue(instance_, &device_, &queue_, &queueFamilyIndex_,
        &physicalDevice);
    physicalDevice_ = physicalDevice;

    importTable_.reset(new ImportTable{ instance_, device_ });

#ifdef _DEBUG
    debugCallback_ = SetupDebugCallback(instance_, importTable_.get());
#endif

    window_.reset(new Window{ "Hello Vulkan", 1280, 720 });

    surface_ = CreateSurface(instance_, window_->GetHWND());

    VkBool32 presentSupported;
    vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice,
        0, surface_, &presentSupported);
    assert(presentSupported);

    VkFormat swapchainFormat = VK_FORMAT_UNDEFINED;
    swapchain_ = CreateSwapchain(physicalDevice, device_, surface_,
        window_->GetWidth(), window_->GetHeight(), QUEUE_SLOT_COUNT,
        importTable_.get(), &swapchainFormat);

    assert(swapchain_);

    uint32_t swapchainImageCount = 0;
    vkGetSwapchainImagesKHR(device_, swapchain_,
        &swapchainImageCount, nullptr);
    assert(static_cast<int> (swapchainImageCount) == QUEUE_SLOT_COUNT);

    vkGetSwapchainImagesKHR(device_, swapchain_,
        &swapchainImageCount, swapchainImages_);

    renderPass_ = CreateRenderPass(device_, swapchainFormat);

    CreateSwapchainImageViews(device_, swapchainFormat,
        QUEUE_SLOT_COUNT, swapchainImages_, swapChainImageViews_);
    CreateFramebuffers(device_, renderPass_, window_->GetWidth(), window_->GetHeight(),
        QUEUE_SLOT_COUNT, swapChainImageViews_, framebuffer_);

    VkCommandPoolCreateInfo commandPoolCreateInfo = {};
    commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolCreateInfo.queueFamilyIndex = queueFamilyIndex_;
    commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    vkCreateCommandPool(device_, &commandPoolCreateInfo, nullptr,
        &commandPool_);

    VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.commandBufferCount = QUEUE_SLOT_COUNT + 1;
    commandBufferAllocateInfo.commandPool = commandPool_;
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    VkCommandBuffer commandBuffers[QUEUE_SLOT_COUNT + 1];

    vkAllocateCommandBuffers(device_, &commandBufferAllocateInfo,
        commandBuffers);

    for (int i = 0; i < QUEUE_SLOT_COUNT; ++i)
    {
        commandBuffers_[i] = commandBuffers[i];
    }

    setupCommandBuffer_ = commandBuffers[QUEUE_SLOT_COUNT];

    for (int i = 0; i < QUEUE_SLOT_COUNT; ++i)
    {
        VkFenceCreateInfo fenceCreateInfo = {};
        fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        // We need this so we can wait for them on the first try
        fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        vkCreateFence(device_, &fenceCreateInfo, nullptr, &frameFences_[i]);
    }
}

///////////////////////////////////////////////////////////////////////////////
VulkanSample::~VulkanSample()
{
}

///////////////////////////////////////////////////////////////////////////////
void VulkanSample::ShutdownImpl()
{
    for (int i = 0; i < QUEUE_SLOT_COUNT; ++i)
    {
        vkDestroyFence(device_, frameFences_[i], nullptr);
    }

    vkDestroyRenderPass(device_, renderPass_, nullptr);

    for (int i = 0; i < QUEUE_SLOT_COUNT; ++i)
    {
        vkDestroyFramebuffer(device_, framebuffer_[i], nullptr);
        vkDestroyImageView(device_, swapChainImageViews_[i], nullptr);
    }

    vkDestroyCommandPool(device_, commandPool_, nullptr);

    vkDestroySwapchainKHR(device_, swapchain_, nullptr);
    vkDestroySurfaceKHR(instance_, surface_, nullptr);

#ifdef _DEBUG
    CleanupDebugCallback(instance_, debugCallback_, importTable_.get());
#endif

    vkDestroyDevice(device_, nullptr);
    vkDestroyInstance(instance_, nullptr);
}

///////////////////////////////////////////////////////////////////////////////
void VulkanSample::Run(const int frameCount)
{
    if (IsInitialized() == false)
    {
        // just bail out if the user does not have a compatible Vulkan driver
        return;
    }

    vkResetFences(device_, 1, &frameFences_[0]);
    {
        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        vkBeginCommandBuffer(setupCommandBuffer_, &beginInfo);

        InitializeImpl(setupCommandBuffer_);

        vkEndCommandBuffer(setupCommandBuffer_);

        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &setupCommandBuffer_;
        vkQueueSubmit(queue_, 1, &submitInfo, frameFences_[0]);
    }

    vkWaitForFences(device_, 1, &frameFences_[0], VK_TRUE, UINT64_MAX);

    VkSemaphoreCreateInfo semaphoreCreateInfo = {};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkSemaphore imageAcquiredSemaphore;
    vkCreateSemaphore(device_, &semaphoreCreateInfo,
        nullptr, &imageAcquiredSemaphore);

    VkSemaphore renderingCompleteSemaphore;
    vkCreateSemaphore(device_, &semaphoreCreateInfo,
        nullptr, &renderingCompleteSemaphore);

    for (int i = 0; i < frameCount; ++i)
    {
        vkAcquireNextImageKHR(
            device_, swapchain_, UINT64_MAX, imageAcquiredSemaphore,
            VK_NULL_HANDLE, &currentBackBuffer_);

        vkWaitForFences(device_, 1, &frameFences_[currentBackBuffer_], VK_TRUE,
            UINT64_MAX);
        vkResetFences(device_, 1, &frameFences_[currentBackBuffer_]);

        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        
        vkBeginCommandBuffer(commandBuffers_[currentBackBuffer_], &beginInfo);

        VkRenderPassBeginInfo renderPassBeginInfo = {};
        renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassBeginInfo.framebuffer = framebuffer_[currentBackBuffer_];
        renderPassBeginInfo.renderArea.extent.width = window_->GetWidth();
        renderPassBeginInfo.renderArea.extent.height = window_->GetHeight();
        renderPassBeginInfo.renderPass = renderPass_;

        VkClearValue clearValue = {};

        clearValue.color.float32[0] = 0.042f;
        clearValue.color.float32[1] = 0.042f;
        clearValue.color.float32[2] = 0.042f;
        clearValue.color.float32[3] = 1.0f;

        renderPassBeginInfo.pClearValues = &clearValue;
        renderPassBeginInfo.clearValueCount = 1;

        vkCmdBeginRenderPass(commandBuffers_[currentBackBuffer_],
            &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        RenderImpl(commandBuffers_[currentBackBuffer_]);

        vkCmdEndRenderPass(commandBuffers_[currentBackBuffer_]);
        vkEndCommandBuffer(commandBuffers_[currentBackBuffer_]);

        // Submit rendering work to the graphics queue
        const VkPipelineStageFlags waitDstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &imageAcquiredSemaphore;
        submitInfo.pWaitDstStageMask = &waitDstStageMask;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffers_[currentBackBuffer_];
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &renderingCompleteSemaphore;
        vkQueueSubmit(queue_, 1, &submitInfo, VK_NULL_HANDLE);

        // Submit present operation to present queue
        VkPresentInfoKHR presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &renderingCompleteSemaphore;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &swapchain_;
        presentInfo.pImageIndices = &currentBackBuffer_;
        vkQueuePresentKHR(queue_, &presentInfo);

        vkQueueSubmit(queue_, 0, nullptr, frameFences_[currentBackBuffer_]);
    };

    // Wait for all rendering to finish
    vkWaitForFences(device_, 3, frameFences_, VK_TRUE, UINT64_MAX);

    vkDestroySemaphore(device_, imageAcquiredSemaphore, nullptr);
    vkDestroySemaphore(device_, renderingCompleteSemaphore, nullptr);

    ShutdownImpl();
}

///////////////////////////////////////////////////////////////////////////////
void VulkanSample::InitializeImpl(VkCommandBuffer /*commandBuffer*/)
{
}

///////////////////////////////////////////////////////////////////////////////
void VulkanSample::RenderImpl(VkCommandBuffer /*commandBuffer*/)
{
}

}   // namespace AMD
