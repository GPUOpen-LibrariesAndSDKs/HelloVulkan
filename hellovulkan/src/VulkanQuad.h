#ifndef AMD_QUAD_D3D12_SAMPLE_H_
#define AMD_QUAD_D3D12_SAMPLE_H_

#include "VulkanSample.h"

namespace AMD {
class VulkanQuad : public VulkanSample
{
private:
	void CreatePipelineStateObject ();
	void CreateMeshBuffers (VkCommandBuffer uploadCommandList);
	void RenderImpl (VkCommandBuffer commandList) override;
	void InitializeImpl (VkCommandBuffer uploadCommandList) override;
	void ShutdownImpl () override;
	
	VkDeviceMemory deviceMemory_ = VK_NULL_HANDLE;
	VkBuffer vertexBuffer_ = VK_NULL_HANDLE;
	VkBuffer indexBuffer_ = VK_NULL_HANDLE;

	VkShaderModule vertexShader_ = VK_NULL_HANDLE;
	VkShaderModule fragmentShader_ = VK_NULL_HANDLE;

	VkPipeline pipeline_ = VK_NULL_HANDLE;
	VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
};
}

#endif