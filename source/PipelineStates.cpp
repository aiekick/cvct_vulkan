#include "VulkanCore.h"

void DestroyRenderStates(RenderState& rs, VulkanCore* core, VkCommandPool commandpool)
{
	VkDevice device = core->GetViewDevice();;

	// Delete pipeline layout
	if (rs.m_pipelineLayout)
	{
		vkDestroyPipelineLayout(device, rs.m_pipelineLayout, NULL);
		rs.m_pipelineLayout = VK_NULL_HANDLE;
	}
	// Delete descriptorlayouts
	if (rs.m_descriptorLayouts)
	{
		for (uint32_t i = 0; i < rs.m_descriptorLayoutCount; i++)
			vkDestroyDescriptorSetLayout(device, rs.m_descriptorLayouts[i], NULL);

		free(rs.m_descriptorLayouts);
		rs.m_descriptorLayouts = NULL;
	}
	// Delete renderpass
	if (rs.m_renderpass != VK_NULL_HANDLE)
	{
		vkDestroyRenderPass(device, rs.m_renderpass, NULL);
		rs.m_renderpass = VK_NULL_HANDLE;
	}
	// Delete descriptorsets
	if (rs.m_descriptorSets)
	{
		for (uint32_t i = 0; i < rs.m_descriptorSetCount; i++)
		{
			//	vkFreeDescriptorSets(device, rs.m_descriptorPool, 1, &rs.m_descriptorSets[i]);
		}
		free(rs.m_descriptorSets);
		rs.m_descriptorSets = NULL;
	}
	// Delete pipeline
	if (rs.m_pipelines)
	{
		for (uint32_t i = 0; i < rs.m_pipelineCount; i++)
		{
			vkDestroyPipeline(device, rs.m_pipelines[i], NULL);
		}
		rs.m_pipelines = NULL;
	}
	// Delete pipeline Cache
	if (rs.m_pipelineCache != VK_NULL_HANDLE)
	{
		vkDestroyPipelineCache(device, rs.m_pipelineCache, NULL);
		rs.m_pipelineCache = VK_NULL_HANDLE;
	}
	// delete commandbuffers
	if (rs.m_commandBuffers)
	{
		for (uint32_t i = 0; i < rs.m_commandBufferCount; i++)
		{
			vkFreeCommandBuffers(device, commandpool, 1, &rs.m_commandBuffers[i]);
		}
		free(rs.m_commandBuffers);
		rs.m_commandBuffers = NULL;
	}
	// delete semaphores
	if (rs.m_semaphores)
	{
		for (uint32_t i = 0; i < rs.m_semaphoreCount; i++)
			vkDestroySemaphore(device, rs.m_semaphores[i], NULL);

		free(rs.m_semaphores);
		rs.m_semaphores = NULL;
	}
	// Delete uniform data
	if (rs.m_uniformData)
	{
		for (uint32_t i = 0; i < rs.m_uniformDataCount; i++)
		{
			vkDestroyBuffer(device, rs.m_uniformData[i].m_buffer, NULL);
			vkFreeMemory(device, rs.m_uniformData[i].m_memory, NULL);
		}
		free(rs.m_uniformData);
		rs.m_uniformData = NULL;
	}
	// Delete the Descriptor Pool
	if (rs.m_descriptorPool != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorPool(device, rs.m_descriptorPool, NULL);
		rs.m_descriptorPool = VK_NULL_HANDLE;
	}
	// Delete framebuffers
	if (rs.m_framebuffers)
	{
		for (uint32_t i = 0; i < rs.m_framebufferCount; i++)
			vkDestroyFramebuffer(device, rs.m_framebuffers[i], NULL);

		free(rs.m_framebuffers);
		rs.m_framebuffers = NULL;
	}
	// delete cmdbuffer parameters
	if (rs.m_cmdBufferParameters)
	{
		free(rs.m_cmdBufferParameters);
		rs.m_cmdBufferParameters = NULL;
	}
	// delete framebufferattatchments
	if (rs.m_framebufferAttatchments)
	{
		for (uint32_t i = 0; i < rs.m_framebufferAttatchmentCount; i++)
		{
			vkFreeMemory(device, rs.m_framebufferAttatchments[i].mem, NULL);
			vkDestroyImage(device, rs.m_framebufferAttatchments[i].image, NULL);
			vkDestroyImageView(device, rs.m_framebufferAttatchments[i].view, NULL);
			rs.m_framebufferAttatchments[0].format = VK_FORMAT_UNDEFINED;
		}
		free(rs.m_framebufferAttatchments);
		rs.m_framebufferAttatchments = NULL;
	}
	if (rs.m_queryPool != VK_NULL_HANDLE)
	{
		vkDestroyQueryPool(device, rs.m_queryPool, NULL);
		rs.m_queryPool = VK_NULL_HANDLE;
		free(rs.m_queryResults);
	}
	rs.m_framebufferCount = 0;
	rs.m_uniformDataCount = 0;
	rs.m_semaphoreCount = 0;
	rs.m_commandBufferCount = 0;
	rs.m_descriptorSetCount = 0;
	rs.m_descriptorLayoutCount = 0;
	rs.m_framebufferAttatchmentCount = 0;
	rs.m_pipelineCount = 0;
	rs.m_queryCount = 0;
}

void DestroyCommandBuffer(RenderState& rs, VulkanCore* core, VkCommandPool commandpool)
{
	// delete commandbuffers
	if (rs.m_commandBuffers)
	{
		for (uint32_t i = 0; i < rs.m_commandBufferCount; i++)
		{
			vkFreeCommandBuffers(core->GetViewDevice(), commandpool, 1, &rs.m_commandBuffers[i]);
		}
		free(rs.m_commandBuffers);
		rs.m_commandBuffers = NULL;
	}
	rs.m_commandBufferCount = 0;
}

void BuildCommandBuffer(RenderState& rs, VkCommandPool commandpool, VulkanCore* core, uint32_t framebufferCount, VkFramebuffer* framebuffers)
{
	if (rs.m_CreateCommandBufferFunc)
	{
		rs.m_CreateCommandBufferFunc(&rs, commandpool, core, framebufferCount, framebuffers, rs.m_cmdBufferParameters);
	}
}