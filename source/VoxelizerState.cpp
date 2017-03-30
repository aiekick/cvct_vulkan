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

#define AXISSCALAR 2

// order axis: YZ(x), XZ(y), XY(z)

uint32_t uniformgridresolution = {};

struct UBOGeom
{
	glm::mat4 viewprojectionAxis;
	BYTE padding0[192];
};

struct UBOFrag
{
	glm::vec4 voxelregionworld;					// xyz = position, w = size
	glm::ivec3 voxelresolution;	float padding0;	// resolution of the voxel grid
	BYTE padding1[224];
};

void UpdateDUBOVoxelizer(
	RenderState& renderstate,
	VulkanCore& core,
	glm::vec4 voxelregionworld,
	glm::ivec3 voxelresolution)
{
	VkDevice device = core.GetViewDevice();
	////////////////////////////////////////////////////////////////////////////////
	// Update the descriptorsets for the voxel textures
	////////////////////////////////////////////////////////////////////////////////
	glm::mat4 correction;
	correction[1][1] = -1;
	correction[2][2] = 0.5;
	correction[3][2] = 0.5;

	glm::ivec3 scalar[AXISCOUNT] =
	{
		glm::ivec3(1,AXISSCALAR,AXISSCALAR),
		glm::ivec3(AXISSCALAR,1,AXISSCALAR),
		glm::ivec3(AXISSCALAR,AXISSCALAR,1)
	};

	float worldSize = voxelregionworld.w;
	float halfSize = worldSize / 2.0f;

	glm::vec3 bMin = glm::vec3(voxelregionworld);
	glm::vec3 bMax = bMin + worldSize;
	glm::vec3 bMid = (bMin + bMax) / 2.0f;
	glm::mat4 orthoProjection = glm::ortho(-halfSize, halfSize, -halfSize, halfSize, 0.0f, worldSize);

	glm::mat4 ViewAxis[3] =
	{
		(correction * orthoProjection) * glm::lookAt(glm::vec3(bMin.x, bMid.y, bMid.z), glm::vec3(bMax.x, bMid.y, bMid.z), glm::vec3(0, 0, 1)),
		(correction * orthoProjection) * glm::lookAt(glm::vec3(bMid.x, bMin.y, bMid.z), glm::vec3(bMid.x, bMax.y, bMid.z), glm::vec3(1, 0, 0)),
		(correction * orthoProjection) * glm::lookAt(glm::vec3(bMid.x, bMid.y, bMin.z), glm::vec3(bMid.x, bMid.y, bMax.z), glm::vec3(0, 1, 0))
	};

	UBOGeom ubogeom[AXISCOUNT];
	UBOFrag ubofrag[AXISCOUNT];
	for (uint32_t i = 0; i < AXISCOUNT; i++)
	{
		ubogeom[i].viewprojectionAxis = ViewAxis[i];
		ubofrag[i].voxelregionworld = voxelregionworld;
		ubofrag[i].voxelresolution = voxelresolution * scalar[i];
	}

	uint8_t *pData;
	// Update geoemtry UBO
	VK_CHECK_RESULT(vkMapMemory(device, renderstate.m_bufferData[0].memory, 0, sizeof(UBOGeom) * AXISCOUNT, 0, (void**)&pData));
	memcpy(pData, &ubogeom, sizeof(UBOGeom) * AXISCOUNT);
	vkUnmapMemory(device, renderstate.m_bufferData[0].memory);
	// Update fragment UBO
	VK_CHECK_RESULT(vkMapMemory(device, renderstate.m_bufferData[1].memory, 0, sizeof(UBOFrag) * AXISCOUNT, 0, (void**)&pData));
	memcpy(pData, &ubofrag, sizeof(UBOFrag) * AXISCOUNT);
	vkUnmapMemory(device, renderstate.m_bufferData[1].memory);
	pData = NULL;
}

void CommandIndirectVoxelizer(
	RenderState& renderstate,
	VulkanCore* core,
	VKScene* scenes,
	uint32_t sceneCount,
	VKMesh* meshes,
	uint32_t meshCount,
	VkDescriptorSet staticdescriptorset,
	VkDescriptorSet* texturedescriptorset,
	VkBuffer indirectcommandbuffer)
{
	RenderState& renderState = renderstate;
	uint32_t width = uniformgridresolution * AXISSCALAR;
	uint32_t height = uniformgridresolution * AXISSCALAR;
	VkDevice device = core->GetViewDevice();
	VkRenderPass renderpass = renderstate.m_renderpass;
	VkFramebuffer framebuffer = renderstate.m_framebuffers[0];

	////////////////////////////////////////////////////////////////////////////////
	// Record command buffer
	////////////////////////////////////////////////////////////////////////////////
	VkCommandBufferBeginInfo cmdBufInfo = {};
	cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBufInfo.pNext = NULL;

	VkRenderPassBeginInfo renderPassBeginInfo = {};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.pNext = NULL;
	renderPassBeginInfo.renderPass = renderpass;
	renderPassBeginInfo.renderArea.offset.x = 0;
	renderPassBeginInfo.renderArea.offset.y = 0;
	renderPassBeginInfo.renderArea.extent.width = width;
	renderPassBeginInfo.renderArea.extent.height = height;
	renderPassBeginInfo.clearValueCount = 0;
	renderPassBeginInfo.pClearValues = NULL;

	for (uint32_t i = 0; i < AXISCOUNT; i++)
	{
		// Set target frame buffer
		renderPassBeginInfo.framebuffer = framebuffer;

		//begin
		VK_CHECK_RESULT(vkBeginCommandBuffer(renderState.m_commandBuffers[i], &cmdBufInfo));

		vkCmdResetQueryPool(renderState.m_commandBuffers[i], renderState.m_queryPool, 0, 4);
		vkCmdWriteTimestamp(renderState.m_commandBuffers[i], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, renderState.m_queryPool, 0);

		// Start the first sub pass specified in our default render pass setup by the base class
		// This will clear the color and depth attachment
		vkCmdBeginRenderPass(renderState.m_commandBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		// Update dynamic viewport state
		VkViewport viewport = {};
		viewport.width = (float)width;
		viewport.height = (float)height;
		viewport.minDepth = (float) 0.0f;
		viewport.maxDepth = (float) 1.0f;
		vkCmdSetViewport(renderState.m_commandBuffers[i], 0, 1, &viewport);

		// Update dynamic scissor state
		VkRect2D scissor = {}; 
		scissor.extent.width = width;
		scissor.extent.height = height;
		scissor.offset.x = 0;
		scissor.offset.y = 0;
		vkCmdSetScissor(renderState.m_commandBuffers[i], 0, 1, &scissor);

		// Bind the rendering pipeline (including the shaders)
		vkCmdBindPipeline(renderState.m_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, renderState.m_pipelines[0]);
		uint32_t dynamicoffset2[2];
		dynamicoffset2[0] = i * sizeof(UBOGeom);
		dynamicoffset2[1] = i * sizeof(UBOFrag);
		vkCmdBindDescriptorSets(renderState.m_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, renderState.m_pipelineLayout, 2, 1, &renderstate.m_descriptorSets[i], 2, dynamicoffset2);
		uint32_t dynamicoffset0[2];
		// Bind descriptor sets describing shader binding points
		uint32_t submeshoffset = 0;
		uint32_t meshoffset = 0;
		for (uint32_t s = 0; s < sceneCount; s++)
		{
			VKScene* scene = &scenes[s];
			dynamicoffset0[0] = s * sizeof(PerSceneUBO);

			for (uint32_t m = 0; m < scene->vkmeshCount; m++)
			{
				VKMesh* mesh = &meshes[meshoffset + m];
				vkCmdBindVertexBuffers(renderState.m_commandBuffers[i], 0, mesh->vbvCount, mesh->vertexResources, mesh->vertexOffsets);

				for (uint32_t sm = 0; sm < mesh->submeshCount; sm++)
				{
					dynamicoffset0[1] = (submeshoffset + sm) * sizeof(PerSubMeshUBO);
					vkCmdBindDescriptorSets(renderState.m_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, renderState.m_pipelineLayout, 0, 1, &staticdescriptorset, 2, dynamicoffset0);
					// quick fix
					if (mesh->submeshes[sm].textureIndex[0] != INVALID_TEXTURE)
						vkCmdBindDescriptorSets(renderState.m_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, renderState.m_pipelineLayout, 1, 1, &texturedescriptorset[submeshoffset + sm], 0, NULL);
					// Bind triangle indices
					vkCmdBindIndexBuffer(renderState.m_commandBuffers[i], mesh->submeshes[sm].ibv.buffer, mesh->submeshes[sm].ibv.offset, mesh->submeshes[sm].ibv.format);
					// Draw indexed triangle
					vkCmdDrawIndexedIndirect(renderState.m_commandBuffers[i], indirectcommandbuffer, (submeshoffset + sm) * sizeof(VkDrawIndexedIndirectCommand), 1, sizeof(VkDrawIndexedIndirectCommand));
				}
				// Set offset per submesh
				submeshoffset += mesh->submeshCount;
			}
			// Set offset per mesh
			meshoffset += scene->vkmeshCount;
		}

		// Write timestamp
		vkCmdWriteTimestamp(renderState.m_commandBuffers[i], VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, renderState.m_queryPool, 1);
		// End renderpass
		vkCmdEndRenderPass(renderState.m_commandBuffers[i]);
		// End commandbuffer
		VK_CHECK_RESULT(vkEndCommandBuffer(renderState.m_commandBuffers[i]));
	}
}

void StateVoxelizer(
	RenderState& renderState,
	VulkanCore* core,
	VkDescriptorSet staticDescriptorSet,
	VkDescriptorSetLayout staticDescLayout,
	VkDescriptorSetLayout textureDescLayout,
	VKScene* scenes,
	uint32_t scenecount,
	uint32_t griduniformresolution,
	VoxelizerGrid* voxelizergrid)
{
	VkDevice device = core->GetViewDevice();
	SwapChain* swapchain = core->GetSwapChain();
	uint32_t width = swapchain->m_width;
	uint32_t height = swapchain->m_height;
	uniformgridresolution = griduniformresolution;

	CreateBasicRenderstate(renderState, core, 4, 1, 2, AXISCOUNT, true, 1, AXISCOUNT, 1);
	
	////////////////////////////////////////////////////////////////////////////////
	//create the renderpass
	////////////////////////////////////////////////////////////////////////////////
	VkAttachmentDescription attachments = {};
	VkAttachmentReference depthReference = {};
	depthReference.attachment = 0;
	depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	VkSubpassDescription subpass[1] = {};
	subpass[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass[0].flags = 0;
	subpass[0].inputAttachmentCount = 0;
	subpass[0].pInputAttachments = NULL;
	subpass[0].colorAttachmentCount = 0;
	subpass[0].pColorAttachments = NULL;
	subpass[0].pResolveAttachments = NULL;
	subpass[0].pDepthStencilAttachment = NULL;
	subpass[0].preserveAttachmentCount = 0;
	subpass[0].pPreserveAttachments = NULL;
	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.pNext = NULL;
	renderPassInfo.attachmentCount = 0;
	renderPassInfo.pAttachments = NULL;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = subpass;
	renderPassInfo.dependencyCount = 0;
	renderPassInfo.pDependencies = NULL;
	VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassInfo, NULL, &renderState.m_renderpass));

	////////////////////////////////////////////////////////////////////////////////
	// Create the framebuffers
	////////////////////////////////////////////////////////////////////////////////
	//setup create info for framebuffers
	VkFramebufferCreateInfo frameBufferCreateInfo = {};
	frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	frameBufferCreateInfo.pNext = NULL;
	frameBufferCreateInfo.renderPass = renderState.m_renderpass;
	frameBufferCreateInfo.attachmentCount = 0;
	frameBufferCreateInfo.pAttachments = NULL;
	frameBufferCreateInfo.width = griduniformresolution * AXISSCALAR;
	frameBufferCreateInfo.height = griduniformresolution * AXISSCALAR;
	frameBufferCreateInfo.layers = 1;
	VK_CHECK_RESULT(vkCreateFramebuffer(device, &frameBufferCreateInfo, NULL, &renderState.m_framebuffers[0]));

	////////////////////////////////////////////////////////////////////////////////
	// Set the descriptorset layout
	////////////////////////////////////////////////////////////////////////////////
	// Dynamic descriptorset
	VkDescriptorSetLayoutBinding layoutbinding0[VOXELIZER_DESCRIPTOR_COUNT];
	// Binding 0 : Diffuse texture sampled image
	layoutbinding0[VOXELIZER_DESCRIPTOR_ALBEDOOPACITY] =
	{ VOXELIZER_DESCRIPTOR_ALBEDOOPACITY, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL };
	// Binding 1 : Normal texture sampled image
	layoutbinding0[VOXELIZER_DESCRIPTOR_NORMAL] =
	{ VOXELIZER_DESCRIPTOR_NORMAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL };
	// Binding 2: Opacity texture sampled image
	layoutbinding0[VOXELIZER_DESCRIPTOR_EMISSION] =
	{ VOXELIZER_DESCRIPTOR_EMISSION, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL };
	// Binding 3: UBO Geometry
	layoutbinding0[VOXELIZER_DESCRIPTOR_UBO_GEOM] =
	{ VOXELIZER_DESCRIPTOR_UBO_GEOM, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_GEOMETRY_BIT, NULL };
	// Binding 4: UBO Fragment
	layoutbinding0[VOXELIZER_DESCRIPTOR_UBO_FRAG] =
	{ VOXELIZER_DESCRIPTOR_UBO_FRAG, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL };
	
	VkDescriptorSetLayoutCreateInfo descriptorLayout0 = VKTools::Initializers::DescriptorSetLayoutCreateInfo(0, VOXELIZER_DESCRIPTOR_COUNT, layoutbinding0);
	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout0, NULL, &renderState.m_descriptorLayouts[0]));

	////////////////////////////////////////////////////////////////////////////////
	// Create pipeline layout
	////////////////////////////////////////////////////////////////////////////////
	VkDescriptorSetLayout layouts[3] = { staticDescLayout, textureDescLayout, renderState.m_descriptorLayouts[0]};
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = VKTools::Initializers::PipelineLayoutCreateInfo(0, 3, layouts);
	VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &renderState.m_pipelineLayout));

	////////////////////////////////////////////////////////////////////////////////
	// Create descriptor pool
	////////////////////////////////////////////////////////////////////////////////
	VkDescriptorPoolSize poolSize[VOXELIZER_DESCRIPTOR_COUNT];
	poolSize[VOXELIZER_DESCRIPTOR_ALBEDOOPACITY] =	{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, AXISCOUNT };
	poolSize[VOXELIZER_DESCRIPTOR_NORMAL] =			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, AXISCOUNT };
	poolSize[VOXELIZER_DESCRIPTOR_EMISSION] =		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, AXISCOUNT };
	poolSize[VOXELIZER_DESCRIPTOR_UBO_GEOM] =		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, AXISCOUNT };
	poolSize[VOXELIZER_DESCRIPTOR_UBO_FRAG] =		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, AXISCOUNT };
	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = VKTools::Initializers::DescriptorPoolCreateInfo(0, AXISCOUNT, VOXELIZER_DESCRIPTOR_COUNT, poolSize);
	//create the descriptorPool
	VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, NULL, &renderState.m_descriptorPool));

	////////////////////////////////////////////////////////////////////////////////
	// Create the descriptor set
	////////////////////////////////////////////////////////////////////////////////
	for (uint32_t i = 0; i < AXISCOUNT; i++)
	{
		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = VKTools::Initializers::DescriptorSetAllocateInfo(renderState.m_descriptorPool, 1, &renderState.m_descriptorLayouts[0]);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &renderState.m_descriptorSets[i]));
	}

	////////////////////////////////////////////////////////////////////////////////
	// Create the Uniform Data
	////////////////////////////////////////////////////////////////////////////////
	VKTools::CreateBuffer(core, device,
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		sizeof(UBOGeom) * AXISCOUNT,
		NULL,
		&renderState.m_bufferData[0].buffer,
		&renderState.m_bufferData[0].memory,
		&renderState.m_bufferData[0].descriptor);
	renderState.m_bufferData[0].descriptor.range = VK_WHOLE_SIZE;
	VKTools::CreateBuffer(core, device,
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		sizeof(UBOFrag) * AXISCOUNT,
		NULL,
		&renderState.m_bufferData[1].buffer,
		&renderState.m_bufferData[1].memory,
		&renderState.m_bufferData[1].descriptor);
	renderState.m_bufferData[1].descriptor.range = VK_WHOLE_SIZE;

	////////////////////////////////////////////////////////////////////////////////
	// update descriptorsets
	////////////////////////////////////////////////////////////////////////////////
	for (uint32_t i = 0; i < AXISCOUNT; i++)
	{
		VoxelizerGrid& grid = voxelizergrid[i];
		VkWriteDescriptorSet wds[5];
		// dynamic uniform buffers
		wds[0] = VKTools::Initializers::WriteDescriptorSet(
			renderState.m_descriptorSets[i],
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
			VOXELIZER_DESCRIPTOR_UBO_GEOM,
			&renderState.m_bufferData[0].descriptor);
		wds[1] = VKTools::Initializers::WriteDescriptorSet(
			renderState.m_descriptorSets[i],
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
			VOXELIZER_DESCRIPTOR_UBO_FRAG,
			&renderState.m_bufferData[1].descriptor);
		// isotropic textures
		wds[2] = VKTools::Initializers::WriteDescriptorSet(
			renderState.m_descriptorSets[i],
			VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			VOXELIZER_DESCRIPTOR_ALBEDOOPACITY,
			&grid.albedoOpacity.descriptor[0]);
		wds[3] = VKTools::Initializers::WriteDescriptorSet(
			renderState.m_descriptorSets[i],
			VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			VOXELIZER_DESCRIPTOR_NORMAL,
			&grid.normal.descriptor[0]);
		wds[4] = VKTools::Initializers::WriteDescriptorSet(
			renderState.m_descriptorSets[i],
			VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			VOXELIZER_DESCRIPTOR_EMISSION,
			&grid.emission.descriptor[0]);

		vkUpdateDescriptorSets(device, 5, wds, 0, NULL);
	}

	////////////////////////////////////////////////////////////////////////////////
	// Create Pipeline
	////////////////////////////////////////////////////////////////////////////////
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
	pipelineRasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
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
	VkDynamicState dynamicStateEnables[2];
	dynamicStateEnables[0] = VK_DYNAMIC_STATE_VIEWPORT;
	dynamicStateEnables[1] = VK_DYNAMIC_STATE_SCISSOR;
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.pDynamicStates = dynamicStateEnables;
	dynamicState.dynamicStateCount = 2;

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
#define	SHADERNUM 3
	Shader shaderStages[SHADERNUM];
	shaderStages[0] = VKTools::LoadShader("shaders/voxelizer.vert.spv", "main", device, VK_SHADER_STAGE_VERTEX_BIT);
	shaderStages[1] = VKTools::LoadShader("shaders/voxelizer.geom.spv", "main", device, VK_SHADER_STAGE_GEOMETRY_BIT);
	shaderStages[2] = VKTools::LoadShader("shaders/voxelizer.frag.spv", "main", device, VK_SHADER_STAGE_FRAGMENT_BIT);

	// Assign states
	// Assign pipeline state create information
	VkPipelineShaderStageCreateInfo shaderStagesData[SHADERNUM];
	for (int i = 0; i < SHADERNUM; i++)	
		shaderStagesData[i] = shaderStages[i].m_shaderStage;

	// pipelineinfo for creating the pipeline
	VkGraphicsPipelineCreateInfo pipelineCreateInfo[2] = {};
	pipelineCreateInfo[0].sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCreateInfo[0].layout = renderState.m_pipelineLayout;
	pipelineCreateInfo[0].stageCount = SHADERNUM;
	pipelineCreateInfo[0].pStages = shaderStagesData;
	pipelineCreateInfo[0].pVertexInputState = &scenes[0].vertices.inputState;
	pipelineCreateInfo[0].pInputAssemblyState = &pipelineInputAssemblyCreateInfo;
	pipelineCreateInfo[0].pRasterizationState = &pipelineRasterizationStateCreateInfo;
	pipelineCreateInfo[0].pColorBlendState = &colorBlendState;
	pipelineCreateInfo[0].pMultisampleState = &multisampleState;
	pipelineCreateInfo[0].pViewportState = &viewportState;
	pipelineCreateInfo[0].pDepthStencilState = &depthStencilState;
	pipelineCreateInfo[0].renderPass = renderState.m_renderpass;
	pipelineCreateInfo[0].pDynamicState = &dynamicState;
	pipelineCreateInfo[0].subpass = 0;

#undef SHADERNUM

	// Create rendering pipeline
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, renderState.m_pipelineCache, 1, pipelineCreateInfo, NULL, renderState.m_pipelines));

	////////////////////////////////////////////////////////////////////////////////
	// Rebuild the command buffers
	////////////////////////////////////////////////////////////////////////////////
	renderState.m_commandBufferCount = AXISCOUNT;
	renderState.m_commandBuffers = (VkCommandBuffer*)malloc(sizeof(VkCommandBuffer)*renderState.m_commandBufferCount);
	for (uint32_t i = 0; i < renderState.m_commandBufferCount; i++)
		renderState.m_commandBuffers[i] = VKTools::Initializers::CreateCommandBuffer(core->GetGraphicsCommandPool(), device, VK_COMMAND_BUFFER_LEVEL_PRIMARY, false);

}
