#include "PipelineStates.h"

#include "VCTPipelineDefines.h"
#include "SwapChain.h"
#include "VulkanCore.h"
#include "Shader.h"
#include "DataTypes.h"

Vertices vertices;

void CommandIndirectDebugRenderer(
	RenderState& renderstate,
	VulkanCore* core,
	VkDescriptorSet staticDescriptorSet,
	VkBuffer indirectcommandbuffer)
{
	RenderState& renderState = renderstate;
	uint32_t width = core->GetSwapChain()->m_width;
	uint32_t height = core->GetSwapChain()->m_height;
	VkDevice device = core->GetViewDevice();
	uint32_t framebufferCount = core->GetFramebufferCount();
	VkRenderPass& renderpass = renderstate.m_renderpass;
	const VkFramebuffer* framebuffers = core->GetFramebuffers();

	////////////////////////////////////////////////////////////////////////////////
	// Rebuild the command buffers
	////////////////////////////////////////////////////////////////////////////////
	renderState.m_commandBufferCount = framebufferCount;
	renderState.m_commandBuffers = (VkCommandBuffer*)malloc(sizeof(VkCommandBuffer)*renderState.m_commandBufferCount);
	for (uint32_t i = 0; i < renderState.m_commandBufferCount; i++)
		renderState.m_commandBuffers[i] = VKTools::Initializers::CreateCommandBuffer(core->GetComputeCommandPool(), core->GetViewDevice(), VK_COMMAND_BUFFER_LEVEL_PRIMARY, false);

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


	uint32_t dynamicoffset[2];
	dynamicoffset[0] = 0;
	dynamicoffset[1] = 0;
	VkDeviceSize offset = 0;
	for (uint32_t i = 0; i < renderState.m_commandBufferCount; i++)
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

		vkCmdBindPipeline(renderState.m_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, renderState.m_pipelines[0]);
		vkCmdBindDescriptorSets(renderState.m_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, renderState.m_pipelineLayout, 0, 1, &staticDescriptorSet, 2, dynamicoffset);
		vkCmdBindVertexBuffers(renderState.m_commandBuffers[i], 0, 1, &vertices.buf, &offset);

		// indirect
		vkCmdDrawIndirect(renderState.m_commandBuffers[i], indirectcommandbuffer, 0, 1, sizeof(VkDrawIndirectCommand));

		vkCmdWriteTimestamp(renderState.m_commandBuffers[i], VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, renderState.m_queryPool, 1);
		// End renderpass
		vkCmdEndRenderPass(renderState.m_commandBuffers[i]);
		// End commandbuffer
		vkEndCommandBuffer(renderState.m_commandBuffers[i]);
	}
}

void StateDebugRenderer(
	RenderState& renderstate,
	VulkanCore* core,
	VkDescriptorSetLayout staticDescLayout,
	BufferObject& debugbufferobject)
{
	RenderState& renderState = renderstate;
	uint32_t width = core->GetSwapChain()->m_width;
	uint32_t height = core->GetSwapChain()->m_height;
	VkDevice device = core->GetViewDevice();
	uint32_t framebufferCount = core->GetFramebufferCount();
	VkRenderPass renderpass = core->GetRenderpass();
	const VkFramebuffer* framebuffers = core->GetFramebuffers();

	CreateBasicRenderstate(renderState, core, 4, 0, 0, 1, true, 0, 0, 1);
	renderstate.m_renderpass = NULL;

	////////////////////////////////////////////////////////////////////////////////
	// Create vertex attributes
	////////////////////////////////////////////////////////////////////////////////
	// todo: free this
	vertices.bindingDescriptionCount = 1;
	vertices.attributeDescriptionCount = 3;
	vertices.bindingDescriptions = (VkVertexInputBindingDescription*)malloc(sizeof(VkVertexInputBindingDescription) * vertices.bindingDescriptionCount);
	vertices.attributeDescriptions = (VkVertexInputAttributeDescription*)malloc(sizeof(VkVertexInputAttributeDescription) * vertices.attributeDescriptionCount);
	// Location 0 : position, size, color
	vertices.bindingDescriptions[0].binding = 0;
	vertices.bindingDescriptions[0].stride = sizeof(DebugBox);
	vertices.bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	// Attribute descriptions
	// Describes memory layout and shader attribute locations
	// Location 0 : Position
	vertices.attributeDescriptions[0].binding = 0;
	vertices.attributeDescriptions[0].location = 0;
	vertices.attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	vertices.attributeDescriptions[0].offset = 0;
	// Location 1 : Texcoord
	vertices.attributeDescriptions[1].binding = 0;
	vertices.attributeDescriptions[1].location = 1;
	vertices.attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
	vertices.attributeDescriptions[1].offset = sizeof(float) * 4;
	//location 2 : Normal
	vertices.attributeDescriptions[2].binding = 0;
	vertices.attributeDescriptions[2].location = 2;
	vertices.attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
	vertices.attributeDescriptions[2].offset = sizeof(float) * 8;
	// Assign to vertex input state
	vertices.inputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertices.inputState.pNext = NULL;
	vertices.inputState.flags = VK_FLAGS_NONE;
	vertices.inputState.vertexBindingDescriptionCount = vertices.bindingDescriptionCount;
	vertices.inputState.pVertexBindingDescriptions = vertices.bindingDescriptions;
	vertices.inputState.vertexAttributeDescriptionCount = vertices.attributeDescriptionCount;
	vertices.inputState.pVertexAttributeDescriptions = vertices.attributeDescriptions;
	// Set info
	vertices.buf = debugbufferobject.buffer;
	vertices.mem = debugbufferobject.memory;

	////////////////////////////////////////////////////////////////////////////////
	// Create layout
	////////////////////////////////////////////////////////////////////////////////
	VkDescriptorSetLayout layouts[] = { staticDescLayout };
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = VKTools::Initializers::PipelineLayoutCreateInfo(0, 1, layouts);
	VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, NULL, &renderState.m_pipelineLayout));

	////////////////////////////////////////////////////////////////////////////////
	// Create renderpass
	////////////////////////////////////////////////////////////////////////////////
	if (renderState.m_renderpass == VK_NULL_HANDLE)
	{
		VkAttachmentDescription attachments[2] = {};
		// Color attachment
		attachments[0].format = core->m_colorFormat;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;									// We don't use multi sampling in this example
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;							// Clear this attachment at the start of the render pass
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;							// Keep it's contents after the render pass is finished (for displaying it)
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;					// We don't use stencil, so don't care for load
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;				// Same for store
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;						// Layout at render pass start. Initial doesn't matter, so we use undefined
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;					// Layout to which the attachment is transitioned when the render pass is finished
																						// As we want to present the color buffer to the swapchain, we transition to PRESENT_KHR	
		attachments[1].format = core->m_depthFormat;
		attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;								// load the depth
		attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;						// We don't need depth after render pass has finished (DONT_CARE may result in better performance)
		attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;					// No stencil
		attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;				// No Stencil
		attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;						// Layout at render pass start. Initial doesn't matter, so we use undefined
		attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;	// Transition to depth/stencil attachment
	
		VkAttachmentReference colorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
		VkAttachmentReference depthReference = { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.flags = 0;
		subpass.inputAttachmentCount = 0;
		subpass.pInputAttachments = NULL;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorReference;
		subpass.pResolveAttachments = NULL;
		subpass.pDepthStencilAttachment = &depthReference;
		subpass.preserveAttachmentCount = 0;
		subpass.pPreserveAttachments = NULL;
	
		VkSubpassDependency dependencies[2];
		dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[0].dstSubpass = 0;
		dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
	
		dependencies[1].srcSubpass = 0;
		dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
	
		VkRenderPassCreateInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.pNext = NULL;
		renderPassInfo.attachmentCount = 2;
		renderPassInfo.pAttachments = attachments;
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 2;
		renderPassInfo.pDependencies = dependencies;
	
		VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassInfo, NULL, &renderState.m_renderpass));
	}

	////////////////////////////////////////////////////////////////////////////////
	// Create Pipeline
	////////////////////////////////////////////////////////////////////////////////
	// Create the pipeline input assembly state info
	VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyCreateInfo = {};
	pipelineInputAssemblyCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	// This pipeline renders vertex data as triangles
	pipelineInputAssemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

	// Rasterization state
	VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo = {};
	pipelineRasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	// Solid polygon mode
	pipelineRasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_LINE;
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
	Shader shaderStages[3];
	shaderStages[0] = VKTools::LoadShader("shaders/debugboxrenderer.vert.spv", "main", device, VK_SHADER_STAGE_VERTEX_BIT);
	shaderStages[1] = VKTools::LoadShader("shaders/debugboxrenderer.geom.spv", "main", device, VK_SHADER_STAGE_GEOMETRY_BIT);
	shaderStages[2] = VKTools::LoadShader("shaders/debugboxrenderer.frag.spv", "main", device, VK_SHADER_STAGE_FRAGMENT_BIT);

	// Assign states
	// Assign pipeline state create information
	VkPipelineShaderStageCreateInfo shaderStagesData[3];
	for (int i = 0; i < 3; i++)
	{
		shaderStagesData[i] = shaderStages[i].m_shaderStage;
	}

	// pipelineinfo for creating the pipeline
	VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
	pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCreateInfo.layout = renderState.m_pipelineLayout;
	pipelineCreateInfo.stageCount = 3;
	pipelineCreateInfo.pStages = shaderStagesData;
	pipelineCreateInfo.pVertexInputState = &vertices.inputState;
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