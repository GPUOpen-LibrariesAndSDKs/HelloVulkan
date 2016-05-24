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

#ifndef AMD_VULKAN_SAMPLE_H_
#define AMD_VULKAN_SAMPLE_H_

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include <memory>

namespace AMD
{
class Window;

///////////////////////////////////////////////////////////////////////////////
class VulkanSample
{
public:
    VulkanSample(const VulkanSample&) = delete;
    VulkanSample& operator= (const VulkanSample&) = delete;

    VulkanSample();
    virtual ~VulkanSample();

    bool IsInitialized() { return (instance_ != VK_NULL_HANDLE && device_ != VK_NULL_HANDLE); }
    void Run(const int frameCount);
    struct ImportTable;

protected:
    int GetQueueSlot() const
    {
        return currentBackBuffer_;
    }

    static const int QUEUE_SLOT_COUNT = 3;

    static int GetQueueSlotCount()
    {
        return QUEUE_SLOT_COUNT;
    }

    VkViewport viewport_;

    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkInstance instance_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkQueue queue_ = VK_NULL_HANDLE;

    VkSurfaceKHR surface_ = VK_NULL_HANDLE;

    std::unique_ptr<ImportTable> importTable_;

    VkFence frameFences_[QUEUE_SLOT_COUNT];
    VkImage swapchainImages_[QUEUE_SLOT_COUNT];
    VkImageView swapChainImageViews_[QUEUE_SLOT_COUNT];
    VkFramebuffer framebuffer_[QUEUE_SLOT_COUNT];

    VkRenderPass renderPass_ = VK_NULL_HANDLE;

    int queueFamilyIndex_ = -1;

    std::unique_ptr<Window> window_;

    virtual void InitializeImpl(VkCommandBuffer commandBuffer);
    virtual void RenderImpl(VkCommandBuffer commandBuffer);
    virtual void ShutdownImpl();

private:
    VkCommandPool commandPool_;
    VkCommandBuffer commandBuffers_[QUEUE_SLOT_COUNT];
    VkCommandBuffer setupCommandBuffer_;
    uint32_t currentBackBuffer_ = 0;

#ifdef _DEBUG
    VkDebugReportCallbackEXT debugCallback_;
#endif
};

}   // namespace AMD

#endif
