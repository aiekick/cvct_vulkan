#include "PipelineStates.h"

#include "VCTPipelineDefines.h"
#include "SwapChain.h"
#include "VulkanCore.h"
#include "Shader.h"
#include "DataTypes.h"
#include "AnisotropicVoxelTexture.h"

struct Parameter
{
	SwapChain* swapchain;
};

void BuildCommandBufferConeTracerState(
	RenderState* renderstate,
	VkCommandPool commandpool,
	VulkanCore* core,
	uint32_t framebufferCount,
	VkFramebuffer* framebuffers,
	BYTE* parameters)
{
	uint32_t width = core->GetSwapChain()->m_width;
	uint32_t height = core->GetSwapChain()->m_height;
	RenderState* renderState = renderstate;
	VkDevice device = core->GetViewDevice();

	////////////////////////////////////////////////////////////////////////////////
	// Rebuild the parameters
	////////////////////////////////////////////////////////////////////////////////
	SwapChain* swapchain = ((Parameter*)parameters)->swapchain;

	////////////////////////////////////////////////////////////////////////////////
	// Rebuild the command buffers
	////////////////////////////////////////////////////////////////////////////////
	renderState->m_commandBufferCount = framebufferCount;
	renderState->m_commandBuffers = (VkCommandBuffer*)malloc(sizeof(VkCommandBuffer)*renderState->m_commandBufferCount);
	for (uint32_t i = 0; i < renderState->m_commandBufferCount; i++)
		renderState->m_commandBuffers[i] = VKTools::Initializers::CreateCommandBuffer(commandpool, device, VK_COMMAND_BUFFER_LEVEL_PRIMARY, false);
	
	////////////////////////////////////////////////////////////////////////////////
	// Record command buffer
	////////////////////////////////////////////////////////////////////////////////
	VkCommandBufferBeginInfo cmdBufInfo = {};
	cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBufInfo.pNext = NULL;

	VkImageSubresourceRange srRange = {};
	srRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	srRange.baseMipLevel = 0;
	srRange.levelCount = 1;
	srRange.baseArrayLayer = 0;
	srRange.layerCount = 1;

	for (uint32_t i = 0; i < renderState->m_commandBufferCount; i++)
	{
		VK_CHECK_RESULT(vkBeginCommandBuffer(renderState->m_commandBuffers[i], &cmdBufInfo));

		vkCmdResetQueryPool(renderState->m_commandBuffers[i], renderState->m_queryPool, 0, 4);
		vkCmdWriteTimestamp(renderState->m_commandBuffers[i], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, renderState->m_queryPool, 0);

		// Set image layout to write
		VKTools::SetImageLayout(
			renderState->m_commandBuffers[i],
			swapchain->m_images[i],
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_GENERAL,
			srRange);

		vkCmdBindPipeline(renderState->m_commandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, renderState->m_pipelines[0]);
		vkCmdBindDescriptorSets(renderState->m_commandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, renderState->m_pipelineLayout, 0, 1, &renderState->m_descriptorSets[i], 0, 0);

		uint32_t dispatchX = (uint32_t)glm::ceil((float)swapchain->m_width / 32.0f);
		uint32_t dispatchY = (uint32_t)glm::ceil((float)swapchain->m_height / 32.0f);
		vkCmdDispatch(renderState->m_commandBuffers[i], dispatchX, dispatchY, 1);

		// Set image layout to read
		VKTools::SetImageLayout(
			renderState->m_commandBuffers[i],
			swapchain->m_images[i],
			VK_IMAGE_LAYOUT_GENERAL,
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			srRange);

		vkCmdWriteTimestamp(renderState->m_commandBuffers[i], VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, renderState->m_queryPool, 1);

		vkEndCommandBuffer(renderState->m_commandBuffers[i]);
	}
}

void CreateConeTraceState(
	RenderState& renderState,
	uint32_t framebufferCount,
	VulkanCore* core,
	VkCommandPool commandPool,
	SwapChain* swapchain,
	AnisotropicVoxelTexture* avts
)
{
	uint32_t width = swapchain->m_width;
	uint32_t height = swapchain->m_height;
	VkDevice device = core->GetViewDevice();

	////////////////////////////////////////////////////////////////////////////////
	// Create queries
	////////////////////////////////////////////////////////////////////////////////
	renderState.m_queryCount = 4;
	renderState.m_queryResults = (uint64_t*)malloc(sizeof(uint64_t)*renderState.m_queryCount);
	memset(renderState.m_queryResults, 0, sizeof(uint64_t)*renderState.m_queryCount);
	// Create query pool
	VkQueryPoolCreateInfo queryPoolInfo = {};
	queryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
	queryPoolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
	queryPoolInfo.queryCount = renderState.m_queryCount;
	VK_CHECK_RESULT(vkCreateQueryPool(device, &queryPoolInfo, NULL, &renderState.m_queryPool));

	////////////////////////////////////////////////////////////////////////////////
	// Create the pipelineCache
	////////////////////////////////////////////////////////////////////////////////
	if (renderState.m_pipelineCache == VK_NULL_HANDLE)
	{
		// create a default pipelinecache
		VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
		pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
		VK_CHECK_RESULT(vkCreatePipelineCache(device, &pipelineCacheCreateInfo, NULL, &renderState.m_pipelineCache));
	}
	
	////////////////////////////////////////////////////////////////////////////////
	// set framebuffers
	////////////////////////////////////////////////////////////////////////////////
	renderState.m_framebufferCount = 0;
	renderState.m_framebuffers = NULL;

	////////////////////////////////////////////////////////////////////////////////
	// Create semaphores
	////////////////////////////////////////////////////////////////////////////////
	if (!renderState.m_semaphores)
	{
		renderState.m_semaphoreCount = 1;
		renderState.m_semaphores = (VkSemaphore*)malloc(sizeof(VkSemaphore)*renderState.m_semaphoreCount);
		VkSemaphoreCreateInfo semInfo = VKTools::Initializers::SemaphoreCreateInfo();
		for (uint32_t i = 0; i < renderState.m_semaphoreCount; i++)
			vkCreateSemaphore(device, &semInfo, NULL, &renderState.m_semaphores[i]);
	}

	////////////////////////////////////////////////////////////////////////////////
	// Create the Uniform Data
	////////////////////////////////////////////////////////////////////////////////
	if (!renderState.m_uniformData)
	{
		renderState.m_uniformDataCount = 1;
		renderState.m_uniformData = (UniformData*)malloc(sizeof(UniformData) * renderState.m_uniformDataCount);
		VKTools::CreateBuffer(core, device,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			sizeof(ConeTracerUBOComp),
			NULL,
			&renderState.m_uniformData[0].m_buffer,
			&renderState.m_uniformData[0].m_memory,
			&renderState.m_uniformData[0].m_descriptor);
	}
	
	////////////////////////////////////////////////////////////////////////////////
	// Create the descriptorlayout
	////////////////////////////////////////////////////////////////////////////////
	if (!renderState.m_descriptorLayouts)
	{
		renderState.m_descriptorLayoutCount = 1;
		renderState.m_descriptorLayouts = (VkDescriptorSetLayout*)malloc(sizeof(VkDescriptorSetLayout)*renderState.m_descriptorLayoutCount);
		//dynamic descriptorset
		VkDescriptorSetLayoutBinding layoutBinding[FORWARDRENDER_DESCRIPTOR_COUNT];
		// Binding 0 : diffuse texture sampled image
		layoutBinding[CONETRACER_DESCRIPTOR_VOXELGRID] =
		{ CONETRACER_DESCRIPTOR_VOXELGRID, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL };
		// Binding 1 : uniform buffer
		layoutBinding[CONETRACER_DESCRIPTOR_BUFFER_COMP] =
		{ CONETRACER_DESCRIPTOR_BUFFER_COMP, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL };
		// Binding 2 : framebuffer image to write
		layoutBinding[CONETRACER_DESCRIPTOR_FRAMEBUFFER] =
		{ CONETRACER_DESCRIPTOR_FRAMEBUFFER, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL };
		// set the createinfo
		VkDescriptorSetLayoutCreateInfo descriptorLayout = VKTools::Initializers::DescriptorSetLayoutCreateInfo(0, CONETRACER_DESCRIPTOR_COUNT, layoutBinding);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout, NULL, &renderState.m_descriptorLayouts[0]));
	}

	////////////////////////////////////////////////////////////////////////////////
	// Create pipeline layout
	////////////////////////////////////////////////////////////////////////////////
	if (!renderState.m_pipelineLayout)
	{
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = VKTools::Initializers::PipelineLayoutCreateInfo(0, 1, &renderState.m_descriptorLayouts[0]);
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, NULL, &renderState.m_pipelineLayout));
	}

	////////////////////////////////////////////////////////////////////////////////
	// Create descriptor pool
	////////////////////////////////////////////////////////////////////////////////
	if (!renderState.m_descriptorPool)
	{
		VkDescriptorPoolSize poolSize[CONETRACER_DESCRIPTOR_COUNT];
		poolSize[MIPMAPPER_DESCRIPTOR_VOXELGRID] = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 * framebufferCount };
		poolSize[MIPMAPPER_DESCRIPTOR_IMAGE_VOXELGRID] = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 * framebufferCount };
		poolSize[CONETRACER_DESCRIPTOR_FRAMEBUFFER] = { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 * framebufferCount };
		VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = VKTools::Initializers::DescriptorPoolCreateInfo(0, framebufferCount, CONETRACER_DESCRIPTOR_COUNT, poolSize);
		//create the descriptorPool
		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, NULL, &renderState.m_descriptorPool));
	}

	////////////////////////////////////////////////////////////////////////////////
	// Create the descriptor set
	////////////////////////////////////////////////////////////////////////////////
	if (!renderState.m_descriptorSets)
	{
		renderState.m_descriptorSetCount = framebufferCount;
		renderState.m_descriptorSets = (VkDescriptorSet*)malloc(renderState.m_descriptorSetCount * sizeof(VkDescriptorSet));
		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = VKTools::Initializers::DescriptorSetAllocateInfo(renderState.m_descriptorPool, 1, &renderState.m_descriptorLayouts[0]);
		for(uint32_t i = 0; i < framebufferCount;i++)
			VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &renderState.m_descriptorSets[i]));
	
		///////////////////////////////////////////////////////
		///// Set/Update the image and uniform buffer descriptorsets
		/////////////////////////////////////////////////////// 
		VkDescriptorImageInfo descriptors[1];
		//for (uint32_t i = 0; i < VoxelDirections::NUM_DIRECTIONS; i++)
		{
			descriptors[0].imageLayout = avts[0].m_imageLayout;
			descriptors[0].imageView = avts[0].m_descriptor[0].imageView;
			descriptors[0].sampler = avts[0].m_conetraceSampler;
		}

		for (uint32_t i = 0; i < framebufferCount; i++)
		{
			// Bind the 3D voxel textures
			{
				VkWriteDescriptorSet wds = {};
				wds.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				wds.pNext = NULL;
				wds.dstSet = renderState.m_descriptorSets[i];
				wds.dstBinding = CONETRACER_DESCRIPTOR_VOXELGRID;
				wds.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				wds.descriptorCount = 1;
				wds.dstArrayElement = 0;
				wds.pImageInfo = descriptors;
				//update the descriptorset
				vkUpdateDescriptorSets(device, 1, &wds, 0, NULL);
			}
			// Bind the Uniform buffer
			{
				VkWriteDescriptorSet wds = {};
				wds.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				wds.pNext = NULL;
				wds.dstSet = renderState.m_descriptorSets[i];
				wds.dstBinding = CONETRACER_DESCRIPTOR_BUFFER_COMP;
				wds.dstArrayElement = 0;
				wds.descriptorCount = 1;
				wds.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				wds.pImageInfo = NULL;
				wds.pBufferInfo = &renderState.m_uniformData[0].m_descriptor;
				wds.pTexelBufferView = NULL;
				vkUpdateDescriptorSets(device, 1, &wds, 0, NULL);
			}
			// Bind framebuffer image
			{
				VkWriteDescriptorSet wds = {};
				wds.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				wds.pNext = NULL;
				wds.dstSet = renderState.m_descriptorSets[i];
				wds.dstBinding = CONETRACER_DESCRIPTOR_FRAMEBUFFER;
				wds.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
				wds.descriptorCount = 1;
				wds.dstArrayElement = 0;
				wds.pImageInfo = &swapchain->m_descriptors[i];
				//update the descriptorset
				vkUpdateDescriptorSets(device, 1, &wds, 0, NULL);
			}
		}
	}
	

	///////////////////////////////////////////////////////
	///// Create the compute pipeline
	/////////////////////////////////////////////////////// 
	if (!renderState.m_pipelines)
	{
		renderState.m_pipelineCount = 1;
		renderState.m_pipelines = (VkPipeline*)malloc(renderState.m_pipelineCount * sizeof(VkPipeline));

		// Create pipeline		
		VkComputePipelineCreateInfo computePipelineCreateInfo = VKTools::Initializers::ComputePipelineCreateInfo(renderState.m_pipelineLayout, VK_FLAGS_NONE);
		// Shaders are loaded from the SPIR-V format, which can be generated from glsl
		Shader shaderStage;
		shaderStage = VKTools::LoadShader("shaders/conetrace.comp.spv", "main", device, VK_SHADER_STAGE_COMPUTE_BIT);
		computePipelineCreateInfo.stage = shaderStage.m_shaderStage;

		VK_CHECK_RESULT(vkCreateComputePipelines(device, renderState.m_pipelineCache, 1, &computePipelineCreateInfo, NULL, &renderState.m_pipelines[0]));
	}

	////////////////////////////////////////////////////////////////////////////////
	// Build command buffers
	////////////////////////////////////////////////////////////////////////////////
	Parameter* parameter;
	parameter = (Parameter*)malloc(sizeof(Parameter));
	parameter->swapchain = swapchain;
	renderState.m_cmdBufferParameters = (BYTE*)parameter;

	renderState.m_CreateCommandBufferFunc = &BuildCommandBufferConeTracerState;
	renderState.m_CreateCommandBufferFunc(&renderState, commandPool, core, framebufferCount, NULL, renderState.m_cmdBufferParameters);
}