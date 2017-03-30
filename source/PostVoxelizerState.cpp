#include "PipelineStates.h"

#include <glm/gtc/matrix_transform.hpp>
#include "VCTPipelineDefines.h"
#include "SwapChain.h"
#include "VulkanCore.h"
#include "Shader.h"
#include "DataTypes.h"
#include "AnisotropicVoxelTexture.h"
#include "Camera.h"
#include "Defines.h"

#define LOCAL_SIZE 8

struct SurfaceCount
{
	uint32_t surfacecount;
	uint32_t invokecount;
};

struct PushConstantComp
{
	glm::ivec3 gridresolution;	uint32_t cascadenum;
};

void CommandIndirectPostVoxelizer(
	RenderState& renderstate,
	VulkanCore* core,
	VoxelGrid* voxelgrid,
	glm::ivec3 gridresolution,
	uint32_t cascadenum)
{
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
		pc.gridresolution = gridresolution;
		pc.cascadenum = cascadenum;
		vkCmdPushConstants(renderState.m_commandBuffers[i], renderState.m_pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantComp), &pc);

		// Add memory barrier to ensure that the indirect commands have been consumed before the compute shader updates them
		VkBufferMemoryBarrier bufferBarrier = VKTools::Initializers::BufferMemoryBarrier();
		bufferBarrier.buffer = voxelgrid->surfacelist.buffer;
		bufferBarrier.size = voxelgrid->surfacelist.descriptor.range;
		bufferBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		bufferBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		bufferBarrier.srcQueueFamilyIndex = core->GetDeviceQueueIndices().compute;
		bufferBarrier.dstQueueFamilyIndex = core->GetDeviceQueueIndices().compute;
		vkCmdPipelineBarrier(
			renderState.m_commandBuffers[i],
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_FLAGS_NONE,
			0, nullptr,
			1, &bufferBarrier,
			0, nullptr);
		bufferBarrier.buffer = renderstate.m_bufferData[0].buffer;
		bufferBarrier.size = renderstate.m_bufferData[0].descriptor.range;
		vkCmdPipelineBarrier(
			renderState.m_commandBuffers[i],
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_FLAGS_NONE,
			0, nullptr,
			1, &bufferBarrier,
			0, nullptr);
		bufferBarrier.srcAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
		bufferBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		bufferBarrier.buffer = renderstate.m_bufferData[1].buffer;
		bufferBarrier.size = renderstate.m_bufferData[1].descriptor.range;
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

		glm::ivec3 numdis = (gridresolution / LOCAL_SIZE);
		vkCmdDispatch(renderState.m_commandBuffers[i], numdis.x, numdis.y, numdis.z);

		// Add memory barrier to ensure that the compute shader has finished writing the indirect command buffer before it's consumed
		bufferBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		bufferBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		bufferBarrier.buffer = voxelgrid->surfacelist.buffer;
		bufferBarrier.size = voxelgrid->surfacelist.descriptor.range;
		bufferBarrier.srcQueueFamilyIndex = core->GetDeviceQueueIndices().compute;
		bufferBarrier.dstQueueFamilyIndex = core->GetDeviceQueueIndices().compute;
		vkCmdPipelineBarrier(
			renderState.m_commandBuffers[i],
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_FLAGS_NONE,
			0, nullptr,
			1, &bufferBarrier,
			0, nullptr);
		bufferBarrier.buffer = renderstate.m_bufferData[0].buffer;
		bufferBarrier.size = renderstate.m_bufferData[0].descriptor.range;
		vkCmdPipelineBarrier(
			renderState.m_commandBuffers[i],
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_FLAGS_NONE,
			0, nullptr,
			1, &bufferBarrier,
			0, nullptr);
		bufferBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		bufferBarrier.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
		bufferBarrier.buffer = renderstate.m_bufferData[1].buffer;
		bufferBarrier.size = renderstate.m_bufferData[1].descriptor.range;
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

void StatePostVoxelizer(
	RenderState& renderState,
	VulkanCore* core,
	uint32_t uniformGridSize,
	VoxelizerGrid* voxelgrids,
	uint32_t voxelgridcount,
	VoxelGrid* voxelgrid,
	VkSampler sampler)
{
	VkDevice device = core->GetViewDevice();
	CreateBasicRenderstate(renderState, core, 4, 0, 2, 1, true, 1, 1, 1);

	////////////////////////////////////////////////////////////////////////////////
	// Create the Uniform Data
	////////////////////////////////////////////////////////////////////////////////
	// indirect draw: binding 5
	uint32_t size = sizeof(SurfaceCount);
	VKTools::CreateBuffer(core, device,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		size,
		NULL,
		&renderState.m_bufferData[0].buffer,
		&renderState.m_bufferData[0].memory,
		&renderState.m_bufferData[0].descriptor);
	// indirect draw: binding 6
	size = sizeof(VkDispatchIndirectCommand);
	VKTools::CreateBuffer(core, device,
		VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		size,
		NULL,
		&renderState.m_bufferData[1].buffer,
		&renderState.m_bufferData[1].memory,
		&renderState.m_bufferData[1].descriptor);

	////////////////////////////////////////////////////////////////////////////////
	// Set the descriptorset layout
	////////////////////////////////////////////////////////////////////////////////
	VkDescriptorSetLayoutBinding layoutBinding[PostVoxelizerDescriptorLayout::POSTVOXELIZERDESCRIPTOR_COUNT];
	// Binding 0 : diffuse grid
	layoutBinding[POSTVOXELIZER_DESCRIPTOR_VOXELGRID_DIFFUSE] =
	{ POSTVOXELIZER_DESCRIPTOR_VOXELGRID_DIFFUSE, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL };
	// Binding 1 : normal grid
	layoutBinding[POSTVOXELIZER_DESCRIPTOR_VOXELGRID_NORMAL] =
	{ POSTVOXELIZER_DESCRIPTOR_VOXELGRID_NORMAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL };
	// Binding 2 : emission grid
	layoutBinding[POSTVOXELIZER_DESCRIPTOR_VOXELGRID_EMISSION] =
	{ POSTVOXELIZER_DESCRIPTOR_VOXELGRID_EMISSION, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL };
	// Binding 3 : buffer position grid
	layoutBinding[POSTVOXELIZER_DESCRIPTOR_VOXELGRID_BUFFERPOSITION] =
	{ POSTVOXELIZER_DESCRIPTOR_VOXELGRID_BUFFERPOSITION, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL };
	// Binding 4 : buffer surface position
	layoutBinding[POSTVOXELIZER_DESCRIPTOR_BUFFER_SURFACEPOSITION] =
	{ POSTVOXELIZER_DESCRIPTOR_BUFFER_SURFACEPOSITION, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL };
	// Binding 5 : buffer surface count
	layoutBinding[POSTVOXELIZER_DESCRIPTOR_BUFFER_SURFACECOUNT] =
	{ POSTVOXELIZER_DESCRIPTOR_BUFFER_SURFACECOUNT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL };
	// Binding 6 : buffer indirect execute
	layoutBinding[POSTVOXELIZER_DESCRIPTOR_BUFFER_DISPATCHINDIRECT] =
	{ POSTVOXELIZER_DESCRIPTOR_BUFFER_DISPATCHINDIRECT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL };
	// Binding 7 : texture diffuse
	layoutBinding[POSTVOXELIZER_DESCRIPTOR_TEXTURE_DIFFUSE] =
	{ POSTVOXELIZER_DESCRIPTOR_TEXTURE_DIFFUSE, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, AXISCOUNT, VK_SHADER_STAGE_COMPUTE_BIT, NULL };
	// Binding 8 : texture normal
	layoutBinding[POSTVOXELIZER_DESCRIPTOR_TEXTURE_NORMAL] =
	{ POSTVOXELIZER_DESCRIPTOR_TEXTURE_NORMAL, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, AXISCOUNT, VK_SHADER_STAGE_COMPUTE_BIT, NULL };
	// Binding 9 : texture normal
	layoutBinding[POSTVOXELIZER_DESCRIPTOR_TEXTURE_EMISSION] =
	{ POSTVOXELIZER_DESCRIPTOR_TEXTURE_EMISSION, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, AXISCOUNT, VK_SHADER_STAGE_COMPUTE_BIT, NULL };
	// Binding 10 : texture sampler
	layoutBinding[POSTVOXELIZER_DESCRIPTOR_TEXTURE_SAMPLER] =
	{ POSTVOXELIZER_DESCRIPTOR_TEXTURE_SAMPLER, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL };

	// Create the descriptorlayout
	VkDescriptorSetLayoutCreateInfo descriptorLayout = VKTools::Initializers::DescriptorSetLayoutCreateInfo(0, PostVoxelizerDescriptorLayout::POSTVOXELIZERDESCRIPTOR_COUNT, layoutBinding);
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
	VkDescriptorPoolSize poolSize[PostVoxelizerDescriptorLayout::POSTVOXELIZERDESCRIPTOR_COUNT];
	poolSize[POSTVOXELIZER_DESCRIPTOR_VOXELGRID_DIFFUSE] =			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 };
	poolSize[POSTVOXELIZER_DESCRIPTOR_VOXELGRID_NORMAL] =			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 };
	poolSize[POSTVOXELIZER_DESCRIPTOR_VOXELGRID_EMISSION] =			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 };
	poolSize[POSTVOXELIZER_DESCRIPTOR_VOXELGRID_BUFFERPOSITION] =	{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 };
	poolSize[POSTVOXELIZER_DESCRIPTOR_BUFFER_SURFACEPOSITION] =		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 };
	poolSize[POSTVOXELIZER_DESCRIPTOR_BUFFER_SURFACECOUNT] =		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 };
	poolSize[POSTVOXELIZER_DESCRIPTOR_BUFFER_DISPATCHINDIRECT] =	{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 };
	poolSize[POSTVOXELIZER_DESCRIPTOR_TEXTURE_DIFFUSE] =			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, AXISCOUNT };
	poolSize[POSTVOXELIZER_DESCRIPTOR_TEXTURE_NORMAL] =				{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, AXISCOUNT };
	poolSize[POSTVOXELIZER_DESCRIPTOR_TEXTURE_EMISSION] =			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, AXISCOUNT };
	poolSize[POSTVOXELIZER_DESCRIPTOR_TEXTURE_SAMPLER] =			{ VK_DESCRIPTOR_TYPE_SAMPLER, 1 };
	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = VKTools::Initializers::DescriptorPoolCreateInfo(0, 1, POSTVOXELIZERDESCRIPTOR_COUNT, poolSize);
	//create the descriptorPool
	VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, NULL, &renderState.m_descriptorPool));

	////////////////////////////////////////////////////////////////////////////////
	// Create the descriptor set
	////////////////////////////////////////////////////////////////////////////////
	VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = VKTools::Initializers::DescriptorSetAllocateInfo(renderState.m_descriptorPool, 1, &renderState.m_descriptorLayouts[0]);
	VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &renderState.m_descriptorSets[0]));

	///////////////////////////////////////////////////////
	///// Set/Update the image and uniform buffer descriptorsets
	/////////////////////////////////////////////////////// 
	VkWriteDescriptorSet wds[11];
	// grid texture
	wds[0] = VKTools::Initializers::WriteDescriptorSet(
		renderState.m_descriptorSets[0],
		VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
		POSTVOXELIZER_DESCRIPTOR_VOXELGRID_DIFFUSE,
		&voxelgrid->albedoOpacity.descriptor[0]);
	wds[1] = VKTools::Initializers::WriteDescriptorSet(
		renderState.m_descriptorSets[0],
		VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
		POSTVOXELIZER_DESCRIPTOR_VOXELGRID_NORMAL,
		&voxelgrid->normal.descriptor[0]);
	wds[2] = VKTools::Initializers::WriteDescriptorSet(
		renderState.m_descriptorSets[0],
		VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
		POSTVOXELIZER_DESCRIPTOR_VOXELGRID_EMISSION,
		&voxelgrid->emission.descriptor[0]);
	wds[3] = VKTools::Initializers::WriteDescriptorSet(
		renderState.m_descriptorSets[0],
		VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
		POSTVOXELIZER_DESCRIPTOR_VOXELGRID_BUFFERPOSITION,
		&voxelgrid->bufferPosition.descriptor[0]);
	// buffer objects
	wds[4] = VKTools::Initializers::WriteDescriptorSet(
		renderState.m_descriptorSets[0],
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		POSTVOXELIZER_DESCRIPTOR_BUFFER_SURFACEPOSITION,
		&voxelgrid->surfacelist.descriptor);
	wds[5] = VKTools::Initializers::WriteDescriptorSet(
		renderState.m_descriptorSets[0],
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		POSTVOXELIZER_DESCRIPTOR_BUFFER_SURFACECOUNT,
		&renderState.m_bufferData[0].descriptor);
	wds[6] = VKTools::Initializers::WriteDescriptorSet(
		renderState.m_descriptorSets[0],
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		POSTVOXELIZER_DESCRIPTOR_BUFFER_DISPATCHINDIRECT,
		&renderState.m_bufferData[1].descriptor);
	// sample texture
	VkDescriptorImageInfo albedo[AXISCOUNT] = { voxelgrids[0].albedoOpacity.descriptor[0], voxelgrids[1].albedoOpacity.descriptor[0], voxelgrids[2].albedoOpacity.descriptor[0] };
	wds[7] = VKTools::Initializers::WriteDescriptorSet(
		renderState.m_descriptorSets[0],
		VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
		POSTVOXELIZER_DESCRIPTOR_TEXTURE_DIFFUSE,
		albedo);
	wds[7].descriptorCount = AXISCOUNT;
	VkDescriptorImageInfo normal[AXISCOUNT] = { voxelgrids[0].normal.descriptor[0], voxelgrids[1].normal.descriptor[0], voxelgrids[2].normal.descriptor[0] };
	wds[8] = VKTools::Initializers::WriteDescriptorSet(
		renderState.m_descriptorSets[0],
		VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
		POSTVOXELIZER_DESCRIPTOR_TEXTURE_NORMAL,
		normal);
	wds[8].descriptorCount = AXISCOUNT;
	VkDescriptorImageInfo emission[AXISCOUNT] = { voxelgrids[0].emission.descriptor[0], voxelgrids[1].emission.descriptor[0], voxelgrids[2].emission.descriptor[0] };
	wds[9] = VKTools::Initializers::WriteDescriptorSet(
		renderState.m_descriptorSets[0],
		VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
		POSTVOXELIZER_DESCRIPTOR_TEXTURE_EMISSION,
		emission);
	wds[9].descriptorCount = AXISCOUNT;
	// sampler
	VkDescriptorImageInfo dii = { sampler , NULL, VK_IMAGE_LAYOUT_UNDEFINED };
	wds[10] = VKTools::Initializers::WriteDescriptorSet(
		renderState.m_descriptorSets[0],
		VK_DESCRIPTOR_TYPE_SAMPLER,
		POSTVOXELIZER_DESCRIPTOR_TEXTURE_SAMPLER,
		&dii);

	vkUpdateDescriptorSets(device, POSTVOXELIZERDESCRIPTOR_COUNT, wds, 0, NULL);

	///////////////////////////////////////////////////////
	///// Create the compute pipeline
	/////////////////////////////////////////////////////// 
	// Create pipeline		
	VkComputePipelineCreateInfo computePipelineCreateInfo = VKTools::Initializers::ComputePipelineCreateInfo(renderState.m_pipelineLayout, VK_FLAGS_NONE);
	// Shaders are loaded from the SPIR-V format, which can be generated from glsl
	Shader shaderStage;
	shaderStage = VKTools::LoadShader("shaders/postvoxelizer.comp.spv", "main", device, VK_SHADER_STAGE_COMPUTE_BIT);
	computePipelineCreateInfo.stage = shaderStage.m_shaderStage;
	VK_CHECK_RESULT(vkCreateComputePipelines(device, renderState.m_pipelineCache, 1, &computePipelineCreateInfo, nullptr, &renderState.m_pipelines[0]));
}