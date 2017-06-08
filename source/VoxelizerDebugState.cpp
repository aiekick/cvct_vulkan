#include "PipelineStates.h"

#include "VCTPipelineDefines.h"
#include "SwapChain.h"
#include "VulkanCore.h"
#include "Shader.h"
#include "DataTypes.h"
#include "AnisotropicVoxelTexture.h"
#include "Camera.h"
#include "Defines.h"

struct PushConstantComp
{
	uint32_t cascadeNum;		// Current cascade
};

struct Parameter
{
	AnisotropicVoxelTexture* avt;
	VkBuffer buf;			//device buffer
	VkDeviceMemory mem;		//device memory
	uint32_t vertexSize;
	VkDescriptorSet staticDescriptorSet;
};

void BuildCommandBufferVoxelizerDebugState(
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
	VkBuffer buf = ((Parameter*)parameters)->buf;
	VkDeviceMemory mem = ((Parameter*)parameters)->mem;
	uint32_t vertexSize = ((Parameter*)parameters)->vertexSize;
	VkDescriptorSet staticDescriptorSet = ((Parameter*)parameters)->staticDescriptorSet;

	////////////////////////////////////////////////////////////////////////////////
	// Rebuild the command buffers
	////////////////////////////////////////////////////////////////////////////////
	renderState->m_commandBufferCount = avt->m_cascadeCount * framebufferCount;
	renderState->m_commandBuffers = (VkCommandBuffer*)malloc(sizeof(VkCommandBuffer)*renderState->m_commandBufferCount);
	for (uint32_t i = 0; i < renderState->m_commandBufferCount; i++)
		renderState->m_commandBuffers[i] = VKTools::Initializers::CreateCommandBuffer(commandpool, core->GetViewDevice(), VK_COMMAND_BUFFER_LEVEL_PRIMARY, false);

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
	renderPassBeginInfo.renderPass = renderState->m_renderpass;
	renderPassBeginInfo.renderArea.offset.x = 0;
	renderPassBeginInfo.renderArea.offset.y = 0;
	renderPassBeginInfo.renderArea.extent.width = width;
	renderPassBeginInfo.renderArea.extent.height = height;
	renderPassBeginInfo.clearValueCount = 2;
	renderPassBeginInfo.pClearValues = clearValues;

	for (uint32_t i = 0; i < renderState->m_framebufferCount; i++)
	{
		for (uint32_t j = 0; j < avt->m_cascadeCount; j++)
		{
			// Set target frame buffer
			renderPassBeginInfo.framebuffer = framebuffers[i];

			//begin
			VK_CHECK_RESULT(vkBeginCommandBuffer(renderState->m_commandBuffers[i*avt->m_cascadeCount + j], &cmdBufInfo));

			// Start the first sub pass specified in our default render pass setup by the base class
			// This will clear the color and depth attachment
			vkCmdBeginRenderPass(renderState->m_commandBuffers[i*avt->m_cascadeCount + j], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			// Update dynamic viewport state
			VkViewport viewport = {};
			viewport.width = (float)width;
			viewport.height = (float)height;
			viewport.minDepth = (float) 0.0f;
			viewport.maxDepth = (float) 1.0f;
			vkCmdSetViewport(renderState->m_commandBuffers[i*avt->m_cascadeCount + j], 0, 1, &viewport);

			// Update dynamic scissor state
			VkRect2D scissor = {};
			scissor.extent.width = width;
			scissor.extent.height = height;
			scissor.offset.x = 0;
			scissor.offset.y = 0;
			vkCmdSetScissor(renderState->m_commandBuffers[i*avt->m_cascadeCount + j], 0, 1, &scissor);

			// Submit push constant
			PushConstantComp pc;
			pc.cascadeNum = j;
			vkCmdPushConstants(renderState->m_commandBuffers[i*avt->m_cascadeCount + j], renderState->m_pipelineLayout, VK_SHADER_STAGE_GEOMETRY_BIT, 0, sizeof(PushConstantComp), &pc);

			// Bind the rendering pipeline (including the shaders)
			vkCmdBindPipeline(renderState->m_commandBuffers[i*avt->m_cascadeCount + j], VK_PIPELINE_BIND_POINT_GRAPHICS, renderState->m_pipelines[0]);

			// Bind descriptor sets describing shader binding points
			uint32_t doffset = 0;
			VkDeviceSize offsets[1] = { 0 };
			vkCmdBindDescriptorSets(renderState->m_commandBuffers[i*avt->m_cascadeCount + j], VK_PIPELINE_BIND_POINT_GRAPHICS, renderState->m_pipelineLayout, 0, 1, &staticDescriptorSet, 1, &doffset);
			vkCmdBindDescriptorSets(renderState->m_commandBuffers[i*avt->m_cascadeCount + j], VK_PIPELINE_BIND_POINT_GRAPHICS, renderState->m_pipelineLayout, 1, 1, &renderState->m_descriptorSets[0], 0, NULL);
			vkCmdBindVertexBuffers(renderState->m_commandBuffers[i*avt->m_cascadeCount + j], 0, 1, &buf, offsets);
			vkCmdDraw(renderState->m_commandBuffers[i*avt->m_cascadeCount + j], vertexSize, 1, 0, 0);
			// End renderpass
			vkCmdEndRenderPass(renderState->m_commandBuffers[i*avt->m_cascadeCount + j]);
			// End commandbuffer
			VK_CHECK_RESULT(vkEndCommandBuffer(renderState->m_commandBuffers[i*avt->m_cascadeCount + j]));
		}
	}
}

void CreateVoxelRenderDebugState(	// Voxel renderer (debugging purposes) pipeline state
	RenderState& renderState,
	VkFramebuffer* framebuffers,
	uint32_t framebufferCount,
	VulkanCore* core,
	VkCommandPool commandPool,
	SwapChain* swapchain,
	VkDescriptorSet staticDescriptorSet,
	VkDescriptorSetLayout staticDescLayout,
	Camera* camera,
	AnisotropicVoxelTexture* avt)
{
	uint32_t width = swapchain->m_width;
	uint32_t height = swapchain->m_height;
	VkDevice device = core->GetViewDevice();

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
	if (!renderState.m_framebuffers)
	{
		renderState.m_framebufferCount = framebufferCount;
		renderState.m_framebuffers = NULL;
	}

	////////////////////////////////////////////////////////////////////////////////
	// Create the renderpass
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
		attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;							// Clear depth at start of first subpass
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
	// Create semaphores
	////////////////////////////////////////////////////////////////////////////////
	if (!renderState.m_semaphores)
	{
		renderState.m_semaphoreCount = 1;
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
		renderState.m_uniformData = (UniformData*)malloc(sizeof(UniformData) * renderState.m_uniformDataCount);
		VKTools::CreateBuffer(core, device,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			sizeof(VoxelizerDebugUBOGeom),
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
		renderState.m_descriptorLayoutCount = 1;
		renderState.m_descriptorLayouts = (VkDescriptorSetLayout*)malloc(renderState.m_descriptorLayoutCount * sizeof(VkDescriptorSetLayout));
		// Dynamic descriptorset
		VkDescriptorSetLayoutBinding layoutBinding[VOXELIZERDEBUG_DESCRIPTOR_COUNT];
		// Binding 0: 3D voxel textures
		layoutBinding[VOXELIZERDEBUG_DESCRIPTOR_VOXELGRID] =
		{ VOXELIZERDEBUG_DESCRIPTOR_VOXELGRID, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_GEOMETRY_BIT, NULL };
		// Binding 1: Geometry UBO
		layoutBinding[VOXELIZERDEBUG_DESCRIPTOR_BUFFER_GEOM] =
		{ VOXELIZERDEBUG_DESCRIPTOR_BUFFER_GEOM , VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER , 1, VK_SHADER_STAGE_GEOMETRY_BIT , NULL };
		
		VkDescriptorSetLayoutCreateInfo descriptorLayout = VKTools::Initializers::DescriptorSetLayoutCreateInfo(0, VOXELIZERDEBUG_DESCRIPTOR_COUNT, layoutBinding);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout, NULL, &renderState.m_descriptorLayouts[0]));
	}

	////////////////////////////////////////////////////////////////////////////////
	// Create pipeline layout
	////////////////////////////////////////////////////////////////////////////////
	if (!renderState.m_pipelineLayout)
	{
		VkDescriptorSetLayout dLayouts[] = { staticDescLayout, renderState.m_descriptorLayouts[0] };
		VkPushConstantRange pushConstantRange = VKTools::Initializers::PushConstantRange(VK_SHADER_STAGE_GEOMETRY_BIT, 0, sizeof(PushConstantComp));
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = VKTools::Initializers::PipelineLayoutCreateInfo(0, 2, dLayouts);
		pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
		pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, NULL, &renderState.m_pipelineLayout));
	}

	////////////////////////////////////////////////////////////////////////////////
	// Create descriptor pool
	////////////////////////////////////////////////////////////////////////////////
	if (!renderState.m_descriptorPool)
	{
		VkDescriptorPoolSize poolSize[VOXELIZERDEBUG_DESCRIPTOR_COUNT];
		poolSize[VOXELIZERDEBUG_DESCRIPTOR_VOXELGRID] = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 6 };
		poolSize[VOXELIZERDEBUG_DESCRIPTOR_BUFFER_GEOM] = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER , 1 };
		VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = VKTools::Initializers::DescriptorPoolCreateInfo(0, 1, VOXELIZERDEBUG_DESCRIPTOR_COUNT, poolSize);
		//create the descriptorPool
		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, NULL, &renderState.m_descriptorPool));
	}

	////////////////////////////////////////////////////////////////////////////////
	// Create the descriptor set
	////////////////////////////////////////////////////////////////////////////////
	if (!renderState.m_descriptorSets)
	{
		//allocate the requirered descriptorsets
		renderState.m_descriptorSetCount = 1;
		renderState.m_descriptorSets = (VkDescriptorSet*)malloc(renderState.m_descriptorSetCount * sizeof(VkDescriptorSet));
		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = VKTools::Initializers::DescriptorSetAllocateInfo(renderState.m_descriptorPool, 1, &renderState.m_descriptorLayouts[0]);
		//allocate the descriptorset with the pool
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &renderState.m_descriptorSets[0]));

		///////////////////////////////////////////////////////
		///// Set/Update the image and uniform buffer descriptorsets
		/////////////////////////////////////////////////////// 
		VkWriteDescriptorSet writeDescriptorSet = {};
		VkWriteDescriptorSet wds = {};
		// Update the geometry descriptorset
		writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSet.pNext = NULL;
		writeDescriptorSet.dstSet = renderState.m_descriptorSets[0];
		writeDescriptorSet.dstBinding = VOXELIZERDEBUG_DESCRIPTOR_BUFFER_GEOM;
		writeDescriptorSet.dstArrayElement = 0;
		writeDescriptorSet.descriptorCount = 1;
		writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writeDescriptorSet.pImageInfo = NULL;
		writeDescriptorSet.pBufferInfo = &renderState.m_uniformData[0].m_descriptor;
		writeDescriptorSet.pTexelBufferView = NULL;
		vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, NULL);
		// Update the voxelgrid descriptorset
		{
			wds.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			wds.pNext = NULL;
			wds.dstSet = renderState.m_descriptorSets[0];
			wds.dstBinding = VOXELIZERDEBUG_DESCRIPTOR_VOXELGRID;
			wds.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			wds.descriptorCount = 1;
			wds.dstArrayElement = 0;
			wds.pImageInfo = avt->m_descriptor;
			//update the descriptorset
			vkUpdateDescriptorSets(device, 1, &wds, 0, NULL);
		}
	}
	////////////////////////////////////////////////////////////////////////////////
	// Create Pipeline
	////////////////////////////////////////////////////////////////////////////////
	Vertices vertices;
	if (!renderState.m_pipelines)
	{
		renderState.m_pipelineCount = 1;
		renderState.m_pipelines = (VkPipeline*)malloc(renderState.m_pipelineCount * sizeof(VkPipeline));

		// Create the pipeline input assembly state info
		VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyCreateInfo = {};
		pipelineInputAssemblyCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		// This pipeline renders vertex data as triangles
		pipelineInputAssemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
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
		//color blend state
		VkPipelineColorBlendStateCreateInfo colorBlendState = {};
		colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlendState.attachmentCount = 1;
		//don't enable blending for now
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
#define	SHADERNUM 3
		Shader shaderStages[SHADERNUM];
		shaderStages[0] = VKTools::LoadShader("shaders/voxelizerdebug.vert.spv", "main", device, VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = VKTools::LoadShader("shaders/voxelizerdebug.geom.spv", "main", device, VK_SHADER_STAGE_GEOMETRY_BIT);
		shaderStages[2] = VKTools::LoadShader("shaders/voxelizerdebug.frag.spv", "main", device, VK_SHADER_STAGE_FRAGMENT_BIT);

		// Assign states
		// Assign pipeline state create information
		std::vector<VkPipelineShaderStageCreateInfo> shaderStagesData;
		for (int i = 0; i < SHADERNUM; i++)
			shaderStagesData.push_back(shaderStages[i].m_shaderStage);
#undef SHADERNUM

		///////////////////////////////////////////////////////
		// Set vertices attributes
		/////////////////////////////////////////////////////// 
		// Set binding description
		vertices.bindingDescriptions.resize(1);
		// Location 0 : Position
		vertices.bindingDescriptions[0].binding = (uint32_t)0;
		vertices.bindingDescriptions[0].stride = sizeof(uint32_t);
		vertices.bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		// Attribute descriptions
		// Describes memory layout and shader attribute locations
		vertices.attributeDescriptions.resize(1);
		// Location 0 : Position
		vertices.attributeDescriptions[0].binding = (uint32_t)0;
		vertices.attributeDescriptions[0].location = 0;
		vertices.attributeDescriptions[0].format = VK_FORMAT_R32_UINT;
		vertices.attributeDescriptions[0].offset = 0;
		// Assign to vertex input state
		vertices.inputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertices.inputState.pNext = NULL;
		vertices.inputState.flags = VK_FLAGS_NONE;
		vertices.inputState.vertexBindingDescriptionCount = (uint32_t)vertices.bindingDescriptions.size();
		vertices.inputState.pVertexBindingDescriptions = vertices.bindingDescriptions.data();
		vertices.inputState.vertexAttributeDescriptionCount = (uint32_t)vertices.attributeDescriptions.size();
		vertices.inputState.pVertexAttributeDescriptions = vertices.attributeDescriptions.data();

		// pipelineinfo for creating the pipeline
		VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
		pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineCreateInfo.layout = renderState.m_pipelineLayout;
		pipelineCreateInfo.stageCount = (uint32_t)shaderStagesData.size();
		pipelineCreateInfo.pStages = shaderStagesData.data();
		pipelineCreateInfo.pVertexInputState = &vertices.inputState;
		pipelineCreateInfo.pInputAssemblyState = &pipelineInputAssemblyCreateInfo;
		pipelineCreateInfo.pRasterizationState = &pipelineRasterizationStateCreateInfo;
		pipelineCreateInfo.pColorBlendState = &colorBlendState;
		pipelineCreateInfo.pMultisampleState = &multisampleState;
		pipelineCreateInfo.pViewportState = &viewportState;
		pipelineCreateInfo.pDepthStencilState = &depthStencilState;
		pipelineCreateInfo.renderPass = renderState.m_renderpass;
		pipelineCreateInfo.pDynamicState = &dynamicState;

		// Create rendering pipeline
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, renderState.m_pipelineCache, 1, &pipelineCreateInfo, NULL, &renderState.m_pipelines[0]));
	
		///////////////////////////////////////////////////////
		/////Create Vulkan specific buffers
		/////////////////////////////////////////////////////// 
		// Create the device specific and staging buffer
		VkResult result;
		uint32_t vertexSize = avt->m_width*avt->m_height*avt->m_depth;
		uint64_t bufferSizeByte = vertexSize * sizeof(uint32_t);

		uint32_t* vertexBuffer;
		vertexBuffer = (uint32_t*)malloc(bufferSizeByte);

		for (uint32_t i = 0; i < vertexSize; i++)
			vertexBuffer[i] = i;

		VkBufferCreateInfo bufferInfo = {};
		VkBuffer sceneBuffer, sceneStageBuffer;
		VkMemoryRequirements bufferMemoryRequirements, stagingMemoryRequirements;

		// Create the stagingd
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.pNext = NULL;
		bufferInfo.size = bufferSizeByte;
		bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		bufferInfo.flags = 0;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		result = vkCreateBuffer(device, &bufferInfo, NULL, &sceneStageBuffer);

		// Create the device buffer
		bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
		result = vkCreateBuffer(device, &bufferInfo, NULL, &sceneBuffer);

		// Set memory requirements
		vkGetBufferMemoryRequirements(device, sceneBuffer, &bufferMemoryRequirements);
		vkGetBufferMemoryRequirements(device, sceneStageBuffer, &stagingMemoryRequirements);
		uint32_t bufferTypeIndex = core->GetMemoryType(bufferMemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		uint32_t stagingTypeIndex = core->GetMemoryType(stagingMemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

		if (!bufferTypeIndex || !stagingTypeIndex)
			ERROR_VOID("No compatible memory type");

		// Creating the memory
		VkDeviceMemory bufferDeviceMemory, stagingDeviceMemory;
		VkMemoryAllocateInfo allocInfo;

		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.pNext = NULL;
		allocInfo.allocationSize = bufferMemoryRequirements.size;
		allocInfo.memoryTypeIndex = bufferTypeIndex;
		result = vkAllocateMemory(device, &allocInfo, NULL, &bufferDeviceMemory);
		if (result != VkResult::VK_SUCCESS)
			ERROR_VOID("not able to allocate buffer device memory");

		allocInfo.allocationSize = stagingMemoryRequirements.size;
		allocInfo.memoryTypeIndex = stagingTypeIndex;
		result = vkAllocateMemory(device, &allocInfo, NULL, &stagingDeviceMemory);
		if (result != VkResult::VK_SUCCESS)
			ERROR_VOID("not able to allocate staging device memory");

		// Bind all the memory
		result = vkBindBufferMemory(device, sceneBuffer, bufferDeviceMemory, 0);
		if (result != VkResult::VK_SUCCESS)
			ERROR_VOID("Not able to bind buffer with device memory");
		result = vkBindBufferMemory(device, sceneStageBuffer, stagingDeviceMemory, 0);
		if (result != VkResult::VK_SUCCESS)
			ERROR_VOID("Not able to bind staging with device memory");

		uint8_t* dst = NULL;
		result = vkMapMemory(device, stagingDeviceMemory, 0, bufferSizeByte, 0, (void**)&dst);

		if (result != VK_SUCCESS)
			ERROR_VOID("unable to map destination pointer");

		// Copy the vertex and indices data to the destination
		memcpy(dst, vertexBuffer, bufferSizeByte);
		vkUnmapMemory(device, stagingDeviceMemory);
		// Copy command
		VkCommandBuffer uploadCmdBuffer = VKTools::Initializers::CreateCommandBuffer(commandPool, device, VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		VkBufferCopy bufferCopy = { 0,0, bufferSizeByte };
		vkCmdCopyBuffer(uploadCmdBuffer, sceneStageBuffer, sceneBuffer, 1, &bufferCopy);
		VKTools::FlushCommandBuffer(uploadCmdBuffer, core->GetGraphicsQueue(), device, commandPool, true);

		vertices.buf = sceneBuffer;
		vertices.mem = bufferDeviceMemory;
		//todo: clean vertices
	}
	
	////////////////////////////////////////////////////////////////////////////////
	// Build command buffers
	////////////////////////////////////////////////////////////////////////////////
	Parameter* parameter;
	parameter = (Parameter*)malloc(sizeof(Parameter));
	parameter->avt = avt;
	parameter->staticDescriptorSet = staticDescriptorSet;
	parameter->vertexSize = avt->m_width*avt->m_height*avt->m_depth;
	parameter->buf = vertices.buf;
	parameter->mem = vertices.mem;
	renderState.m_cmdBufferParameters = (BYTE*)parameter;

	renderState.m_CreateCommandBufferFunc = &BuildCommandBufferVoxelizerDebugState;
	renderState.m_CreateCommandBufferFunc(&renderState, commandPool, core, framebufferCount, framebuffers, renderState.m_cmdBufferParameters);
}