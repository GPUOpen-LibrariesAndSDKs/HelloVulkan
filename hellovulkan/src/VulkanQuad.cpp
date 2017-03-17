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

#include "VulkanQuad.h"

#include "Shaders.h"
#include "Utility.h"
#include "Window.h"

#include <vector>

namespace AMD {
///////////////////////////////////////////////////////////////////////////////
void VulkanQuad::ShutdownImpl ()
{
    vkDestroyPipeline (device_, pipeline_, nullptr);
    vkDestroyPipelineLayout (device_, pipelineLayout_, nullptr);

    vkDestroyBuffer (device_, vertexBuffer_, nullptr);
    vkDestroyBuffer (device_, indexBuffer_, nullptr);
    vkFreeMemory (device_, deviceMemory_, nullptr);

    vkDestroyShaderModule (device_, vertexShader_, nullptr);
    vkDestroyShaderModule (device_, fragmentShader_, nullptr);

    VulkanSample::ShutdownImpl ();
}

///////////////////////////////////////////////////////////////////////////////
void VulkanQuad::RenderImpl (VkCommandBuffer commandBuffer)
{
    VulkanSample::RenderImpl (commandBuffer);

    vkCmdBindPipeline (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipeline_);
    VkDeviceSize offsets [] = { 0 };
    vkCmdBindIndexBuffer (commandBuffer, indexBuffer_, 0, VK_INDEX_TYPE_UINT32);
    vkCmdBindVertexBuffers (commandBuffer, 0, 1, &vertexBuffer_, offsets);
    vkCmdDrawIndexed (commandBuffer, 6, 1, 0, 0, 0);
}

///////////////////////////////////////////////////////////////////////////////
void VulkanQuad::InitializeImpl (VkCommandBuffer uploadCommandBuffer)
{
    VulkanSample::InitializeImpl (uploadCommandBuffer);

    CreatePipelineStateObject ();
    CreateMeshBuffers (uploadCommandBuffer);
}

namespace {
struct MemoryTypeInfo
{
    bool deviceLocal = false;
    bool hostVisible = false;
    bool hostCoherent = false;
    bool hostCached = false;
    bool lazilyAllocated = false;

    struct Heap
    {
        uint64_t size = 0;
        bool deviceLocal = false;
    };

    Heap heap;
    int index;
};

///////////////////////////////////////////////////////////////////////////////
std::vector<MemoryTypeInfo> EnumerateHeaps (VkPhysicalDevice device)
{
    VkPhysicalDeviceMemoryProperties memoryProperties = {};
    vkGetPhysicalDeviceMemoryProperties (device, &memoryProperties);

    std::vector<MemoryTypeInfo::Heap> heaps;

    for (uint32_t i = 0; i < memoryProperties.memoryHeapCount; ++i)
    {
        MemoryTypeInfo::Heap info;
        info.size = memoryProperties.memoryHeaps [i].size;
        info.deviceLocal = (memoryProperties.memoryHeaps [i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0;

        heaps.push_back (info);
    }

    std::vector<MemoryTypeInfo> result;

    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i)
    {
        MemoryTypeInfo typeInfo;

        typeInfo.deviceLocal = (memoryProperties.memoryTypes [i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0;
        typeInfo.hostVisible = (memoryProperties.memoryTypes [i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0;
        typeInfo.hostCoherent = (memoryProperties.memoryTypes [i].propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0;
        typeInfo.hostCached = (memoryProperties.memoryTypes [i].propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) != 0;
        typeInfo.lazilyAllocated = (memoryProperties.memoryTypes [i].propertyFlags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT) != 0;

        typeInfo.heap = heaps [memoryProperties.memoryTypes [i].heapIndex];

        typeInfo.index = static_cast<int> (i);

        result.push_back (typeInfo);
    }

    return result;
}

///////////////////////////////////////////////////////////////////////////////
VkDeviceMemory AllocateMemory (const std::vector<MemoryTypeInfo>& memoryInfos,
    VkDevice device, const int size, bool* isHostCoherent = nullptr)
{
    // We take the first HOST_VISIBLE memory
    for (auto& memoryInfo : memoryInfos)
    {
        if (memoryInfo.hostVisible)
        {
            VkMemoryAllocateInfo memoryAllocateInfo = {};
            memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            memoryAllocateInfo.memoryTypeIndex = memoryInfo.index;
            memoryAllocateInfo.allocationSize = size;

            VkDeviceMemory deviceMemory;
            vkAllocateMemory (device, &memoryAllocateInfo, nullptr,
                &deviceMemory);

            if (isHostCoherent)
            {
                *isHostCoherent = memoryInfo.hostCoherent;
            }

            return deviceMemory;
        }
    }

    return VK_NULL_HANDLE;
}

///////////////////////////////////////////////////////////////////////////////
VkBuffer AllocateBuffer (VkDevice device, const int size,
    const VkBufferUsageFlagBits bits)
{
    VkBufferCreateInfo bufferCreateInfo = {};
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.size = static_cast<uint32_t> (size);
    bufferCreateInfo.usage = bits;

    VkBuffer result;
    vkCreateBuffer (device, &bufferCreateInfo, nullptr, &result);
    return result;
}

///////////////////////////////////////////////////////////////////////////////
VkPipelineLayout CreatePipelineLayout (VkDevice device)
{
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    VkPipelineLayout result;
    vkCreatePipelineLayout (device, &pipelineLayoutCreateInfo, nullptr,
        &result);

    return result;
}

///////////////////////////////////////////////////////////////////////////////
VkShaderModule LoadShader (VkDevice device, const void* shaderContents,
    const size_t size)
{
    VkShaderModuleCreateInfo shaderModuleCreateInfo = {};
    shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;

    shaderModuleCreateInfo.pCode = static_cast<const uint32_t*> (shaderContents);
    shaderModuleCreateInfo.codeSize = size;

    VkShaderModule result;
    vkCreateShaderModule (device, &shaderModuleCreateInfo, nullptr, &result);

    return result;
}

///////////////////////////////////////////////////////////////////////////////
VkPipeline CreatePipeline (VkDevice device, VkRenderPass renderPass, VkPipelineLayout layout,
    VkShaderModule vertexShader, VkShaderModule fragmentShader,
    VkExtent2D viewportSize)
{
    VkVertexInputBindingDescription vertexInputBindingDescription;
    vertexInputBindingDescription.binding = 0;
    vertexInputBindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    vertexInputBindingDescription.stride = sizeof (float) * 5;

    VkVertexInputAttributeDescription vertexInputAttributeDescription [2] = {};
    vertexInputAttributeDescription [0].binding = vertexInputBindingDescription.binding;
    vertexInputAttributeDescription [0].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertexInputAttributeDescription [0].location = 0;
    vertexInputAttributeDescription [0].offset = 0;

    vertexInputAttributeDescription [1].binding = vertexInputBindingDescription.binding;
    vertexInputAttributeDescription [1].format = VK_FORMAT_R32G32_SFLOAT;
    vertexInputAttributeDescription [1].location = 1;
    vertexInputAttributeDescription [1].offset = sizeof (float) * 3;

    VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo = {};
    pipelineVertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    pipelineVertexInputStateCreateInfo.vertexAttributeDescriptionCount = std::extent<decltype(vertexInputAttributeDescription)>::value;
    pipelineVertexInputStateCreateInfo.pVertexAttributeDescriptions = vertexInputAttributeDescription;
    pipelineVertexInputStateCreateInfo.vertexBindingDescriptionCount = 1;
    pipelineVertexInputStateCreateInfo.pVertexBindingDescriptions = &vertexInputBindingDescription;

    VkPipelineInputAssemblyStateCreateInfo  pipelineInputAssemblyStateCreateInfo = {};
    pipelineInputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    pipelineInputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo pipelineViewportStateCreateInfo = {};
    pipelineViewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;

    VkViewport viewport;
    viewport.height = static_cast<float> (viewportSize.height);
    viewport.width = static_cast<float> (viewportSize.width);
    viewport.x = 0;
    viewport.y = 0;
    viewport.minDepth = 0;
    viewport.maxDepth = 1;

    pipelineViewportStateCreateInfo.viewportCount = 1;
    pipelineViewportStateCreateInfo.pViewports = &viewport;

    VkRect2D rect;
    rect.extent = viewportSize;
    rect.offset.x = 0;
    rect.offset.y = 0;

    pipelineViewportStateCreateInfo.scissorCount = 1;
    pipelineViewportStateCreateInfo.pScissors = &rect;

    VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentState = {};
    pipelineColorBlendAttachmentState.colorWriteMask = 0xF;
    pipelineColorBlendAttachmentState.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo = {};
    pipelineColorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;

    pipelineColorBlendStateCreateInfo.attachmentCount = 1;
    pipelineColorBlendStateCreateInfo.pAttachments = &pipelineColorBlendAttachmentState;

    VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo = {};
    pipelineRasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    pipelineRasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
    pipelineRasterizationStateCreateInfo.cullMode = VK_CULL_MODE_NONE;
    pipelineRasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    pipelineRasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
    pipelineRasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
    pipelineRasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
    pipelineRasterizationStateCreateInfo.lineWidth = 1.0f;

    VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCreateInfo = {};
    pipelineDepthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    pipelineDepthStencilStateCreateInfo.depthTestEnable = VK_FALSE;
    pipelineDepthStencilStateCreateInfo.depthWriteEnable = VK_FALSE;
    pipelineDepthStencilStateCreateInfo.depthCompareOp = VK_COMPARE_OP_ALWAYS;
    pipelineDepthStencilStateCreateInfo.depthBoundsTestEnable = VK_FALSE;
    pipelineDepthStencilStateCreateInfo.back.failOp = VK_STENCIL_OP_KEEP;
    pipelineDepthStencilStateCreateInfo.back.passOp = VK_STENCIL_OP_KEEP;
    pipelineDepthStencilStateCreateInfo.back.compareOp = VK_COMPARE_OP_ALWAYS;
    pipelineDepthStencilStateCreateInfo.stencilTestEnable = VK_FALSE;
    pipelineDepthStencilStateCreateInfo.front = pipelineDepthStencilStateCreateInfo.back;

    VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo = {};
    pipelineMultisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    pipelineMultisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfos [2] = {};
    pipelineShaderStageCreateInfos [0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineShaderStageCreateInfos [0].module = vertexShader;
    pipelineShaderStageCreateInfos [0].pName = "main";
    pipelineShaderStageCreateInfos [0].stage = VK_SHADER_STAGE_VERTEX_BIT;

    pipelineShaderStageCreateInfos [1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineShaderStageCreateInfos [1].module = fragmentShader;
    pipelineShaderStageCreateInfos [1].pName = "main";
    pipelineShaderStageCreateInfos [1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {};
    graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

    graphicsPipelineCreateInfo.layout = layout;
    graphicsPipelineCreateInfo.pVertexInputState = &pipelineVertexInputStateCreateInfo;
    graphicsPipelineCreateInfo.pInputAssemblyState = &pipelineInputAssemblyStateCreateInfo;
    graphicsPipelineCreateInfo.renderPass = renderPass;
    graphicsPipelineCreateInfo.pViewportState = &pipelineViewportStateCreateInfo;
    graphicsPipelineCreateInfo.pColorBlendState = &pipelineColorBlendStateCreateInfo;
    graphicsPipelineCreateInfo.pRasterizationState = &pipelineRasterizationStateCreateInfo;
    graphicsPipelineCreateInfo.pDepthStencilState = &pipelineDepthStencilStateCreateInfo;
    graphicsPipelineCreateInfo.pMultisampleState = &pipelineMultisampleStateCreateInfo;
    graphicsPipelineCreateInfo.pStages = pipelineShaderStageCreateInfos;
    graphicsPipelineCreateInfo.stageCount = 2;

    VkPipeline pipeline;
    vkCreateGraphicsPipelines (device, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo,
        nullptr, &pipeline);

    return pipeline;
}
}

///////////////////////////////////////////////////////////////////////////////
void VulkanQuad::CreateMeshBuffers (VkCommandBuffer /*uploadCommandBuffer*/)
{
    struct Vertex
    {
        float position[3];
        float uv[2];
    };

    static const Vertex vertices[4] = {
        // Upper Left
        { { -1.0f, 1.0f, 0 }, { 0, 0 } },
        // Upper Right
        { { 1.0f, 1.0f, 0 }, { 1, 0 } },
        // Bottom right
        { { 1.0f, -1.0f, 0 }, { 1, 1 } },
        // Bottom left
        { { -1.0f, -1.0f, 0 }, { 0, 1 } }
    };

    static const int indices[6] = {
        0, 1, 2, 2, 3, 0
    };

    auto memoryHeaps = EnumerateHeaps (physicalDevice_);
    indexBuffer_ = AllocateBuffer(device_, sizeof(indices),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    vertexBuffer_ = AllocateBuffer (device_, sizeof (vertices),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    VkMemoryRequirements vertexBufferMemoryRequirements = {};
    vkGetBufferMemoryRequirements(device_, vertexBuffer_,
        &vertexBufferMemoryRequirements);
    VkMemoryRequirements indexBufferMemoryRequirements = {};
    vkGetBufferMemoryRequirements(device_, indexBuffer_,
        &indexBufferMemoryRequirements);

    VkDeviceSize bufferSize = vertexBufferMemoryRequirements.size;
    // We want to place the index buffer behind the vertex buffer. Need to take
    // the alignment into account to find the next suitable location
    VkDeviceSize indexBufferOffset = RoundToNextMultiple(bufferSize,
        indexBufferMemoryRequirements.alignment);

    bufferSize = indexBufferOffset + indexBufferMemoryRequirements.size;
    bool memoryIsHostCoherent = false;
    deviceMemory_ = AllocateMemory(memoryHeaps, device_,
        static_cast<int>(bufferSize), &memoryIsHostCoherent);

    vkBindBufferMemory (device_, vertexBuffer_, deviceMemory_, 0);
    vkBindBufferMemory (device_, indexBuffer_, deviceMemory_,
        indexBufferOffset);

    void* mapping = nullptr;
    vkMapMemory (device_, deviceMemory_, 0, VK_WHOLE_SIZE,
        0, &mapping);
    ::memcpy (mapping, vertices, sizeof (vertices));

    ::memcpy (static_cast<uint8_t*> (mapping) + indexBufferOffset,
        indices, sizeof (indices));

    if (!memoryIsHostCoherent)
    {
        VkMappedMemoryRange mappedMemoryRange = {};
        mappedMemoryRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        mappedMemoryRange.memory = deviceMemory_;
        mappedMemoryRange.offset = 0;
        mappedMemoryRange.size = VK_WHOLE_SIZE;

        vkFlushMappedMemoryRanges (device_, 1, &mappedMemoryRange);
    }

    vkUnmapMemory (device_, deviceMemory_);
}

///////////////////////////////////////////////////////////////////////////////
void VulkanQuad::CreatePipelineStateObject ()
{
    vertexShader_ = LoadShader (device_, BasicVertexShader, sizeof (BasicVertexShader));
    fragmentShader_ = LoadShader (device_, BasicFragmentShader, sizeof (BasicFragmentShader));

    pipelineLayout_ = CreatePipelineLayout (device_);
    VkExtent2D extent = {
        static_cast<uint32_t> (window_->GetWidth ()),
        static_cast<uint32_t> (window_->GetHeight ())
    };
    pipeline_ = CreatePipeline(device_, renderPass_, pipelineLayout_,
        vertexShader_, fragmentShader_, extent);
}
}
