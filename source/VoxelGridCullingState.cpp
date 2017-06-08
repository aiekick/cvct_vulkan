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
	glm::vec3 gridposition;	
	uint32_t meshCount;
	glm::vec3 gridsize;		
	float padding0;
};

void CommandGridCulling(
	RenderState& renderstate,
	VulkanCore* core,
	uint32_t meshcount,
	glm::vec3 gridPosition,
	glm::vec3 gridSize)
{
	uint32_t width = core->GetSwapChain()->m_width;
	uint32_t height = core->GetSwapChain()->m_height;
	RenderState& renderState = renderstate;

	////////////////////////////////////////////////////////////////////////////////
	// Rebuild the command buffers
	////////////////////////////////////////////////////////////////////////////////
	renderState.m_commandBufferCount = 1;
	renderState.m_commandBuffers = (VkCommandBuffer*)malloc(sizeof(VkCommandBuffer)*renderState.m_commandBufferCount);
	for (uint32_t i = 0; i < renderState.m_commandBufferCount; i++)
		renderState.m_commandBuffers[i] = VKTools::Initializers::CreateCommandBuffer(core->GetComputeCommandPool(), core->GetViewDevice(), VK_COMMAND_BUFFER_LEVEL_PRIMARY, false);

	////////////////////////////////////////////////////////////////////////////////
	// Record command buffer
	////////////////////////////////////////////////////////////////////////////////
	VkCommandBufferBeginInfo cmdBufInfo = {};
	cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBufInfo.pNext = NULL;

	for (uint32_t i = 0; i < renderState.m_commandBufferCount; i++)
	{
		VK_CHECK_RESULT(vkBeginCommandBuffer(renderState.m_commandBuffers[i], &cmdBufInfo));

		vkCmdResetQueryPool(renderState.m_commandBuffers[i], renderState.m_queryPool, 0, 4);
		vkCmdWriteTimestamp(renderState.m_commandBuffers[i], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, renderState.m_queryPool, 0);

		// Submit push constant
		PushConstantComp pc;
		pc.meshCount = meshcount;
		pc.gridposition = gridPosition;
		pc.gridsize = gridSize;
		vkCmdPushConstants(renderState.m_commandBuffers[i], renderState.m_pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantComp), &pc);

		// Add memory barrier to ensure that the indirect commands have been consumed before the compute shader updates them
		VkBufferMemoryBarrier bufferBarrier = VKTools::Initializers::BufferMemoryBarrier();
		bufferBarrier.buffer = renderstate.m_bufferData[1].buffer;
		bufferBarrier.size = renderstate.m_bufferData[1].descriptor.range;
		bufferBarrier.srcAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
		bufferBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		bufferBarrier.srcQueueFamilyIndex = core->GetDeviceQueueIndices().graphics;
		bufferBarrier.dstQueueFamilyIndex = core->GetDeviceQueueIndices().compute;
		vkCmdPipelineBarrier(
			renderState.m_commandBuffers[i],
			VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_FLAGS_NONE,
			0, nullptr,
			1, &bufferBarrier,
			0, nullptr);
		bufferBarrier.buffer = renderstate.m_bufferData[3].buffer;
		bufferBarrier.size = renderstate.m_bufferData[3].descriptor.range;
		vkCmdPipelineBarrier(
			renderState.m_commandBuffers[i],
			VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_FLAGS_NONE,
			0, nullptr,
			1, &bufferBarrier,
			0, nullptr);
		bufferBarrier.buffer = renderstate.m_bufferData[4].buffer;
		bufferBarrier.size = renderstate.m_bufferData[4].descriptor.range;
		vkCmdPipelineBarrier(
			renderState.m_commandBuffers[i],
			VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_FLAGS_NONE,
			0, nullptr,
			1, &bufferBarrier,
			0, nullptr);

		vkCmdBindPipeline(renderState.m_commandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, renderState.m_pipelines[0]);
		vkCmdBindDescriptorSets(renderState.m_commandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, renderState.m_pipelineLayout, 0, 1, &renderState.m_descriptorSets[0], 0, 0);

		uint32_t numdis = glm::ceil((float)meshcount / 16.0f);
		vkCmdDispatch(renderState.m_commandBuffers[i], numdis, 1, 1);

		// Add memory barrier to ensure that the compute shader has finished writing the indirect command buffer before it's consumed
		bufferBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		bufferBarrier.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
		bufferBarrier.buffer = renderstate.m_bufferData[1].buffer;
		bufferBarrier.size = renderstate.m_bufferData[1].descriptor.range;
		bufferBarrier.srcQueueFamilyIndex = core->GetDeviceQueueIndices().compute;
		bufferBarrier.dstQueueFamilyIndex = core->GetDeviceQueueIndices().graphics;
		vkCmdPipelineBarrier(
			renderState.m_commandBuffers[i],
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
			VK_FLAGS_NONE,
			0, nullptr,
			1, &bufferBarrier,
			0, nullptr);
		bufferBarrier.buffer = renderstate.m_bufferData[3].buffer;
		bufferBarrier.size = renderstate.m_bufferData[3].descriptor.range;
		vkCmdPipelineBarrier(
			renderState.m_commandBuffers[i],
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
			VK_FLAGS_NONE,
			0, nullptr,
			1, &bufferBarrier,
			0, nullptr);
		bufferBarrier.buffer = renderstate.m_bufferData[4].buffer;
		bufferBarrier.size = renderstate.m_bufferData[4].descriptor.range;
		vkCmdPipelineBarrier(
			renderState.m_commandBuffers[i],
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
			VK_FLAGS_NONE,
			0, nullptr,
			1, &bufferBarrier,
			0, nullptr);

		vkCmdWriteTimestamp(renderState.m_commandBuffers[i], VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, renderState.m_queryPool, 1);

		vkEndCommandBuffer(renderState.m_commandBuffers[i]);
	}
}

void StateGridCulling(
	RenderState& renderState,
	VulkanCore* core,
	InstanceDataAABB* aabb,
	uint32_t submeshcount,
	InstanceStatistic& statistic,
	VkDrawIndexedIndirectCommand* indirectbuffer,
	uint32_t debugboxcount)
{
	VkDevice device = core->GetViewDevice();
	CreateBasicRenderstate(renderState, core, 4, 0, 5, 1, true, 1, 1, 1);

	////////////////////////////////////////////////////////////////////////////////
	// Create the Uniform Data
	////////////////////////////////////////////////////////////////////////////////
	BufferObject stagingbuffer;
	// need staging
	uint32_t size = sizeof(InstanceDataAABB) * submeshcount;
	VKTools::CreateBuffer(core, device,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		size,
		aabb,
		&stagingbuffer.buffer,
		&stagingbuffer.memory,
		&stagingbuffer.descriptor);

	// instance data: binding 0
	VKTools::CreateBuffer(core, device,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		size,
		NULL,
		&renderState.m_bufferData[0].buffer,
		&renderState.m_bufferData[0].memory,
		&renderState.m_bufferData[0].descriptor);
	// Copy buffer
	VKTools::CopyBuffer(core, device, core->GetComputeCommandPool(), core->GetComputeQueue(), stagingbuffer.buffer, renderState.m_bufferData[0].buffer, sizeof(InstanceDataAABB) * submeshcount);
	// Clean
	vkDestroyBuffer(device, stagingbuffer.buffer, NULL);
	vkFreeMemory(device, stagingbuffer.memory, NULL);

	// indirect draw: binding 1
	VKTools::CreateBuffer(core, device,
		VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		//VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		//todo, make this device local
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		sizeof(VkDrawIndexedIndirectCommand) * submeshcount,
		indirectbuffer,
		&renderState.m_bufferData[1].buffer,
		&renderState.m_bufferData[1].memory,
		&renderState.m_bufferData[1].descriptor);

	// indirect draw: binding 2
	VKTools::CreateBuffer(core, device,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		sizeof(InstanceStatistic),
		&statistic,
		&renderState.m_bufferData[2].buffer,
		&renderState.m_bufferData[2].memory,
		&renderState.m_bufferData[2].descriptor);

	// indirect draw: binding 3
	VKTools::CreateBuffer(core, device,
		VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		sizeof(VkDrawIndirectCommand),
		NULL,
		&renderState.m_bufferData[3].buffer,
		&renderState.m_bufferData[3].memory,
		&renderState.m_bufferData[3].descriptor);

	// vertex buffer: binding 4
	VKTools::CreateBuffer(core, device,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		sizeof(DebugBox) * debugboxcount,
		NULL,
		&renderState.m_bufferData[4].buffer,
		&renderState.m_bufferData[4].memory,
		&renderState.m_bufferData[4].descriptor);

	////////////////////////////////////////////////////////////////////////////////
	// Set the descriptorset layout
	////////////////////////////////////////////////////////////////////////////////
	VkDescriptorSetLayoutBinding layoutBinding[GRIDCULLING_DESCRIPTOR_COUNT];
	// Binding 0 : Diffuse texture sampled image
	layoutBinding[GRIDCULLING_DESCRIPTOR_INSTANCE_DATA] =
	{ GRIDCULLING_DESCRIPTOR_INSTANCE_DATA, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL };
	layoutBinding[GRIDCULLING_DESCRIPTOR_INSTANCE_DRAW] =
	{ GRIDCULLING_DESCRIPTOR_INSTANCE_DRAW, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL };
	layoutBinding[GRIDCULLING_DESCRIPTOR_STATISTICS] =
	{ GRIDCULLING_DESCRIPTOR_STATISTICS, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL };
	layoutBinding[GRIDCULLING_DESCRIPTOR_INSTANCE_DRAW2] =
	{ GRIDCULLING_DESCRIPTOR_INSTANCE_DRAW2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL };
	layoutBinding[GRIDCULLING_DESCRIPTOR_DEBUG_DATA] =
	{ GRIDCULLING_DESCRIPTOR_DEBUG_DATA, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL };
	// Create the descriptorlayout
	VkDescriptorSetLayoutCreateInfo descriptorLayout = VKTools::Initializers::DescriptorSetLayoutCreateInfo(0, GRIDCULLING_DESCRIPTOR_COUNT, layoutBinding);
	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout, NULL, &renderState.m_descriptorLayouts[0]));

	////////////////////////////////////////////////////////////////////////////////
	// Create pipeline layout
	////////////////////////////////////////////////////////////////////////////////
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = VKTools::Initializers::PipelineLayoutCreateInfo(0, 1, &renderState.m_descriptorLayouts[0]);
	VkPushConstantRange pushConstantRange[1]; 
	pushConstantRange[0] = VKTools::Initializers::PushConstantRange(VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantComp));
	pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
	pipelineLayoutCreateInfo.pPushConstantRanges = pushConstantRange;
	VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, NULL, &renderState.m_pipelineLayout));

	////////////////////////////////////////////////////////////////////////////////
	// Create descriptor pool
	////////////////////////////////////////////////////////////////////////////////
	VkDescriptorPoolSize poolSize[GRIDCULLING_DESCRIPTOR_COUNT];
	poolSize[GRIDCULLING_DESCRIPTOR_INSTANCE_DATA] = { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 };
	poolSize[GRIDCULLING_DESCRIPTOR_INSTANCE_DRAW] = { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 };
	poolSize[GRIDCULLING_DESCRIPTOR_STATISTICS] = { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 };
	poolSize[GRIDCULLING_DESCRIPTOR_INSTANCE_DRAW2] = { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 };
	poolSize[GRIDCULLING_DESCRIPTOR_DEBUG_DATA] = { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 };
	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = VKTools::Initializers::DescriptorPoolCreateInfo(0, 1, GRIDCULLING_DESCRIPTOR_COUNT, poolSize);
	//create the descriptorPool
	VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, NULL, &renderState.m_descriptorPool));

	////////////////////////////////////////////////////////////////////////////////
	// Create the descriptor set
	////////////////////////////////////////////////////////////////////////////////
	VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = VKTools::Initializers::DescriptorSetAllocateInfo(renderState.m_descriptorPool, 1, &renderState.m_descriptorLayouts[0]);
	//allocate the descriptorset with the pool
	VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &renderState.m_descriptorSets[0]));

	VkWriteDescriptorSet wds[5];
	wds[0] = VKTools::Initializers::WriteDescriptorSet(
		renderState.m_descriptorSets[0],
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		GRIDCULLING_DESCRIPTOR_INSTANCE_DATA,
		&renderState.m_bufferData[0].descriptor);

	wds[1] = VKTools::Initializers::WriteDescriptorSet(
		renderState.m_descriptorSets[0],
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		GRIDCULLING_DESCRIPTOR_INSTANCE_DRAW,
		&renderState.m_bufferData[1].descriptor);

	wds[2] = VKTools::Initializers::WriteDescriptorSet(
		renderState.m_descriptorSets[0],
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		GRIDCULLING_DESCRIPTOR_STATISTICS,
		&renderState.m_bufferData[2].descriptor);

	wds[3] = VKTools::Initializers::WriteDescriptorSet(
		renderState.m_descriptorSets[0],
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		GRIDCULLING_DESCRIPTOR_INSTANCE_DRAW2,
		&renderState.m_bufferData[3].descriptor);

	wds[4] = VKTools::Initializers::WriteDescriptorSet(
		renderState.m_descriptorSets[0],
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		GRIDCULLING_DESCRIPTOR_DEBUG_DATA,
		&renderState.m_bufferData[4].descriptor);

	vkUpdateDescriptorSets(device, 5, wds, 0, NULL);

	///////////////////////////////////////////////////////
	///// Create the compute pipeline
	///////////////////////////////////////////////////////
	// Create pipeline		
	VkComputePipelineCreateInfo computePipelineCreateInfo = VKTools::Initializers::ComputePipelineCreateInfo(renderState.m_pipelineLayout, VK_FLAGS_NONE);
	// Shaders are loaded from the SPIR-V format, which can be generated from glsl
	Shader shaderStage;
	shaderStage = VKTools::LoadShader("shaders/gridculling.comp.spv", "main", device, VK_SHADER_STAGE_COMPUTE_BIT);
	computePipelineCreateInfo.stage = shaderStage.m_shaderStage;

	VK_CHECK_RESULT(vkCreateComputePipelines(device, renderState.m_pipelineCache, 1, &computePipelineCreateInfo, NULL, &renderState.m_pipelines[0]));
}
