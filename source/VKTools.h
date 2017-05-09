#ifndef VKTOOLS_H
#define VKTOOLS_H

#include <string>
#include <iostream>
#include <vulkan.h>
#include "DataTypes.h"

class VulkanCore;

// Macro to check and display Vulkan return results
#define VK_CHECK_RESULT(f)																				\
{																										\
	VkResult res = (f);																					\
	if (res != VK_SUCCESS)																				\
	{																									\
		std::cout << "Fatal : VkResult is \"" << VKTools::ErrorString(res) << "\" in " << __FILE__ << " at line " << __LINE__ << std::endl; \
		assert(res == VK_SUCCESS);																		\
	}																									\
}	

namespace VKTools
{
	// Return string representation of a vulkan error string
	extern std::string ErrorString(VkResult errorCode);
	// Asserts and outputs the error message if the result is not VK_SUCCESS
	extern VkResult CheckResult(VkResult result);
	// Selected a suitable supported depth format starting with 32 bit down to 16 bit
	// Returns false if none of the depth formats in the list is supported by the device
	extern VkBool32 GetSupportedDepthFormat(VkPhysicalDevice physicalDevice, VkFormat *depthFormat);
	// Get memory type for a given memory allocation ( flags and bits)
	extern uint32_t GetMemoryType(VulkanCore* core, uint32_t typeBits, VkFlags properties);
	// Fatal error with message box
	extern void ExitFatal(std::string message, std::string caption);
	// Put an image memory barrier for setting an image layout on the sub resource into the given command buffer
	extern void SetImageLayout(
		VkCommandBuffer cmdbuffer,
		VkImage image,
		VkImageLayout oldImageLayout,
		VkImageLayout newImageLayout,
		VkImageSubresourceRange subresourceRange);
	// Uses a fixed sub resource layout with first mip level and layer
	extern void SetImageLayout(
		VkCommandBuffer cmdbuffer,
		VkImage image,
		VkImageAspectFlags aspectMask,
		VkImageLayout oldImageLayout,
		VkImageLayout newImageLayout);
	// Commandbuffer helper functions
	extern int32_t FlushCommandBuffer(	
		VkCommandBuffer commandBuffer, 
		VkQueue queue,
		VkDevice viewDevice,
		VkCommandPool commandPool, 
		bool free);
	// Create Buffers
	extern uint32_t CreateBuffer(VulkanCore* core,
		VkDevice device,
		VkBufferUsageFlags usageFlags,
		VkMemoryPropertyFlags memoryPropertyFlags,
		VkDeviceSize size,
		void* data,
		VkBuffer * buffer,
		VkDeviceMemory * memory);
	extern uint32_t CreateBuffer(VulkanCore* core,
		VkDevice device,
		VkBufferUsageFlags usage,
		VkMemoryPropertyFlags memoryPropertyFlags,
		VkDeviceSize size,
		void* data,
		VkBuffer * buffer,
		VkDeviceMemory * memory,
		VkDescriptorBufferInfo * descriptor);
	// Copy buffer
	extern uint32_t CopyBuffer(VulkanCore* core,
		VkDevice device,
		VkCommandPool commandpool,
		VkQueue queue,
		VkBuffer src,
		VkBuffer dst,
		uint32_t size);
	// Attatchment creations
	extern void CreateAttachment(
		VkDevice device,
		VulkanCore* core,
		VkFormat format,
		VkImageUsageFlagBits usage,
		FrameBufferAttachment* attachment,
		uint32_t width,
		uint32_t height);
	extern void CreateImage(
		VulkanCore* core,
		VkFormat format,
		FrameBufferAttachment* attachment,
		uint32_t width,
		uint32_t height);
	// Initaialize functions
	namespace Initializers
	{
		// Buffer helper functions
		extern VkBufferCreateInfo BufferCreateInfo();
		extern VkBufferCreateInfo BufferCreateInfo(VkBufferUsageFlags usage, VkDeviceSize size);
		// Image initializer helper functions
		extern VkImageCreateInfo		ImageCreateInfo();
		extern VkSamplerCreateInfo		SamplerCreateInfo();
		extern VkImageMemoryBarrier	ImageMemoryBarrier();
		extern VkImageViewCreateInfo	ImageViewCreateInfo();
		// Memory initializer helper funcitons
		extern VkMemoryAllocateInfo	MemoryAllocateCreateInfo();
		// Semaphore initializer helper functions
		extern VkSemaphoreCreateInfo	SemaphoreCreateInfo();
		extern VkSubmitInfo			SubmitInfo();
		// Commandbuffer initializer helper functions
		extern VkCommandBufferAllocateInfo CommandBufferAllocateInfo(VkCommandPool commandPool, VkCommandBufferLevel level, uint32_t bufferCount);
		extern VkCommandBufferBeginInfo CommandBufferBeginInfo();
		extern VkCommandBuffer CreateCommandBuffer(VkCommandPool commandPool,VkDevice device, VkCommandBufferLevel level, bool start);
		extern VkMemoryAllocateInfo MemoryAllocateInfo();
		extern VkComputePipelineCreateInfo ComputePipelineCreateInfo(VkPipelineLayout layout, VkPipelineCreateFlags flags);
		extern VkDescriptorSetLayoutCreateInfo DescriptorSetLayoutCreateInfo(VkDescriptorSetLayoutCreateFlags flags, uint32_t bindingCount, const VkDescriptorSetLayoutBinding* binding);
		extern VkPipelineLayoutCreateInfo PipelineLayoutCreateInfo(VkPipelineLayoutCreateFlags flags, uint32_t layoutCount, const VkDescriptorSetLayout* layout);
		extern VkDescriptorPoolCreateInfo DescriptorPoolCreateInfo(VkDescriptorPoolCreateFlags flags, uint32_t maxSets, uint32_t poolSizeCount, const VkDescriptorPoolSize* poolSizes);
		extern VkDescriptorSetAllocateInfo DescriptorSetAllocateInfo(VkDescriptorPool descriptorPool, uint32_t descriptorSetCount, const VkDescriptorSetLayout* setLayouts);
		extern VkPushConstantRange PushConstantRange(VkShaderStageFlags flags, uint32_t offset, uint32_t size);
		extern VkVertexInputAttributeDescription VertexInputAttributeDescription(uint32_t binding, uint32_t location, VkFormat format, uint32_t offset);
		extern VkPipelineVertexInputStateCreateInfo PipelineVertexInputStateCreateInfo();
		extern VkPipelineInputAssemblyStateCreateInfo PipelineInputAssemblyStateCreateInfo(VkPrimitiveTopology topology, VkPipelineInputAssemblyStateCreateFlags flags, VkBool32 primitiveRestartEnable);
		extern VkPipelineRasterizationStateCreateInfo PipelineRasterizationStateCreateInfo(VkPolygonMode polygonMode, VkCullModeFlags cullMode, VkFrontFace frontFace, VkPipelineRasterizationStateCreateFlags flags);
		extern VkPipelineColorBlendAttachmentState PipelineColorBlendAttachmentState(VkColorComponentFlags colorWriteMask, VkBool32 blendEnable);
		extern VkPipelineColorBlendStateCreateInfo PipelineColorBlendStateCreateInfo(uint32_t attachmentCount, const VkPipelineColorBlendAttachmentState * pAttachments);
		extern VkPipelineDepthStencilStateCreateInfo PipelineDepthStencilStateCreateInfo(VkBool32 depthTestEnable, VkBool32 depthWriteEnable, VkCompareOp depthCompareOp);
		extern VkPipelineViewportStateCreateInfo PipelineViewportStateCreateInfo(uint32_t viewportCount, uint32_t scissorCount, VkPipelineViewportStateCreateFlags flags);
		extern VkPipelineDynamicStateCreateInfo PipelineDynamicStateCreateInfo(const VkDynamicState * pDynamicStates, uint32_t dynamicStateCount, VkPipelineDynamicStateCreateFlags flags);
		extern VkPipelineMultisampleStateCreateInfo PipelineMultisampleStateCreateInfo(VkSampleCountFlagBits rasterizationSamples, VkPipelineMultisampleStateCreateFlags flags);
		extern VkDescriptorImageInfo DescriptorImageInfo(VkSampler sampler, VkImageView imageView, VkImageLayout imageLayout);
		extern VkWriteDescriptorSet WriteDescriptorSet(VkDescriptorSet dstSet, VkDescriptorType type, uint32_t binding, VkDescriptorBufferInfo * bufferInfo);
		extern VkWriteDescriptorSet WriteDescriptorSet(VkDescriptorSet dstSet, VkDescriptorType type, uint32_t binding, VkDescriptorImageInfo * imageInfo);
		extern VkBufferMemoryBarrier BufferMemoryBarrier();
	}
}

#endif	//VKTOOLS_H