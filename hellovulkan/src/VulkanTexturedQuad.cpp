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

#include "VulkanTexturedQuad.h"

#include "Shaders.h"
#include "Utility.h"
#include "Window.h"

#include "RubyTexture.h"
#include "ImageIO.h"

#include <vector>

namespace AMD
{
///////////////////////////////////////////////////////////////////////////////
void VulkanTexturedQuad::ShutdownImpl()
{
    vkDestroyPipeline(device_, pipeline_, nullptr);
    vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);

    vkDestroyBuffer(device_, vertexBuffer_, nullptr);
    vkDestroyBuffer(device_, indexBuffer_, nullptr);
    vkFreeMemory(device_, deviceBufferMemory_, nullptr);

    vkDestroyImageView (device_, rubyImageView_, nullptr);
    vkDestroyImage (device_, rubyImage_, nullptr);
    vkFreeMemory (device_, deviceImageMemory_, nullptr);

    vkDestroyBuffer (device_, uploadImageBuffer_, nullptr);
    vkFreeMemory (device_, uploadImageMemory_, nullptr);

    vkDestroyBuffer (device_, uploadBufferBuffer_, nullptr);
    vkFreeMemory (device_, uploadBufferMemory_, nullptr);

    vkDestroyDescriptorSetLayout (device_, descriptorSetLayout_, nullptr);
    vkDestroyDescriptorPool (device_, descriptorPool_, nullptr);

    vkDestroySampler (device_, sampler_, nullptr);

    vkDestroyShaderModule(device_, vertexShader_, nullptr);
    vkDestroyShaderModule(device_, fragmentShader_, nullptr);

    VulkanSample::ShutdownImpl();
}

///////////////////////////////////////////////////////////////////////////////
void VulkanTexturedQuad::RenderImpl(VkCommandBuffer commandBuffer)
{
    VulkanSample::RenderImpl(commandBuffer);

    VkViewport viewports [1] = {};
    viewports [0].width = static_cast<float> (window_->GetWidth ());
    viewports [0].height = static_cast<float> (window_->GetHeight ());
    viewports [0].minDepth = 0;
    viewports [0].maxDepth = 1;

    vkCmdSetViewport (commandBuffer, 0, 1, viewports);

    VkRect2D scissors [1] = {};
    scissors [0].extent.width = window_->GetWidth ();
    scissors [0].extent.height = window_->GetHeight ();
    vkCmdSetScissor (commandBuffer, 0, 1, scissors);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipeline_);
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer_, 0, VK_INDEX_TYPE_UINT32);
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer_, offsets);

    vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipelineLayout_, 0, 1, &descriptorSet_, 0, nullptr);

    vkCmdDrawIndexed(commandBuffer, 6, 1, 0, 0, 0);
}

///////////////////////////////////////////////////////////////////////////////
void VulkanTexturedQuad::InitializeImpl(VkCommandBuffer uploadCommandBuffer)
{
    VulkanSample::InitializeImpl(uploadCommandBuffer);

    CreateSampler ();
    CreateTexture (uploadCommandBuffer);
    CreateDescriptors ();
    CreatePipelineStateObject();
    CreateMeshBuffers(uploadCommandBuffer);
}

namespace
{
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
std::vector<MemoryTypeInfo> EnumerateHeaps(VkPhysicalDevice device)
{
    VkPhysicalDeviceMemoryProperties memoryProperties = {};
    vkGetPhysicalDeviceMemoryProperties(device, &memoryProperties);

    std::vector<MemoryTypeInfo::Heap> heaps;

    for (uint32_t i = 0; i < memoryProperties.memoryHeapCount; ++i)
    {
        MemoryTypeInfo::Heap info;
        info.size = memoryProperties.memoryHeaps[i].size;
        info.deviceLocal = (memoryProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0;

        heaps.push_back(info);
    }

    std::vector<MemoryTypeInfo> result;

    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i)
    {
        MemoryTypeInfo typeInfo;

        typeInfo.deviceLocal = (memoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0;
        typeInfo.hostVisible = (memoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0;
        typeInfo.hostCoherent = (memoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0;
        typeInfo.hostCached = (memoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) != 0;
        typeInfo.lazilyAllocated = (memoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT) != 0;

        typeInfo.heap = heaps[memoryProperties.memoryTypes[i].heapIndex];

        typeInfo.index = static_cast<int> (i);

        result.push_back(typeInfo);
    }

    return result;
}

enum MemoryProperties
{
    MT_DeviceLocal = 1,
    MT_HostVisible = 2
};

///////////////////////////////////////////////////////////////////////////////
VkDeviceMemory AllocateMemory(const std::vector<MemoryTypeInfo>& memoryInfos,
    VkDevice device, const int size, const uint32_t memoryBits,
    unsigned int memoryProperties, bool* isHostCoherent = nullptr)
{
    for (auto& memoryInfo : memoryInfos)
    {
        if (((1 << memoryInfo.index) & memoryBits) == 0) {
            continue;
        }

        if ((memoryProperties & MT_DeviceLocal) && !memoryInfo.deviceLocal)
        {
            continue;
        }

        if ((memoryProperties & MT_HostVisible) && !memoryInfo.hostVisible)
        {
            continue;
        }

        if (isHostCoherent)
        {
            *isHostCoherent = memoryInfo.hostCoherent;
        }

        VkMemoryAllocateInfo memoryAllocateInfo = {};
        memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memoryAllocateInfo.memoryTypeIndex = memoryInfo.index;
        memoryAllocateInfo.allocationSize = size;

        VkDeviceMemory deviceMemory;
        vkAllocateMemory(device, &memoryAllocateInfo, nullptr,
            &deviceMemory);
        return deviceMemory;
    }

    return VK_NULL_HANDLE;
}

///////////////////////////////////////////////////////////////////////////////
VkBuffer AllocateBuffer(VkDevice device, const int size,
    const VkBufferUsageFlags bits)
{
    VkBufferCreateInfo bufferCreateInfo = {};
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.size = static_cast<uint32_t> (size);
    bufferCreateInfo.usage = bits;

    VkBuffer result;
    vkCreateBuffer(device, &bufferCreateInfo, nullptr, &result);
    return result;
}

///////////////////////////////////////////////////////////////////////////////
VkPipelineLayout CreatePipelineLayout(VkDevice device)
{
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    VkPipelineLayout result;
    vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr,
        &result);

    return result;
}

///////////////////////////////////////////////////////////////////////////////
VkShaderModule LoadShader(VkDevice device, const void* shaderContents,
    const size_t size)
{
    VkShaderModuleCreateInfo shaderModuleCreateInfo = {};
    shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;

    shaderModuleCreateInfo.pCode = static_cast<const uint32_t*> (shaderContents);
    shaderModuleCreateInfo.codeSize = size;

    VkShaderModule result;
    vkCreateShaderModule(device, &shaderModuleCreateInfo, nullptr, &result);

    return result;
}

///////////////////////////////////////////////////////////////////////////////
VkPipeline CreatePipeline(VkDevice device, VkRenderPass renderPass, VkPipelineLayout layout,
    VkShaderModule vertexShader, VkShaderModule fragmentShader)
{
    VkVertexInputBindingDescription vertexInputBindingDescription;
    vertexInputBindingDescription.binding = 0;
    vertexInputBindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    vertexInputBindingDescription.stride = sizeof(float) * 5;

    VkVertexInputAttributeDescription vertexInputAttributeDescription[2] = {};
    vertexInputAttributeDescription[0].binding = vertexInputBindingDescription.binding;
    vertexInputAttributeDescription[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertexInputAttributeDescription[0].location = 0;
    vertexInputAttributeDescription[0].offset = 0;

    vertexInputAttributeDescription[1].binding = vertexInputBindingDescription.binding;
    vertexInputAttributeDescription[1].format = VK_FORMAT_R32G32_SFLOAT;
    vertexInputAttributeDescription[1].location = 1;
    vertexInputAttributeDescription[1].offset = sizeof(float) * 3;

    VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo = {};
    pipelineVertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    pipelineVertexInputStateCreateInfo.vertexAttributeDescriptionCount = std::extent<decltype(vertexInputAttributeDescription)>::value;
    pipelineVertexInputStateCreateInfo.pVertexAttributeDescriptions = vertexInputAttributeDescription;
    pipelineVertexInputStateCreateInfo.vertexBindingDescriptionCount = 1;
    pipelineVertexInputStateCreateInfo.pVertexBindingDescriptions = &vertexInputBindingDescription;

    VkPipelineInputAssemblyStateCreateInfo  pipelineInputAssemblyStateCreateInfo = {};
    pipelineInputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    pipelineInputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkDynamicState dynamicStates [] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };    

    VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
    dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicStateCreateInfo.dynamicStateCount = 2;
    dynamicStateCreateInfo.pDynamicStates = dynamicStates;	

    VkPipelineViewportStateCreateInfo pipelineViewportStateCreateInfo = {};
    pipelineViewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    pipelineViewportStateCreateInfo.viewportCount = 1;
    pipelineViewportStateCreateInfo.scissorCount = 1;

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

    VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfos[2] = {};
    pipelineShaderStageCreateInfos[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineShaderStageCreateInfos[0].module = vertexShader;
    pipelineShaderStageCreateInfos[0].pName = "main";
    pipelineShaderStageCreateInfos[0].stage = VK_SHADER_STAGE_VERTEX_BIT;

    pipelineShaderStageCreateInfos[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineShaderStageCreateInfos[1].module = fragmentShader;
    pipelineShaderStageCreateInfos[1].pName = "main";
    pipelineShaderStageCreateInfos[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;

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
    graphicsPipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
    graphicsPipelineCreateInfo.pStages = pipelineShaderStageCreateInfos;
    graphicsPipelineCreateInfo.stageCount = 2;

    VkPipeline pipeline;
    vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo,
        nullptr, &pipeline);

    return pipeline;
}
}   // namespace

///////////////////////////////////////////////////////////////////////////////
void VulkanTexturedQuad::CreateMeshBuffers(VkCommandBuffer uploadCommandBuffer)
{
    struct Vertex
    {
        float position[3];
        float uv[2];
    };

    static const Vertex vertices[4] =
    {
        // Upper Left
        { { -1.0f,  1.0f, 0 }, { 0, 1 } },
        // Upper Right
        { {  1.0f,  1.0f, 0 }, { 1, 1 } },
        // Bottom right
        { {  1.0f, -1.0f, 0 }, { 1, 0 } },
        // Bottom left
        { { -1.0f, -1.0f, 0 }, { 0, 0 } }
    };

    static const int indices[6] =
    {
        0, 1, 2, 2, 3, 0
    };

    auto memoryHeaps = EnumerateHeaps(physicalDevice_);

    indexBuffer_ = AllocateBuffer(device_, sizeof(indices),
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    vertexBuffer_ = AllocateBuffer(device_, sizeof(vertices),
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

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
    deviceBufferMemory_ = AllocateMemory(memoryHeaps, device_,
        static_cast<int>(bufferSize),
        vertexBufferMemoryRequirements.memoryTypeBits & indexBufferMemoryRequirements.memoryTypeBits,
        MT_DeviceLocal);

    vkBindBufferMemory(device_, vertexBuffer_, deviceBufferMemory_, 0);
    vkBindBufferMemory(device_, indexBuffer_, deviceBufferMemory_,
        indexBufferOffset);

    uploadBufferBuffer_ = AllocateBuffer (device_, static_cast<int> (bufferSize),
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    VkMemoryRequirements uploadBufferMemoryRequirements = {};
    vkGetBufferMemoryRequirements (device_, uploadBufferBuffer_,
        &uploadBufferMemoryRequirements);

    bool memoryIsHostCoherent = false;
    uploadBufferMemory_ = AllocateMemory(memoryHeaps, device_,
        static_cast<int>(uploadBufferMemoryRequirements.size),
        vertexBufferMemoryRequirements.memoryTypeBits & indexBufferMemoryRequirements.memoryTypeBits,
        MT_HostVisible, &memoryIsHostCoherent);

    vkBindBufferMemory (device_, uploadBufferBuffer_, uploadBufferMemory_, 0);

    void* mapping = nullptr;
    vkMapMemory(device_, uploadBufferMemory_, 0, VK_WHOLE_SIZE,
        0, &mapping);
    ::memcpy(mapping, vertices, sizeof(vertices));

    ::memcpy(static_cast<uint8_t*> (mapping) + indexBufferOffset,
        indices, sizeof(indices));

    if (!memoryIsHostCoherent) 
    {
        VkMappedMemoryRange mappedMemoryRange = {};
        mappedMemoryRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        mappedMemoryRange.memory = uploadBufferMemory_;
        mappedMemoryRange.offset = 0;
        mappedMemoryRange.size = VK_WHOLE_SIZE;

        vkFlushMappedMemoryRanges (device_, 1, &mappedMemoryRange);
    }

    vkUnmapMemory(device_, uploadBufferMemory_);

    VkBufferCopy vertexCopy = {};
    vertexCopy.size = sizeof (vertices);

    VkBufferCopy indexCopy = {};
    indexCopy.size = sizeof (indices);
    indexCopy.srcOffset = indexBufferOffset;

    vkCmdCopyBuffer (uploadCommandBuffer, uploadBufferBuffer_, vertexBuffer_,
        1, &vertexCopy);
    vkCmdCopyBuffer (uploadCommandBuffer, uploadBufferBuffer_, indexBuffer_,
        1, &indexCopy);

    VkBufferMemoryBarrier uploadBarriers[2] = {};
    uploadBarriers[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    uploadBarriers[0].buffer = vertexBuffer_;
    uploadBarriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    uploadBarriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    uploadBarriers[0].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    uploadBarriers[0].dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    uploadBarriers[0].size = VK_WHOLE_SIZE;

    uploadBarriers[1].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    uploadBarriers[1].buffer = indexBuffer_;
    uploadBarriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    uploadBarriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    uploadBarriers[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    uploadBarriers[1].dstAccessMask = VK_ACCESS_INDEX_READ_BIT;
    uploadBarriers[1].size = VK_WHOLE_SIZE;

    vkCmdPipelineBarrier (uploadCommandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
        0, 0, nullptr, 2, uploadBarriers,
        0, nullptr);
}

///////////////////////////////////////////////////////////////////////////////
void VulkanTexturedQuad::CreateTexture (VkCommandBuffer uploadCommandList)
{
    int width, height;
    auto image = LoadImageFromMemory (RubyTexture, sizeof (RubyTexture),
        1, &width, &height);

    VkImageCreateInfo imageCreateInfo = {};
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.pNext = nullptr;
    imageCreateInfo.queueFamilyIndexCount = 1;
    uint32_t queueFamilyIndex = static_cast<uint32_t> (queueFamilyIndex_);
    imageCreateInfo.pQueueFamilyIndices = &queueFamilyIndex;
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.extent.depth = 1;
    imageCreateInfo.extent.height = height;
    imageCreateInfo.extent.width = width;
    imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    vkCreateImage (device_, &imageCreateInfo, nullptr, &rubyImage_);

    VkMemoryRequirements requirements = {};
    vkGetImageMemoryRequirements (device_, rubyImage_,
        &requirements);

    VkDeviceSize requiredSizeForImage = requirements.size;

    auto memoryHeaps = EnumerateHeaps (physicalDevice_);
    deviceImageMemory_ = AllocateMemory (memoryHeaps, device_, static_cast<int> (requiredSizeForImage),
        requirements.memoryTypeBits,
        MT_DeviceLocal);

    vkBindImageMemory (device_, rubyImage_, deviceImageMemory_, 0);

    VkBufferCreateInfo bufferCreateInfo = {};
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.pNext = nullptr;
    bufferCreateInfo.queueFamilyIndexCount = 1;
    bufferCreateInfo.pQueueFamilyIndices = &queueFamilyIndex;
    bufferCreateInfo.size = requiredSizeForImage;
    bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    vkCreateBuffer (device_, &bufferCreateInfo, nullptr, &uploadImageBuffer_);

    vkGetBufferMemoryRequirements (device_, uploadImageBuffer_, &requirements);

    VkDeviceSize requiredSizeForBuffer = requirements.size;

    bool memoryIsHostCoherent = false;
    uploadImageMemory_ = AllocateMemory (memoryHeaps, device_,
        static_cast<int> (requiredSizeForBuffer), requirements.memoryTypeBits,
        MT_HostVisible, &memoryIsHostCoherent);

    vkBindBufferMemory (device_, uploadImageBuffer_, uploadImageMemory_, 0);

    void* data = nullptr;
    vkMapMemory (device_, uploadImageMemory_, 0, VK_WHOLE_SIZE,
        0, &data);
    ::memcpy (data, image.data (), image.size ());
    
    if (!memoryIsHostCoherent) 
    {
        VkMappedMemoryRange mappedMemoryRange = {};
        mappedMemoryRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        mappedMemoryRange.memory = uploadImageMemory_;
        mappedMemoryRange.offset = 0;
        mappedMemoryRange.size = VK_WHOLE_SIZE;

        vkFlushMappedMemoryRanges (device_, 1, &mappedMemoryRange);
    }

    vkUnmapMemory (device_, uploadImageMemory_);

    VkBufferImageCopy bufferImageCopy = {};
    bufferImageCopy.imageExtent.width = width;
    bufferImageCopy.imageExtent.height = height;
    bufferImageCopy.imageExtent.depth = 1;
    bufferImageCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    bufferImageCopy.imageSubresource.mipLevel = 0;
    bufferImageCopy.imageSubresource.layerCount = 1;

    VkImageMemoryBarrier imageBarrier = {};
    imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageBarrier.pNext = nullptr;
    imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    imageBarrier.srcAccessMask = 0;
    imageBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    imageBarrier.image = rubyImage_;
    imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageBarrier.subresourceRange.layerCount = 1;
    imageBarrier.subresourceRange.levelCount = 1;

    vkCmdPipelineBarrier (uploadCommandList,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr,
        1, &imageBarrier);

    vkCmdCopyBufferToImage (uploadCommandList, uploadImageBuffer_,
        rubyImage_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &bufferImageCopy);

    imageBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    imageBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    imageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    imageBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    vkCmdPipelineBarrier (uploadCommandList,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, nullptr, 0, nullptr,
        1, &imageBarrier);

    VkImageViewCreateInfo imageViewCreateInfo = {};
    imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewCreateInfo.format = imageCreateInfo.format;
    imageViewCreateInfo.image = rubyImage_;
    imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageViewCreateInfo.subresourceRange.levelCount = 1;
    imageViewCreateInfo.subresourceRange.layerCount = 1;
    imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;

    vkCreateImageView (device_, &imageViewCreateInfo, nullptr, &rubyImageView_);
}

///////////////////////////////////////////////////////////////////////////////
void VulkanTexturedQuad::CreateSampler ()
{
    VkSamplerCreateInfo samplerCreateInfo = {};
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.minFilter = VK_FILTER_LINEAR;

    vkCreateSampler (device_, &samplerCreateInfo, nullptr, &sampler_);
}

///////////////////////////////////////////////////////////////////////////////
void VulkanTexturedQuad::CreateDescriptors ()
{
    VkDescriptorSetLayoutBinding descriptorSetLayoutBinding[2] = {};
    descriptorSetLayoutBinding[0].binding = 0;
    descriptorSetLayoutBinding[0].descriptorCount = 1;
    descriptorSetLayoutBinding[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    descriptorSetLayoutBinding[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    descriptorSetLayoutBinding[1].binding = 1;
    descriptorSetLayoutBinding[1].descriptorCount = 1;
    descriptorSetLayoutBinding[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    descriptorSetLayoutBinding[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    descriptorSetLayoutBinding[1].pImmutableSamplers = &sampler_;

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo[1] = {};
    descriptorSetLayoutCreateInfo[0].sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutCreateInfo[0].bindingCount = 2;
    descriptorSetLayoutCreateInfo[0].pBindings = descriptorSetLayoutBinding;

    vkCreateDescriptorSetLayout (
        device_, descriptorSetLayoutCreateInfo,
        nullptr, &descriptorSetLayout_);

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayout_;
    pipelineLayoutCreateInfo.setLayoutCount = 1;

    vkCreatePipelineLayout (device_, &pipelineLayoutCreateInfo,
        nullptr, &pipelineLayout_);

    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
    descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolCreateInfo.maxSets = 1;

    VkDescriptorPoolSize descriptorPoolSize [2] = {};
    descriptorPoolSize[0].descriptorCount = 1;
    descriptorPoolSize[0].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    descriptorPoolSize[1].descriptorCount = 1;
    descriptorPoolSize[1].type = VK_DESCRIPTOR_TYPE_SAMPLER;

    descriptorPoolCreateInfo.poolSizeCount = 2;
    descriptorPoolCreateInfo.pPoolSizes = descriptorPoolSize;

    vkCreateDescriptorPool (device_, &descriptorPoolCreateInfo,
        nullptr, &descriptorPool_);

    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
    descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorSetAllocateInfo.pSetLayouts = &descriptorSetLayout_;
    descriptorSetAllocateInfo.descriptorSetCount = 1;
    descriptorSetAllocateInfo.descriptorPool = descriptorPool_;

    vkAllocateDescriptorSets (device_, &descriptorSetAllocateInfo, &descriptorSet_);

    VkWriteDescriptorSet writeDescriptorSets[1] = {};
    writeDescriptorSets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSets[0].dstSet = descriptorSet_;
    writeDescriptorSets[0].descriptorCount = 1;
    writeDescriptorSets[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    writeDescriptorSets[0].dstBinding = 0;

    VkDescriptorImageInfo descriptorImageInfo[1] = {};
    descriptorImageInfo[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    descriptorImageInfo[0].imageView = rubyImageView_;

    writeDescriptorSets[0].pImageInfo = &descriptorImageInfo[0];

    vkUpdateDescriptorSets (device_, 1, writeDescriptorSets, 0, nullptr);
}

///////////////////////////////////////////////////////////////////////////////
void VulkanTexturedQuad::CreatePipelineStateObject()
{
    vertexShader_ = LoadShader(device_, BasicVertexShader, sizeof(BasicVertexShader));
    fragmentShader_ = LoadShader(device_, TexturedFragmentShader, sizeof(TexturedFragmentShader));

    pipeline_ = CreatePipeline(device_, renderPass_, pipelineLayout_,
        vertexShader_, fragmentShader_);
}
}   // namespace AMD
