#include "PipelineStates.h"

#include <glm/gtc/matrix_transform.hpp>
#include "VCTPipelineDefines.h"
#include "SwapChain.h"
#include "VulkanCore.h"
#include "Shader.h"
#include "DataTypes.h"
#include "AnisotropicVoxelTexture.h"
#include "Camera.h"

struct PushConstantFrag
{
	uint32_t cascadeNum;		// The current cascade
};

struct Parameter
{
	AnisotropicVoxelTexture* avt;
	VkDescriptorSet staticDescriptorSet;
	vk_mesh_s* meshes;
	uint32_t meshCount;
};

void BuildCommandBufferVoxelizerState(
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
	AnisotropicVoxelTexture* avt = ((Parameter*)parameters)->avt;
	VkDescriptorSet staticDescriptorSet = ((Parameter*)parameters)->staticDescriptorSet;
	vk_mesh_s* meshes = ((Parameter*)parameters)->meshes;
	uint32_t meshCount = ((Parameter*)parameters)->meshCount;

	////////////////////////////////////////////////////////////////////////////////
	// Rebuild the command buffers
	////////////////////////////////////////////////////////////////////////////////
	renderState->m_commandBufferCount = avt->m_cascadeCount;
	renderState->m_commandBuffers = (VkCommandBuffer*)malloc(sizeof(VkCommandBuffer)*renderState->m_commandBufferCount);
	for (uint32_t i = 0; i < renderState->m_commandBufferCount; i++)
		renderState->m_commandBuffers[i] = VKTools::Initializers::CreateCommandBuffer(commandpool, device, VK_COMMAND_BUFFER_LEVEL_PRIMARY, false);

	////////////////////////////////////////////////////////////////////////////////
	// Record command buffer
	////////////////////////////////////////////////////////////////////////////////
	VkCommandBufferBeginInfo cmdBufInfo = {};
	cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBufInfo.pNext = NULL;

	VkRenderPassBeginInfo renderPassBeginInfo = {};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.pNext = NULL;
	renderPassBeginInfo.renderPass = renderState->m_renderpass;
	renderPassBeginInfo.renderArea.offset.x = 0;
	renderPassBeginInfo.renderArea.offset.y = 0;
	renderPassBeginInfo.renderArea.extent.width = avt->m_width;
	renderPassBeginInfo.renderArea.extent.height = avt->m_height;
	renderPassBeginInfo.clearValueCount = 0;
	renderPassBeginInfo.pClearValues = NULL;

	renderPassBeginInfo.framebuffer = renderState->m_framebuffers[0];
	uint32_t d = 0;
	for (uint32_t i = 0; i < renderState->m_commandBufferCount; i++)
	{
		VK_CHECK_RESULT(vkBeginCommandBuffer(renderState->m_commandBuffers[i], &cmdBufInfo));
		
		// Write timestamp
		vkCmdResetQueryPool(renderState->m_commandBuffers[i], renderState->m_queryPool, 0, 4);
		vkCmdWriteTimestamp(renderState->m_commandBuffers[i], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, renderState->m_queryPool, 0);
		vkCmdBeginRenderPass(renderState->m_commandBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		// Update dynamic viewport state
		VkViewport viewport = {};
		viewport.width = (float)avt->m_width;
		viewport.height = (float)avt->m_height;
		viewport.minDepth = (float) 0.0f;
		viewport.maxDepth = (float) 1.0f;
		vkCmdSetViewport(renderState->m_commandBuffers[i], 0, 1, &viewport);

		// Update dynamic scissor state
		VkRect2D scissor = {};
		scissor.extent.width = avt->m_width;
		scissor.extent.height = avt->m_height;
		scissor.offset.x = 0;
		scissor.offset.y = 0;
		vkCmdSetScissor(renderState->m_commandBuffers[i], 0, 1, &scissor);

		// Submit push constant
		PushConstantFrag pc;
		pc.cascadeNum = i;
		vkCmdPushConstants(renderState->m_commandBuffers[i], renderState->m_pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstantFrag), &pc);

		// Bind the rendering pipeline (including the shaders)
		vkCmdBindPipeline(renderState->m_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, renderState->m_pipelines[0]);

		// Bind descriptor sets describing shader binding points
		uint32_t doffset = 0;
		vkCmdBindDescriptorSets(renderState->m_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, renderState->m_pipelineLayout, 0, 1, &staticDescriptorSet, 1, &doffset);
		vkCmdBindDescriptorSets(renderState->m_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, renderState->m_pipelineLayout, 2, 1, &renderState->m_descriptorSets[0], 0, NULL);

		for (uint32_t m = 0; m < meshCount; m++)
		{
			//select the current mesh
			vk_mesh_s* mesh = &meshes[m];
			//bind vertexbuffer per mesh
			vkCmdBindVertexBuffers(renderState->m_commandBuffers[i], 0, mesh->vbvCount, mesh->vertexResources, mesh->vertexOffsets);
			for (uint32_t j = 0; j < mesh->submeshCount; j++)
			{
				//TODO: BIND new descriptorset. This looks wrong. fix...
				uint32_t setnum = (d++) + 1;
				VkDescriptorSet descriptorset = renderState->m_descriptorSets[setnum];

				//bind the textures to the correct format
				//format: stype,pnext,scSet,srcBinding,srcArrayelement,dstSet,dstbinding,dstarrayelement,descriptorcount
				VkCopyDescriptorSet textureDescriptorSets[TextureIndex::TEXTURE_NUM];
				//diffuse texture
				VkCopyDescriptorSet diffuse;
				diffuse.sType = VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET;
				diffuse.pNext = NULL;
				diffuse.srcSet = staticDescriptorSet;
				diffuse.srcBinding = STATIC_DESCRIPTOR_IMAGE;
				diffuse.srcArrayElement = mesh->submeshes[j].textureIndex[DIFFUSE_TEXTURE];
				diffuse.dstSet = descriptorset;
				diffuse.dstBinding = VOXELIZER_DESCRIPTOR_IMAGE_DIFFUSE;
				diffuse.dstArrayElement = 0;
				diffuse.descriptorCount = 1;
				textureDescriptorSets[DIFFUSE_TEXTURE] = diffuse;
				//normal texture
				VkCopyDescriptorSet normal;
				normal.sType = VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET;
				normal.pNext = NULL;
				normal.srcSet = staticDescriptorSet;
				normal.srcBinding = STATIC_DESCRIPTOR_IMAGE;
				normal.srcArrayElement = mesh->submeshes[j].textureIndex[NORMAL_TEXTURE];
				normal.dstSet = descriptorset;
				normal.dstBinding = VOXELIZER_DESCRIPTOR_IMAGE_NORMAL;
				normal.dstArrayElement = 0;
				normal.descriptorCount = 1;
				textureDescriptorSets[NORMAL_TEXTURE] = normal;
				//opacity teture
				VkCopyDescriptorSet opacity;
				opacity.sType = VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET;
				opacity.pNext = NULL;
				opacity.srcSet = staticDescriptorSet;
				opacity.srcBinding = STATIC_DESCRIPTOR_IMAGE;
				opacity.srcArrayElement = (uint32_t)mesh->submeshes[j].textureIndex[OPACITY_TEXTURE];
				opacity.dstSet = descriptorset;
				opacity.dstBinding = VOXELIZER_DESCRIPTOR_IMAGE_OPACITY;
				opacity.dstArrayElement = 0;
				opacity.descriptorCount = 1;
				textureDescriptorSets[OPACITY_TEXTURE] = opacity;
				//update the descriptors
				vkUpdateDescriptorSets(device, 0, NULL, (uint32_t)TEXTURE_NUM, textureDescriptorSets);
				// Bind descriptor sets describing shader binding points
				vkCmdBindDescriptorSets(renderState->m_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, renderState->m_pipelineLayout, 1, 1, &descriptorset, 0, NULL);
				// Bind triangle indices
				vkCmdBindIndexBuffer(renderState->m_commandBuffers[i], mesh->submeshes[j].ibv.buffer, mesh->submeshes[j].ibv.offset, mesh->submeshes[j].ibv.format);
				// Draw indexed triangle
				vkCmdDrawIndexed(renderState->m_commandBuffers[i], (uint32_t)mesh->submeshes[j].ibv.count, 1, 0, 0, 0);
			}
		}

		vkCmdWriteTimestamp(renderState->m_commandBuffers[i], VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, renderState->m_queryPool, 1);

		//end
		vkCmdEndRenderPass(renderState->m_commandBuffers[i]);
		VK_CHECK_RESULT(vkEndCommandBuffer(renderState->m_commandBuffers[i]));
	}
}

void CreateVoxelizerState(
	RenderState& renderState,
	VulkanCore* core,
	VkCommandPool commandPool,
	SwapChain* swapchain,
	VkDescriptorSet staticDescriptorSet,
	Vertices* vertices,
	vk_mesh_s* meshes,
	uint32_t meshCount,
	VkDescriptorSetLayout staticDescLayout,
	Camera* camera,
	AnisotropicVoxelTexture* avt)
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

	///////////////////////////////////////////////////////////////////////////////
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
	// Create semaphores
	////////////////////////////////////////////////////////////////////////////////
	if (!renderState.m_semaphores)
	{
		renderState.m_semaphoreCount = avt->m_cascadeCount;
		renderState.m_semaphores = (VkSemaphore*)malloc(sizeof(VkSemaphore)*renderState.m_semaphoreCount);
		VkSemaphoreCreateInfo semInfo = VKTools::Initializers::SemaphoreCreateInfo();
		for (uint32_t i = 0; i < renderState.m_semaphoreCount; i++)
			vkCreateSemaphore(device, &semInfo, NULL, &renderState.m_semaphores[i]);
	}

	////////////////////////////////////////////////////////////////////////////////
	//create the renderpass
	////////////////////////////////////////////////////////////////////////////////
	if (renderState.m_renderpass == VK_NULL_HANDLE)
	{
		VkAttachmentDescription attachments = {};
		VkAttachmentReference depthReference = {};
		depthReference.attachment = 0;
		depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.flags = 0;
		subpass.inputAttachmentCount = 0;
		subpass.pInputAttachments = NULL;
		subpass.colorAttachmentCount = 0;
		subpass.pColorAttachments = NULL;
		subpass.pResolveAttachments = NULL;
		subpass.pDepthStencilAttachment = NULL;
		subpass.preserveAttachmentCount = 0;
		subpass.pPreserveAttachments = NULL;

		VkRenderPassCreateInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.pNext = NULL;
		renderPassInfo.attachmentCount = 0;
		renderPassInfo.pAttachments = NULL;
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 0;
		renderPassInfo.pDependencies = NULL;

		VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassInfo, NULL, &renderState.m_renderpass));
	}

	////////////////////////////////////////////////////////////////////////////////
	// Create the framebuffers
	////////////////////////////////////////////////////////////////////////////////
	if (!renderState.m_framebuffers)
	{
		renderState.m_framebufferCount = 1;
		renderState.m_framebuffers = (VkFramebuffer*)malloc(sizeof(VkFramebuffer) * renderState.m_framebufferCount);
		//setup create info for framebuffers
		VkFramebufferCreateInfo frameBufferCreateInfo = {};
		frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		frameBufferCreateInfo.pNext = NULL;
		frameBufferCreateInfo.renderPass = renderState.m_renderpass;
		frameBufferCreateInfo.attachmentCount = 0;
		frameBufferCreateInfo.pAttachments = NULL;
		frameBufferCreateInfo.width = avt->m_width;
		frameBufferCreateInfo.height = avt->m_height;
		frameBufferCreateInfo.layers = 1;
		VK_CHECK_RESULT(vkCreateFramebuffer(device, &frameBufferCreateInfo, NULL, &renderState.m_framebuffers[0]));
	}
	
	////////////////////////////////////////////////////////////////////////////////
	// Create the Uniform Data
	////////////////////////////////////////////////////////////////////////////////
	if (!renderState.m_uniformData)
	{
		renderState.m_uniformDataCount = 2;
		renderState.m_uniformData = (UniformData*)malloc(sizeof(UniformData)*renderState.m_uniformDataCount);

		VKTools::CreateBuffer(core, device,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			sizeof(VoxelizerUBOGeom),
			NULL,
			&renderState.m_uniformData[0].m_buffer,
			&renderState.m_uniformData[0].m_memory,
			&renderState.m_uniformData[0].m_descriptor);

		VKTools::CreateBuffer(core, device,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			sizeof(VoxelizerUBOFrag),
			NULL,
			&renderState.m_uniformData[1].m_buffer,
			&renderState.m_uniformData[1].m_memory,
			&renderState.m_uniformData[1].m_descriptor);
	}

	////////////////////////////////////////////////////////////////////////////////
	// Set the descriptorset layout
	////////////////////////////////////////////////////////////////////////////////
	if (!renderState.m_descriptorLayouts)
	{
		renderState.m_descriptorLayoutCount = 2;
		renderState.m_descriptorLayouts = (VkDescriptorSetLayout*)malloc(renderState.m_descriptorLayoutCount * sizeof(VkDescriptorSetLayout));
		// Dynamic descriptorset
		VkDescriptorSetLayoutBinding layoutbinding0[VOXELIZER_MULTIPLE_DESCRIPTOR_COUNT];
		VkDescriptorSetLayoutBinding layoutbinding1[VOXELIZER_SINGLE_DESCRIPTOR_COUNT];
		// Binding 0 : Diffuse texture sampled image
		layoutbinding0[VOXELIZER_DESCRIPTOR_IMAGE_DIFFUSE] =
		{ VOXELIZER_DESCRIPTOR_IMAGE_DIFFUSE, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL };
		// Binding 1 : Normal texture sampled image
		layoutbinding0[VOXELIZER_DESCRIPTOR_IMAGE_NORMAL] =
		{ VOXELIZER_DESCRIPTOR_IMAGE_NORMAL, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL };
		// Binding 2: Opacity texture sampled image
		layoutbinding0[VOXELIZER_DESCRIPTOR_IMAGE_OPACITY] =
		{ VOXELIZER_DESCRIPTOR_IMAGE_OPACITY, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL };
		// Binding 3: 3D voxel textures
		layoutbinding1[VOXELIZER_DESCRIPTOR_IMAGE_VOXELGRID] =
		{ VOXELIZER_DESCRIPTOR_IMAGE_VOXELGRID, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL };
		// Binding 4: Geometry uniform buffer
		layoutbinding1[VOXELIZER_DESCRIPTOR_BUFFER_GEOM] =
		{ VOXELIZER_DESCRIPTOR_BUFFER_GEOM , VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER , 1, VK_SHADER_STAGE_GEOMETRY_BIT, NULL };
		// Binding 5: Fragment uniform buffer
		layoutbinding1[VOXELIZER_DESCRIPTOR_BUFFER_FRAG] =
		{ VOXELIZER_DESCRIPTOR_BUFFER_FRAG , VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER , 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL };
		// Binding 6: 3D voxel textures
		layoutbinding1[VOXELIZER_DESCRIPTOR_IMAGE_ALPHAVOXELGRID] =
		{ VOXELIZER_DESCRIPTOR_IMAGE_ALPHAVOXELGRID, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL };
		// Create the descriptorlayout0
		VkDescriptorSetLayoutCreateInfo descriptorLayout0 = VKTools::Initializers::DescriptorSetLayoutCreateInfo(0, VOXELIZER_MULTIPLE_DESCRIPTOR_COUNT, layoutbinding0);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout0, NULL, &renderState.m_descriptorLayouts[0]));
		// Create the descriptorlayout1
		VkDescriptorSetLayoutCreateInfo descriptorLayout1 = VKTools::Initializers::DescriptorSetLayoutCreateInfo(0, VOXELIZER_SINGLE_DESCRIPTOR_COUNT, layoutbinding1);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout1, NULL, &renderState.m_descriptorLayouts[1]));
	}

	////////////////////////////////////////////////////////////////////////////////
	// Create pipeline layout
	////////////////////////////////////////////////////////////////////////////////
	if (!renderState.m_pipelineLayout)
	{
		VkDescriptorSetLayout dLayouts[] = { staticDescLayout, renderState.m_descriptorLayouts[0],renderState.m_descriptorLayouts[1] };
		VkPushConstantRange pushConstantRange = VKTools::Initializers::PushConstantRange(VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstantFrag));
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = VKTools::Initializers::PipelineLayoutCreateInfo(0, 3, dLayouts);
		pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
		pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &renderState.m_pipelineLayout));
	}

	////////////////////////////////////////////////////////////////////////////////
	// Create descriptor pool
	////////////////////////////////////////////////////////////////////////////////
	if (!renderState.m_descriptorPool)
	{
		VkDescriptorPoolSize poolSize[VOXELIZER_DESCRIPTOR_COUNT];
		poolSize[VOXELIZER_DESCRIPTOR_IMAGE_DIFFUSE] = { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE , avt->m_cascadeCount * DYNAMIC_DESCRIPTOR_SET_COUNT };
		poolSize[VOXELIZER_DESCRIPTOR_IMAGE_NORMAL] = { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE , avt->m_cascadeCount * DYNAMIC_DESCRIPTOR_SET_COUNT };
		poolSize[VOXELIZER_DESCRIPTOR_IMAGE_OPACITY] = { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE , avt->m_cascadeCount * DYNAMIC_DESCRIPTOR_SET_COUNT };
		poolSize[VOXELIZER_DESCRIPTOR_IMAGE_VOXELGRID + VOXELIZER_MULTIPLE_DESCRIPTOR_COUNT] = { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 };
		poolSize[VOXELIZER_DESCRIPTOR_BUFFER_GEOM + VOXELIZER_MULTIPLE_DESCRIPTOR_COUNT] = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER , 1 };
		poolSize[VOXELIZER_DESCRIPTOR_BUFFER_FRAG + VOXELIZER_MULTIPLE_DESCRIPTOR_COUNT] = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER , 1 };
		poolSize[VOXELIZER_DESCRIPTOR_IMAGE_ALPHAVOXELGRID + VOXELIZER_MULTIPLE_DESCRIPTOR_COUNT] = { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE , 1 };
		VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = VKTools::Initializers::DescriptorPoolCreateInfo(0, (avt->m_cascadeCount * DYNAMIC_DESCRIPTOR_SET_COUNT) + 1, VOXELIZER_DESCRIPTOR_COUNT, poolSize);
		//create the descriptorPool
		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, NULL, &renderState.m_descriptorPool));
	}

	////////////////////////////////////////////////////////////////////////////////
	// Create the descriptor set
	////////////////////////////////////////////////////////////////////////////////
	if (!renderState.m_descriptorSets)
	{
		//allocate the requirered descriptorsets
		renderState.m_descriptorSetCount = (DYNAMIC_DESCRIPTOR_SET_COUNT * avt->m_cascadeCount) + 1;
		renderState.m_descriptorSets = (VkDescriptorSet*)malloc(renderState.m_descriptorSetCount * sizeof(VkDescriptorSet));
		for (uint32_t i = 0; i < avt->m_cascadeCount; i++)
		{
			for (uint32_t j = 0; j < DYNAMIC_DESCRIPTOR_SET_COUNT; j++)
			{
				VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = VKTools::Initializers::DescriptorSetAllocateInfo(renderState.m_descriptorPool, 1, &renderState.m_descriptorLayouts[0]);
				VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &renderState.m_descriptorSets[(i*DYNAMIC_DESCRIPTOR_SET_COUNT) + j + 1]));
			}
		}
		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = VKTools::Initializers::DescriptorSetAllocateInfo(renderState.m_descriptorPool, 1, &renderState.m_descriptorLayouts[1]);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &renderState.m_descriptorSets[0]));

		///////////////////////////////////////////////////////
		///// Set/Update the image and uniform buffer descriptorsets
		/////////////////////////////////////////////////////// 
		// Descriptorset 1 = multiple
		// Descriptorset 0 = single
		VkWriteDescriptorSet writeDescriptorSet = {};
		// Update GEOM descriptorset
		writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSet.pNext = NULL;
		writeDescriptorSet.dstSet = renderState.m_descriptorSets[0];
		writeDescriptorSet.dstBinding = VOXELIZER_DESCRIPTOR_BUFFER_GEOM;
		writeDescriptorSet.dstArrayElement = 0;
		writeDescriptorSet.descriptorCount = 1;
		writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writeDescriptorSet.pImageInfo = NULL;
		writeDescriptorSet.pBufferInfo = &renderState.m_uniformData[0].m_descriptor;
		writeDescriptorSet.pTexelBufferView = NULL;
		vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, NULL);
		// Update FRAG descriptorset
		writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSet.pNext = NULL;
		writeDescriptorSet.dstSet = renderState.m_descriptorSets[0];
		writeDescriptorSet.dstBinding = VOXELIZER_DESCRIPTOR_BUFFER_FRAG;
		writeDescriptorSet.dstArrayElement = 0;
		writeDescriptorSet.descriptorCount = 1;
		writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writeDescriptorSet.pImageInfo = NULL;
		writeDescriptorSet.pBufferInfo = &renderState.m_uniformData[1].m_descriptor;
		writeDescriptorSet.pTexelBufferView = NULL;
		vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, NULL);
		// Update the descriptorsets for the voxel textures
		VkWriteDescriptorSet wds = {};
		// Bind the 3D voxel textures
		{
			wds.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			wds.pNext = NULL;
			wds.dstSet = renderState.m_descriptorSets[0];
			wds.dstBinding = VOXELIZER_DESCRIPTOR_IMAGE_VOXELGRID;
			wds.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			wds.descriptorCount = 1;
			wds.dstArrayElement = 0;
			wds.pImageInfo = &avt->m_descriptor[0];
			//update the descriptorset
			vkUpdateDescriptorSets(device, 1, &wds, 0, NULL);
		}
		// Bind the 3D voxel textures
		{
			wds.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			wds.pNext = NULL;
			wds.dstSet = renderState.m_descriptorSets[0];
			wds.dstBinding = VOXELIZER_DESCRIPTOR_IMAGE_ALPHAVOXELGRID;
			wds.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			wds.descriptorCount = 1;
			wds.dstArrayElement = 0;
			wds.pImageInfo = &avt->m_alphaDescriptor;
			//update the descriptorset
			vkUpdateDescriptorSets(device, 1, &wds, 0, NULL);
		}
	}

	////////////////////////////////////////////////////////////////////////////////
	// Create Pipeline
	////////////////////////////////////////////////////////////////////////////////
	if (!renderState.m_pipelines)
	{
		renderState.m_pipelineCount = 1;
		renderState.m_pipelines = (VkPipeline*)malloc(renderState.m_pipelineCount * sizeof(VkPipeline));

		// Create the pipeline input assembly state info
		VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyCreateInfo = {};
		pipelineInputAssemblyCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		// This pipeline renders vertex data as triangles
		pipelineInputAssemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

		// Rasterization state
		VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo = {};
		pipelineRasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		// Solid polygon mode
		pipelineRasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
		// Enable culling TODO: might need fix
		pipelineRasterizationStateCreateInfo.cullMode = VK_CULL_MODE_NONE;
		// Set vert read to counter clockwise
		pipelineRasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		pipelineRasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
		pipelineRasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
		pipelineRasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
		pipelineRasterizationStateCreateInfo.lineWidth = 1.0f;

		// Color blend state
		VkPipelineColorBlendStateCreateInfo colorBlendState = {};
		colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlendState.attachmentCount = 0;
		colorBlendState.pAttachments = NULL;

		VkPipelineViewportStateCreateInfo viewportState = {};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		//one viewport for now
		viewportState.viewportCount = 1;
		//scissor rectable
		viewportState.scissorCount = 1;

		//enable dynamic state
		VkPipelineDynamicStateCreateInfo dynamicState = {};
		// The dynamic state properties themselves are stored in the command buffer
		std::vector<VkDynamicState> dynamicStateEnables;
		dynamicStateEnables.push_back(VK_DYNAMIC_STATE_VIEWPORT);
		dynamicStateEnables.push_back(VK_DYNAMIC_STATE_SCISSOR);
		dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.pDynamicStates = dynamicStateEnables.data();
		dynamicState.dynamicStateCount = (uint32_t)dynamicStateEnables.size();

		// Depth and stencil state
		// Describes depth and stenctil test and compare ops
		VkPipelineDepthStencilStateCreateInfo depthStencilState = {};
		// Basic depth compare setup with depth writes and depth test enabled
		// No stencil used 
		depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencilState.depthTestEnable = VK_FALSE;
		depthStencilState.depthWriteEnable = VK_FALSE;
		depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
		depthStencilState.depthBoundsTestEnable = VK_FALSE;
		depthStencilState.back.failOp = VK_STENCIL_OP_KEEP;
		depthStencilState.back.passOp = VK_STENCIL_OP_KEEP;
		depthStencilState.back.compareOp = VK_COMPARE_OP_ALWAYS;
		depthStencilState.stencilTestEnable = VK_FALSE;
		depthStencilState.front = depthStencilState.back;

		// Multi sampling state
		VkPipelineMultisampleStateCreateInfo multisampleState = {};
		multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampleState.pSampleMask = NULL;
		// No multi sampling used in this example
		multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		// Load shaders
		// Shaders are loaded from the SPIR-V format, which can be generated from glsl
#define	SHADERNUM 3
		Shader shaderStages[SHADERNUM];
		shaderStages[0] = VKTools::LoadShader("shaders/voxelizer.vert.spv", "main", device, VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = VKTools::LoadShader("shaders/voxelizer.geom.spv", "main", device, VK_SHADER_STAGE_GEOMETRY_BIT);
		shaderStages[2] = VKTools::LoadShader("shaders/voxelizer.frag.spv", "main", device, VK_SHADER_STAGE_FRAGMENT_BIT);

		// Assign states
		// Assign pipeline state create information
		std::vector<VkPipelineShaderStageCreateInfo> shaderStagesData;
		for (int i = 0; i < SHADERNUM; i++)
			shaderStagesData.push_back(shaderStages[i].m_shaderStage);
#undef SHADERNUM

		// pipelineinfo for creating the pipeline
		VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
		pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineCreateInfo.layout = renderState.m_pipelineLayout;
		pipelineCreateInfo.stageCount = (uint32_t)shaderStagesData.size();
		pipelineCreateInfo.pStages = shaderStagesData.data();
		pipelineCreateInfo.pVertexInputState = &vertices->inputState;
		pipelineCreateInfo.pInputAssemblyState = &pipelineInputAssemblyCreateInfo;
		pipelineCreateInfo.pRasterizationState = &pipelineRasterizationStateCreateInfo;
		pipelineCreateInfo.pColorBlendState = &colorBlendState;
		pipelineCreateInfo.pMultisampleState = &multisampleState;
		pipelineCreateInfo.pViewportState = &viewportState;
		pipelineCreateInfo.pDepthStencilState = &depthStencilState;
		pipelineCreateInfo.renderPass = renderState.m_renderpass;
		pipelineCreateInfo.pDynamicState = &dynamicState;

		VkPhysicalDeviceProperties deviceProps;
		vkGetPhysicalDeviceProperties(core->GetPhysicalGPU(), &deviceProps);
		//assert(sizeof(pushConstants) <= deviceProps.limits.maxPushConstantsSize);

		// Create rendering pipeline
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, renderState.m_pipelineCache, 1, &pipelineCreateInfo, NULL, &renderState.m_pipelines[0]));
	}

	////////////////////////////////////////////////////////////////////////////////
	// Build command buffers
	////////////////////////////////////////////////////////////////////////////////
	Parameter* parameter;
	parameter = (Parameter*)malloc(sizeof(Parameter));
	parameter->avt = avt;
	parameter->meshCount = meshCount;
	parameter->meshes = meshes;
	parameter->staticDescriptorSet = staticDescriptorSet;
	renderState.m_cmdBufferParameters = (BYTE*)parameter;

	renderState.m_CreateCommandBufferFunc = &BuildCommandBufferVoxelizerState;
	renderState.m_CreateCommandBufferFunc(&renderState, commandPool, core, renderState.m_framebufferCount, renderState.m_framebuffers, renderState.m_cmdBufferParameters);
}