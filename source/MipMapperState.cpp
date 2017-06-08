#include "PipelineStates.h"

#include <glm/gtc/matrix_transform.hpp>
#include "VCTPipelineDefines.h"
#include "SwapChain.h"
#include "VulkanCore.h"
#include "Shader.h"
#include "DataTypes.h"
#include "AnisotropicVoxelTexture.h"
#include "Camera.h"

struct PushConstantComp
{
	uint32_t cascadeNum;		// Current cascade
};

struct Parameter
{
	AnisotropicVoxelTexture* avt;
};

void BuildCommandBufferMipMapperState(
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

	////////////////////////////////////////////////////////////////////////////////
	// Rebuild the parameters
	////////////////////////////////////////////////////////////////////////////////
	AnisotropicVoxelTexture* avt = ((Parameter*)parameters)->avt;

	////////////////////////////////////////////////////////////////////////////////
	// Rebuild the command buffers
	////////////////////////////////////////////////////////////////////////////////
	renderState->m_commandBufferCount = avt->m_cascadeCount;;
	renderState->m_commandBuffers = (VkCommandBuffer*)malloc(sizeof(VkCommandBuffer)*renderState->m_commandBufferCount);
	for (uint32_t i = 0; i < renderState->m_commandBufferCount; i++)
		renderState->m_commandBuffers[i] = VKTools::Initializers::CreateCommandBuffer(commandpool, core->GetViewDevice(), VK_COMMAND_BUFFER_LEVEL_PRIMARY, false);

	////////////////////////////////////////////////////////////////////////////////
	// Record command buffer
	////////////////////////////////////////////////////////////////////////////////
	VkCommandBufferBeginInfo cmdBufInfo = {};
	cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBufInfo.pNext = NULL;

	for (uint32_t i = 0; i < renderState->m_commandBufferCount; i++)
	{
		VK_CHECK_RESULT(vkBeginCommandBuffer(renderState->m_commandBuffers[i], &cmdBufInfo));

		vkCmdResetQueryPool(renderState->m_commandBuffers[i], renderState->m_queryPool, 0, 4);
		vkCmdWriteTimestamp(renderState->m_commandBuffers[i], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, renderState->m_queryPool, 0);

		// Submit push constant
		PushConstantComp pc;
		pc.cascadeNum = i;
		vkCmdPushConstants(renderState->m_commandBuffers[i], renderState->m_pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantComp), &pc);

		vkCmdBindPipeline(renderState->m_commandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, renderState->m_pipelines[0]);
		vkCmdBindDescriptorSets(renderState->m_commandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, renderState->m_pipelineLayout, 0, 1, &renderState->m_descriptorSets[0], 0, 0);

		uint32_t numdis = (uint32_t)(((float)avt->m_width * 0.5f) / 8.0);
		vkCmdDispatch(renderState->m_commandBuffers[i], numdis* VoxelDirections::NUM_DIRECTIONS, numdis, numdis);

		vkCmdWriteTimestamp(renderState->m_commandBuffers[i], VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, renderState->m_queryPool, 1);

		vkEndCommandBuffer(renderState->m_commandBuffers[i]);
	}
}

void CreateMipMapperState(
	RenderState& renderState,
	VulkanCore* core,
	VkCommandPool commandPool,
	VkDevice device,
	SwapChain* swapchain,
	AnisotropicVoxelTexture* avt)
{
	uint32_t width = swapchain->m_width;
	uint32_t height = swapchain->m_height;

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
	// create a default pipelinecache
	if (!renderState.m_pipelineCache)
	{
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
		renderState.m_semaphoreCount = avt->m_cascadeCount;
		renderState.m_semaphores = (VkSemaphore*)malloc(sizeof(VkSemaphore)*renderState.m_semaphoreCount);
		VkSemaphoreCreateInfo semInfo = VKTools::Initializers::SemaphoreCreateInfo();
		for (uint32_t i = 0; i < renderState.m_semaphoreCount; i++)
			vkCreateSemaphore(core->GetViewDevice(), &semInfo, NULL, &renderState.m_semaphores[i]);
	}

	////////////////////////////////////////////////////////////////////////////////
	// Create the Uniform Data
	////////////////////////////////////////////////////////////////////////////////
	if (!renderState.m_uniformData)
	{
		renderState.m_uniformDataCount = 1;
		renderState.m_uniformData = (UniformData*)malloc(sizeof(UniformData)*renderState.m_uniformDataCount);
		VKTools::CreateBuffer(core, device,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			sizeof(VoxelMipMapperUBOComp),
			NULL,
			&renderState.m_uniformData[0].m_buffer,
			&renderState.m_uniformData[0].m_memory,
			&renderState.m_uniformData[0].m_descriptor);
	}

	////////////////////////////////////////////////////////////////////////////////
	// Set the descriptorset layout
	////////////////////////////////////////////////////////////////////////////////
	if (!renderState.m_descriptorLayouts)
	{
		// Descriptorset
		renderState.m_descriptorLayoutCount = 1;
		renderState.m_descriptorLayouts = (VkDescriptorSetLayout*)malloc(renderState.m_descriptorLayoutCount * sizeof(VkDescriptorSetLayout));
		VkDescriptorSetLayoutBinding layoutBinding[MipMapperDescriptorLayout::MIPMAPPER_DESCRIPTOR_COUNT];
		// Binding 0 : Diffuse texture sampled image
		layoutBinding[MIPMAPPER_DESCRIPTOR_VOXELGRID] =
		{ MIPMAPPER_DESCRIPTOR_VOXELGRID, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL };
		layoutBinding[MIPMAPPER_DESCRIPTOR_IMAGE_VOXELGRID] =
		{ MIPMAPPER_DESCRIPTOR_IMAGE_VOXELGRID, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2, VK_SHADER_STAGE_COMPUTE_BIT, NULL };
		layoutBinding[MIPMAPPER_DESCRIPTOR_BUFFER_COMP] =
		{ MIPMAPPER_DESCRIPTOR_BUFFER_COMP, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL };
		// Create the descriptorlayout
		VkDescriptorSetLayoutCreateInfo descriptorLayout = VKTools::Initializers::DescriptorSetLayoutCreateInfo(0, MipMapperDescriptorLayout::MIPMAPPER_DESCRIPTOR_COUNT, layoutBinding);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout, NULL, &renderState.m_descriptorLayouts[0]));
	}

	////////////////////////////////////////////////////////////////////////////////
	// Create pipeline layout
	////////////////////////////////////////////////////////////////////////////////
	if (!renderState.m_pipelineLayout)
	{
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = VKTools::Initializers::PipelineLayoutCreateInfo(0, 1, &renderState.m_descriptorLayouts[0]);
		VkPushConstantRange pushConstantRange = VKTools::Initializers::PushConstantRange(VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantComp));
		pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
		pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, NULL, &renderState.m_pipelineLayout));
	}

	////////////////////////////////////////////////////////////////////////////////
	// Create descriptor pool
	////////////////////////////////////////////////////////////////////////////////
	if (!renderState.m_descriptorPool)
	{
		VkDescriptorPoolSize poolSize[MIPMAPPER_DESCRIPTOR_COUNT];
		poolSize[MIPMAPPER_DESCRIPTOR_VOXELGRID] = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1 };
		poolSize[MIPMAPPER_DESCRIPTOR_IMAGE_VOXELGRID] = { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,2 };
		poolSize[MIPMAPPER_DESCRIPTOR_BUFFER_COMP] = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 };
		VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = VKTools::Initializers::DescriptorPoolCreateInfo(0, 1, MIPMAPPER_DESCRIPTOR_COUNT, poolSize);
		//create the descriptorPool
		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, NULL, &renderState.m_descriptorPool));
	}

	////////////////////////////////////////////////////////////////////////////////
	// Create the descriptor set
	////////////////////////////////////////////////////////////////////////////////
	if (!renderState.m_descriptorSets)
	{
		renderState.m_descriptorSetCount = 1;
		renderState.m_descriptorSets = (VkDescriptorSet*)malloc(renderState.m_descriptorSetCount * sizeof(VkDescriptorSet));
		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = VKTools::Initializers::DescriptorSetAllocateInfo(renderState.m_descriptorPool, 1, &renderState.m_descriptorLayouts[0]);
		//allocate the descriptorset with the pool
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &renderState.m_descriptorSets[0]));

		///////////////////////////////////////////////////////
		///// Set/Update the image and uniform buffer descriptorsets
		///////////////////////////////////////////////////////
		VkWriteDescriptorSet wds = {};
		// Bind the 3D voxel textures
		{
			wds.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			wds.pNext = NULL;
			wds.dstSet = renderState.m_descriptorSets[0];
			wds.dstBinding = MIPMAPPER_DESCRIPTOR_VOXELGRID;
			wds.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			wds.descriptorCount = 1;
			wds.dstArrayElement = 0;
			wds.pImageInfo = avt->m_descriptor;
			//update the descriptorset
			vkUpdateDescriptorSets(device, 1, &wds, 0, NULL);
		}
		VkDescriptorImageInfo di[2];
		for (uint32_t i = 1; i < 3; i++)
		{
			di[i - 1] = avt->m_descriptor[i];
		}
		wds.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		wds.pNext = NULL;
		wds.dstSet = renderState.m_descriptorSets[0];
		wds.dstBinding = MIPMAPPER_DESCRIPTOR_IMAGE_VOXELGRID;
		wds.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		wds.descriptorCount = 2;
		wds.dstArrayElement = 0;
		wds.pImageInfo = di;
		//update the descriptorset
		vkUpdateDescriptorSets(device, 1, &wds, 0, NULL);

		wds.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		wds.pNext = NULL;
		wds.dstSet = renderState.m_descriptorSets[0];
		wds.dstBinding = MIPMAPPER_DESCRIPTOR_BUFFER_COMP;
		wds.dstArrayElement = 0;
		wds.descriptorCount = 1;
		wds.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		wds.pImageInfo = NULL;
		wds.pBufferInfo = &renderState.m_uniformData[0].m_descriptor;
		wds.pTexelBufferView = NULL;
		vkUpdateDescriptorSets(device, 1, &wds, 0, NULL);

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
		shaderStage = VKTools::LoadShader("shaders/voxelmipmapper.comp.spv", "main", device, VK_SHADER_STAGE_COMPUTE_BIT);
		computePipelineCreateInfo.stage = shaderStage.m_shaderStage;


		// TODO: Set constants, might be handy
		// Use specialization constants to pass max. level of detail (determined by no. of meshes)
	//	VkSpecializationMapEntry specializationEntry{};
	//	specializationEntry.constantID = 0;
	//	specializationEntry.offset = 0;
	//	specializationEntry.size = sizeof(uint32_t);
	//
	//	uint32_t specializationData = static_cast<uint32_t>(meshes.lodObject.meshDescriptors.size()) - 1;
	//
	//	VkSpecializationInfo specializationInfo;
	//	specializationInfo.mapEntryCount = 1;
	//	specializationInfo.pMapEntries = &specializationEntry;
	//	specializationInfo.dataSize = sizeof(specializationData);
	//	specializationInfo.pData = &specializationData;
	//
	//	computePipelineCreateInfo.stage.pSpecializationInfo = &specializationInfo;

		VK_CHECK_RESULT(vkCreateComputePipelines(device, renderState.m_pipelineCache, 1, &computePipelineCreateInfo, NULL, &renderState.m_pipelines[0]));
	}

	////////////////////////////////////////////////////////////////////////////////
	// Build command buffers
	////////////////////////////////////////////////////////////////////////////////
	Parameter* parameter;
	parameter = (Parameter*)malloc(sizeof(Parameter));
	parameter->avt = avt;
	renderState.m_cmdBufferParameters = (BYTE*)parameter;

	renderState.m_CreateCommandBufferFunc = &BuildCommandBufferMipMapperState;
	renderState.m_CreateCommandBufferFunc(&renderState, commandPool, core, 0, NULL, renderState.m_cmdBufferParameters);
}