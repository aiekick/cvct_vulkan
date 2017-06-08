#include "PipelineStates.h"

#include <array>

#include "VCTPipelineDefines.h"
#include "SwapChain.h"
#include "VulkanCore.h"
#include "Shader.h"
#include "DataTypes.h"


void CommandIndirectForwardRender(
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
	uint32_t width = core->GetSwapChain()->m_width;
	uint32_t height = core->GetSwapChain()->m_height;
	VkDevice device = core->GetViewDevice();
	uint32_t framebufferCount = core->GetFramebufferCount();
	VkRenderPass renderpass = core->GetRenderpass();
	const VkFramebuffer* framebuffers = core->GetFramebuffers();

	////////////////////////////////////////////////////////////////////////////////
	// Rebuild the command buffers
	////////////////////////////////////////////////////////////////////////////////
	renderState.m_commandBufferCount = framebufferCount;
	renderState.m_commandBuffers = (VkCommandBuffer*)malloc(sizeof(VkCommandBuffer)*renderState.m_commandBufferCount);
	for (uint32_t i = 0; i < renderState.m_commandBufferCount; i++)
		renderState.m_commandBuffers[i] = VKTools::Initializers::CreateCommandBuffer(core->GetGraphicsCommandPool(), device, VK_COMMAND_BUFFER_LEVEL_PRIMARY, false);

	////////////////////////////////////////////////////////////////////////////////
	// Record command buffer
	////////////////////////////////////////////////////////////////////////////////
	VkCommandBufferBeginInfo cmdBufInfo = {};
	cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBufInfo.pNext = NULL;

	VkClearValue clearValues[2];
	VkClearColorValue defaultClearColor = { { 0.025f, 0.025f, 0.025f, 1.0f } };
	clearValues[0].color = defaultClearColor;
	clearValues[1].depthStencil = { 1.0f, 0 };

	VkRenderPassBeginInfo renderPassBeginInfo = {};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.pNext = NULL;
	renderPassBeginInfo.renderPass = renderpass;
	renderPassBeginInfo.renderArea.offset.x = 0;
	renderPassBeginInfo.renderArea.offset.y = 0;
	renderPassBeginInfo.renderArea.extent.width = width;
	renderPassBeginInfo.renderArea.extent.height = height;
	renderPassBeginInfo.clearValueCount = 2;
	renderPassBeginInfo.pClearValues = clearValues;

	for (uint32_t i = 0; i < framebufferCount; i++)
	{
		// Set target frame buffer
		renderPassBeginInfo.framebuffer = framebuffers[i];

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
		uint32_t dynamicoffset[2];
		// Bind descriptor sets describing shader binding points
		uint32_t submeshoffset = 0;
		uint32_t meshoffset = 0;
		for (uint32_t s = 0; s < sceneCount; s++)
		{
			VKScene* scene = &scenes[s];
			dynamicoffset[0] = s * sizeof(PerSceneUBO);

			for (uint32_t m = 0; m < scene->vkmeshCount; m++)
			{
				VKMesh* mesh = &meshes[meshoffset + m];
				vkCmdBindVertexBuffers(renderState.m_commandBuffers[i], 0, mesh->vbvCount, mesh->vertexResources, mesh->vertexOffsets);

				for (uint32_t sm = 0; sm < mesh->submeshCount; sm++)
				{
					dynamicoffset[1] = (submeshoffset + sm) * sizeof(PerSubMeshUBO);
					vkCmdBindDescriptorSets(renderState.m_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, renderState.m_pipelineLayout, 0, 1, &staticdescriptorset, 2, dynamicoffset);
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

void CommandForwardRenderer(
	RenderState& renderstate,
	VulkanCore* core,
	VKScene* scenes,
	uint32_t sceneCount,
	VKMesh* meshes,
	uint32_t meshCount,
	VkDescriptorSet staticdescriptorset,
	VkDescriptorSet* texturedescriptorset)
{
	RenderState& renderState = renderstate;
	uint32_t width = core->GetSwapChain()->m_width;
	uint32_t height = core->GetSwapChain()->m_height;
	VkDevice device = core->GetViewDevice();
	uint32_t framebufferCount = core->GetFramebufferCount();
	VkRenderPass renderpass = core->GetRenderpass();
	const VkFramebuffer* framebuffers = core->GetFramebuffers();

	////////////////////////////////////////////////////////////////////////////////
	// Rebuild the command buffers
	////////////////////////////////////////////////////////////////////////////////
	renderState.m_commandBufferCount = framebufferCount;
	renderState.m_commandBuffers = (VkCommandBuffer*)malloc(sizeof(VkCommandBuffer)*renderState.m_commandBufferCount);
	for (uint32_t i = 0; i < renderState.m_commandBufferCount; i++)
		renderState.m_commandBuffers[i] = VKTools::Initializers::CreateCommandBuffer(core->GetGraphicsCommandPool(), device, VK_COMMAND_BUFFER_LEVEL_PRIMARY, false);

	////////////////////////////////////////////////////////////////////////////////
	// Record command buffer
	////////////////////////////////////////////////////////////////////////////////
	VkCommandBufferBeginInfo cmdBufInfo = {};
	cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBufInfo.pNext = NULL;

	VkClearValue clearValues[2];
	VkClearColorValue defaultClearColor = { { 0.025f, 0.025f, 0.025f, 1.0f } };
	clearValues[0].color = defaultClearColor;
	clearValues[1].depthStencil = { 1.0f, 0 };

	VkRenderPassBeginInfo renderPassBeginInfo = {};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.pNext = NULL;
	renderPassBeginInfo.renderPass = renderpass;
	renderPassBeginInfo.renderArea.offset.x = 0;
	renderPassBeginInfo.renderArea.offset.y = 0;
	renderPassBeginInfo.renderArea.extent.width = width;
	renderPassBeginInfo.renderArea.extent.height = height;
	renderPassBeginInfo.clearValueCount = 2;
	renderPassBeginInfo.pClearValues = clearValues;

	for (uint32_t i = 0; i < framebufferCount; i++)
	{
		// Set target frame buffer
		renderPassBeginInfo.framebuffer = framebuffers[i];

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
		uint32_t dynamicoffset[2];
		// Bind descriptor sets describing shader binding points
		uint32_t submeshoffset = 0;
		uint32_t meshoffset = 0;
		for (uint32_t s = 0; s < sceneCount; s++)
		{
			VKScene* scene = &scenes[s];
			dynamicoffset[0] = s * sizeof(PerSceneUBO);

			for (uint32_t m = 0; m < scene->vkmeshCount; m++)
			{ 
				VKMesh* mesh = &meshes[meshoffset+m];
				vkCmdBindVertexBuffers(renderState.m_commandBuffers[i], 0, mesh->vbvCount, mesh->vertexResources, mesh->vertexOffsets);

				for (uint32_t sm = 0; sm < mesh->submeshCount; sm++)
				{
					dynamicoffset[1] = (submeshoffset + sm) * sizeof(PerSubMeshUBO);
					vkCmdBindDescriptorSets(renderState.m_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, renderState.m_pipelineLayout, 0, 1, &staticdescriptorset, 2, dynamicoffset);
					// quick fix
					if(mesh->submeshes[sm].textureIndex[0] != INVALID_TEXTURE)
						vkCmdBindDescriptorSets(renderState.m_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, renderState.m_pipelineLayout, 1, 1, &texturedescriptorset[submeshoffset + sm], 0, NULL);
					// Bind triangle indices
					vkCmdBindIndexBuffer(renderState.m_commandBuffers[i], mesh->submeshes[sm].ibv.buffer, mesh->submeshes[sm].ibv.offset, mesh->submeshes[sm].ibv.format);
					// Draw indexed triangle
					vkCmdDrawIndexed(renderState.m_commandBuffers[i], (uint32_t)mesh->submeshes[sm].ibv.count, 1, 0, 0, 0);
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

void StateForwardRenderer(
	RenderState& renderState,
	VulkanCore* core,
	VkDescriptorSet staticDescriptorSet,
	VkDescriptorSetLayout staticDescLayout,
	VkDescriptorSetLayout textureDescLayout,
	VKScene* scenes,
	uint32_t scenecount)		// Forward renderer pipeline state
{
	VkDevice device = core->GetViewDevice();
	SwapChain* swapchain = core->GetSwapChain();
	uint32_t width = swapchain->m_width;
	uint32_t height = swapchain->m_height;
	VkRenderPass renderpass = core->GetRenderpass();

	CreateBasicRenderstate(renderState, core, 4, 0, 0, 1, true, 0, 0, 1);
	renderState.m_renderpass = NULL;
	renderState.m_descriptorPool = VK_NULL_HANDLE;

	////////////////////////////////////////////////////////////////////////////////
	// Create layout
	////////////////////////////////////////////////////////////////////////////////
	VkDescriptorSetLayout layouts[] = {staticDescLayout, textureDescLayout };
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = VKTools::Initializers::PipelineLayoutCreateInfo(0, 2, layouts);
	VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, NULL, &renderState.m_pipelineLayout));

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
	// Enable culling
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
	colorBlendState.attachmentCount = 1;
	// Don't enable blending for now
	//blending is not used in this example
	VkPipelineColorBlendAttachmentState blendAttatchmentState[1] = {};
	blendAttatchmentState[0].colorWriteMask = 0xf;	//white
	blendAttatchmentState[0].blendEnable = VK_FALSE;
	colorBlendState.pAttachments = blendAttatchmentState;

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
	depthStencilState.depthTestEnable = VK_TRUE;
	depthStencilState.depthWriteEnable = VK_TRUE;
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
	Shader shaderStages[2];
	shaderStages[0] = VKTools::LoadShader("shaders/diffuse.vert.spv", "main", device, VK_SHADER_STAGE_VERTEX_BIT);
	shaderStages[1] = VKTools::LoadShader("shaders/diffuse.frag.spv", "main", device, VK_SHADER_STAGE_FRAGMENT_BIT);

	// Assign states
	// Assign pipeline state create information
	VkPipelineShaderStageCreateInfo shaderStagesData[2];
	for (int i = 0; i < 2; i++)
		shaderStagesData[i] = shaderStages[i].m_shaderStage;

	// pipelineinfo for creating the pipeline
	VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
	pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCreateInfo.layout = renderState.m_pipelineLayout;
	pipelineCreateInfo.stageCount = 2;
	pipelineCreateInfo.pStages = shaderStagesData;
	pipelineCreateInfo.pVertexInputState = &scenes[0].vertices.inputState;
	pipelineCreateInfo.pInputAssemblyState = &pipelineInputAssemblyCreateInfo;
	pipelineCreateInfo.pRasterizationState = &pipelineRasterizationStateCreateInfo;
	pipelineCreateInfo.pColorBlendState = &colorBlendState;
	pipelineCreateInfo.pMultisampleState = &multisampleState;
	pipelineCreateInfo.pViewportState = &viewportState;
	pipelineCreateInfo.pDepthStencilState = &depthStencilState;
	pipelineCreateInfo.renderPass = renderpass;
	pipelineCreateInfo.pDynamicState = &dynamicState;

	// Create rendering pipeline
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, renderState.m_pipelineCache, 1, &pipelineCreateInfo, NULL, &renderState.m_pipelines[0]));
}
