#include "PipelineStates.h"

#include "VCTPipelineDefines.h"
#include "SwapChain.h"
#include "VulkanCore.h"
#include "Shader.h"
#include "DataTypes.h"
#include "AnisotropicVoxelTexture.h"

struct Parameter
{
	vk_mesh_s* meshes;
	uint32_t meshCount;
	VkDescriptorSet staticDescriptorSet;
	float scale;
};

void BuildCommandBufferDeferredMainRenderState(
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
	vk_mesh_s* meshes = ((Parameter*)parameters)->meshes;
	uint32_t meshCount = ((Parameter*)parameters)->meshCount;
	VkDescriptorSet staticDescriptorSet = ((Parameter*)parameters)->staticDescriptorSet;
	float scale = ((Parameter*)parameters)->scale;

	uint32_t widthScaled = uint32_t(core->GetSwapChain()->m_width * scale);
	uint32_t heightScaled = uint32_t(core->GetSwapChain()->m_height * scale);

	////////////////////////////////////////////////////////////////////////////////
	// Rebuild the command buffers
	////////////////////////////////////////////////////////////////////////////////
	renderState->m_commandBufferCount = framebufferCount;
	renderState->m_commandBuffers = (VkCommandBuffer*)malloc(sizeof(VkCommandBuffer)*renderState->m_commandBufferCount);
	for (uint32_t i = 0; i < renderState->m_commandBufferCount; i++)
		renderState->m_commandBuffers[i] = VKTools::Initializers::CreateCommandBuffer(commandpool, core->GetViewDevice(), VK_COMMAND_BUFFER_LEVEL_PRIMARY, false);

	////////////////////////////////////////////////////////////////////////////////
	// Record command buffer
	////////////////////////////////////////////////////////////////////////////////
	VkCommandBufferBeginInfo cmdBufInfo = {};
	cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBufInfo.pNext = NULL;

	VkClearValue clearValues[6];
	clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
	clearValues[1].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
	clearValues[2].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
	clearValues[3].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
	clearValues[4].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
	clearValues[5].depthStencil = { 1.0f, 0 };

	VkRenderPassBeginInfo renderPassBeginInfo = {};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.pNext = NULL;
	renderPassBeginInfo.renderPass = renderstate->m_renderpass;
	renderPassBeginInfo.renderArea.offset.x = 0;
	renderPassBeginInfo.renderArea.offset.y = 0;
	renderPassBeginInfo.renderArea.extent.width = width;
	renderPassBeginInfo.renderArea.extent.height = height;
	renderPassBeginInfo.clearValueCount = 6;
	renderPassBeginInfo.pClearValues = clearValues;

	for (uint32_t i = 0; i < framebufferCount; i++)
	{
		// Set target frame buffer
		renderPassBeginInfo.framebuffer = framebuffers[i];

		//begin
		VK_CHECK_RESULT(vkBeginCommandBuffer(renderState->m_commandBuffers[i], &cmdBufInfo));

		vkCmdResetQueryPool(renderState->m_commandBuffers[i], renderState->m_queryPool, 0, 4);
		vkCmdWriteTimestamp(renderState->m_commandBuffers[i], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, renderState->m_queryPool, 0);

		// Start the first sub pass specified in our default render pass setup by the base class
		// This will clear the color and depth attachment
		vkCmdBeginRenderPass(renderState->m_commandBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		// Update dynamic viewport state
		VkViewport viewport = {};
		viewport.width = (float)widthScaled;
		viewport.height = (float)heightScaled;
		viewport.minDepth = (float) 0.0f;
		viewport.maxDepth = (float) 1.0f;
		vkCmdSetViewport(renderState->m_commandBuffers[i], 0, 1, &viewport);

		// Update dynamic scissor state
		VkRect2D scissor = {};
		scissor.extent.width = widthScaled;
		scissor.extent.height = heightScaled;
		scissor.offset.x = 0;
		scissor.offset.y = 0;
		vkCmdSetScissor(renderState->m_commandBuffers[i], 0, 1, &scissor);

		// Bind the rendering pipeline (including the shaders)
		vkCmdBindPipeline(renderState->m_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, renderState->m_pipelines[0]);

		// Bind descriptor sets describing shader binding points
		uint32_t doffset = 0;
		vkCmdBindDescriptorSets(renderState->m_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, renderState->m_pipelineLayout, 0, 1, &staticDescriptorSet, 1, &doffset);
		uint32_t d = 0;
		for (uint32_t m = 0; m < meshCount; m++)
		{
			//select the current mesh
			vk_mesh_s* mesh = &meshes[m];
			//bind vertexbuffer per mesh
			vkCmdBindVertexBuffers(renderState->m_commandBuffers[i], 0, mesh->vbvCount, mesh->vertexResources, mesh->vertexOffsets);
			for (uint32_t j = 0; j < mesh->submeshCount; j++)
			{
				//TODO: BIND new descriptorset
				VkDescriptorSet descriptorset = renderState->m_descriptorSets[(i*DYNAMIC_DESCRIPTOR_SET_COUNT) + d++ +2];

				//bind the textures to the correct format
				//format: stype,pnext,scSet,srcBinding,srcArrayelement,dstSet,dstbinding,dstarrayelement,descriptorcount
				VkCopyDescriptorSet textureDescriptorSets[TEXTURE_NUM];
				//diffuse texture
				VkCopyDescriptorSet diffuse;
				diffuse.sType = VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET;
				diffuse.pNext = NULL;
				diffuse.srcSet = staticDescriptorSet;
				diffuse.srcBinding = STATIC_DESCRIPTOR_IMAGE;
				diffuse.srcArrayElement = mesh->submeshes[j].textureIndex[DIFFUSE_TEXTURE];
				diffuse.dstSet = descriptorset;
				diffuse.dstBinding = FORWARDRENDER_DESCRIPTOR_IMAGE_DIFFUSE;
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
				normal.dstBinding = FORWARDRENDER_DESCRIPTOR_IMAGE_NORMAL;
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
				opacity.dstBinding = FORWARDRENDER_DESCRIPTOR_IMAGE_OPACITY;
				opacity.dstArrayElement = 0;
				opacity.descriptorCount = 1;
				textureDescriptorSets[OPACITY_TEXTURE] = opacity;
				// Update the descriptors
				vkUpdateDescriptorSets(core->GetViewDevice(), 0, NULL, (uint32_t)TEXTURE_NUM, textureDescriptorSets);
				// Bind descriptor sets describing shader binding points
				vkCmdBindDescriptorSets(renderState->m_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, renderState->m_pipelineLayout, 1, 1, &descriptorset, 0, NULL);
				// Bind triangle indices
				vkCmdBindIndexBuffer(renderState->m_commandBuffers[i], mesh->submeshes[j].ibv.buffer, mesh->submeshes[j].ibv.offset, mesh->submeshes[j].ibv.format);
				// Draw indexed triangle
				vkCmdDrawIndexed(renderState->m_commandBuffers[i], (uint32_t)mesh->submeshes[j].ibv.count, 1, 0, 0, 0);
			}
		}

		// renderpass two
		vkCmdNextSubpass(renderState->m_commandBuffers[i], VK_SUBPASS_CONTENTS_INLINE);

		// Update dynamic viewport state
		viewport = {};
		viewport.width = (float)widthScaled;
		viewport.height = (float)heightScaled;
		viewport.minDepth = (float) 0.0f;
		viewport.maxDepth = (float) 1.0f;
		vkCmdSetViewport(renderState->m_commandBuffers[i], 0, 1, &viewport);
		// Update dynamic scissor state
		scissor = {};
		scissor.extent.width = widthScaled;
		scissor.extent.height = heightScaled;
		scissor.offset.x = 0;
		scissor.offset.y = 0;
		vkCmdSetScissor(renderState->m_commandBuffers[i], 0, 1, &scissor);
		vkCmdBindPipeline(renderState->m_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, renderstate->m_pipelines[2]);
		vkCmdBindDescriptorSets(renderState->m_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, renderState->m_pipelineLayout, 2, 1, &renderState->m_descriptorSets[0], 0, NULL);
		vkCmdDraw(renderState->m_commandBuffers[i], 3, 1, 0, 0);

		// renderpass three
		vkCmdNextSubpass(renderState->m_commandBuffers[i], VK_SUBPASS_CONTENTS_INLINE);
		// Update dynamic viewport state
		viewport = {};
		viewport.width = (float)width;
		viewport.height = (float)height;
		viewport.minDepth = (float) 0.0f;
		viewport.maxDepth = (float) 1.0f;
		vkCmdSetViewport(renderState->m_commandBuffers[i], 0, 1, &viewport);
		// Update dynamic scissor state
		scissor = {};
		scissor.extent.width = width;
		scissor.extent.height = height;
		scissor.offset.x = 0;
		scissor.offset.y = 0;
		vkCmdSetScissor(renderState->m_commandBuffers[i], 0, 1, &scissor);
		vkCmdBindPipeline(renderState->m_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, renderstate->m_pipelines[1]);

		//clear attatchments
		VkClearAttachment at[6] = {};
		VkRect2D rect = {};
		rect.extent.width = widthScaled;
		rect.extent.height = heightScaled;
		VkClearRect clearrect = {};
		clearrect.baseArrayLayer = 0;
		clearrect.layerCount = 1;
		clearrect.rect = rect;
		at[0] = { VK_IMAGE_ASPECT_COLOR_BIT,0,clearValues[0] };
		at[1] = { VK_IMAGE_ASPECT_COLOR_BIT,1,clearValues[1] };
		at[2] = { VK_IMAGE_ASPECT_COLOR_BIT,2,clearValues[2] };
		at[3] = { VK_IMAGE_ASPECT_COLOR_BIT,3,clearValues[3] };
		at[4] = { VK_IMAGE_ASPECT_COLOR_BIT,4,clearValues[4] };
		at[5] = { VK_IMAGE_ASPECT_DEPTH_BIT,5,clearValues[5] };
		vkCmdClearAttachments(renderState->m_commandBuffers[i], 6, at, 1, &clearrect);

		vkCmdBindDescriptorSets(renderState->m_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, renderState->m_pipelineLayout, 0, 1, &staticDescriptorSet, 1, &doffset);
		d = 0;
		for (uint32_t m = 0; m < meshCount; m++)
		{
			//select the current mesh
			vk_mesh_s* mesh = &meshes[m];
			//bind vertexbuffer per mesh
			vkCmdBindVertexBuffers(renderState->m_commandBuffers[i], 0, mesh->vbvCount, mesh->vertexResources, mesh->vertexOffsets);
			for (uint32_t j = 0; j < mesh->submeshCount; j++)
			{
				// Bind descriptor sets describing shader binding points
				vkCmdBindDescriptorSets(renderState->m_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, renderState->m_pipelineLayout, 1, 1, &renderState->m_descriptorSets[(i*DYNAMIC_DESCRIPTOR_SET_COUNT) + d++ + 2], 0, NULL);
				// Bind triangle indices
				vkCmdBindIndexBuffer(renderState->m_commandBuffers[i], mesh->submeshes[j].ibv.buffer, mesh->submeshes[j].ibv.offset, mesh->submeshes[j].ibv.format);
				// Draw indexed triangle
				vkCmdDrawIndexed(renderState->m_commandBuffers[i], (uint32_t)mesh->submeshes[j].ibv.count, 1, 0, 0, 0);
			}
		}
		
		// renderpass four
		vkCmdNextSubpass(renderState->m_commandBuffers[i], VK_SUBPASS_CONTENTS_INLINE);

		// Update dynamic viewport state
		viewport = {};
		viewport.width = (float)width;
		viewport.height = (float)height;
		viewport.minDepth = (float) 0.0f;
		viewport.maxDepth = (float) 1.0f;
		vkCmdSetViewport(renderState->m_commandBuffers[i], 0, 1, &viewport);
		// Update dynamic scissor state
		scissor = {};
		scissor.extent.width = width;
		scissor.extent.height = height;
		scissor.offset.x = 0;
		scissor.offset.y = 0;
		vkCmdSetScissor(renderState->m_commandBuffers[i], 0, 1, &scissor);
		vkCmdBindPipeline(renderState->m_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, renderstate->m_pipelines[3]);
		vkCmdBindDescriptorSets(renderState->m_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, renderState->m_pipelineLayout, 2, 1, &renderState->m_descriptorSets[0], 0, NULL);
		vkCmdBindDescriptorSets(renderState->m_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, renderState->m_pipelineLayout, 3, 1, &renderState->m_descriptorSets[1], 0, NULL);
		vkCmdDraw(renderState->m_commandBuffers[i], 3, 1, 0, 0);
		
		// Setup query
		vkCmdWriteTimestamp(renderState->m_commandBuffers[i], VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, renderState->m_queryPool, 1);


		// End renderpass
		vkCmdEndRenderPass(renderState->m_commandBuffers[i]);
		// End commandbuffer
		VK_CHECK_RESULT(vkEndCommandBuffer(renderState->m_commandBuffers[i]));
	}
}

extern void CreateDeferredMainRenderState(
	RenderState& renderState,
	uint32_t framebufferCount,
	VulkanCore* core,
	VkCommandPool commandPool,
	SwapChain* swapchain,
	VkDescriptorSet staticDescriptorSet,
	Vertices* vertices,
	vk_mesh_s* meshes,
	uint32_t meshCount,
	AnisotropicVoxelTexture* avt,
	VkDescriptorSetLayout staticDescLayout,
	float scale)		// Forward renderer pipeline state
{
	uint32_t width = swapchain->m_width;
	uint32_t height = swapchain->m_height;
	uint32_t widthScaled = (uint32_t)(width*scale);
	uint32_t heightScaled = (uint32_t)(height*scale);
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
	// Create framebuffer attatchments
	////////////////////////////////////////////////////////////////////////////////
	if (!renderState.m_framebufferAttatchments)
	{
		renderState.m_framebufferAttatchmentCount = 4;
		renderState.m_framebufferAttatchments = (FrameBufferAttachment*)malloc(sizeof(FrameBufferAttachment)*renderState.m_framebufferAttatchmentCount);
		VKTools::CreateAttachment(core->GetViewDevice(), core, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &renderState.m_framebufferAttatchments[0], width, height);	// (World space) Positions		
		VKTools::CreateAttachment(core->GetViewDevice(), core, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &renderState.m_framebufferAttatchments[1], width, height);	// (World space) Normals		
		VKTools::CreateAttachment(core->GetViewDevice(), core, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &renderState.m_framebufferAttatchments[2], width, height);			// Albedo	
		VKTools::CreateAttachment(core->GetViewDevice(), core, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &renderState.m_framebufferAttatchments[3], width, height);	// (World space) Tangents

		VkAttachmentDescription attachments[6];
		// Scaled color attachment
		attachments[0] = {};
		attachments[0].format = core->m_colorFormat;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		// Scaled Position
		attachments[1] = {};
		attachments[1].format = renderState.m_framebufferAttatchments[0].format;
		attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[1].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		// Scaled Normals
		attachments[2] = {};
		attachments[2].format = renderState.m_framebufferAttatchments[1].format;
		attachments[2].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[2].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		// Scaled Albedo
		attachments[3] = {};
		attachments[3].format = renderState.m_framebufferAttatchments[2].format;
		attachments[3].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[3].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[3].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[3].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[3].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[3].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[3].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		// Scaled Tangent
		attachments[4] = {};
		attachments[4].format = renderState.m_framebufferAttatchments[3].format;
		attachments[4].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[4].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[4].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[4].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[4].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[4].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[4].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		// Depth attachment
		attachments[5] = {};
		attachments[5].format = core->m_depthFormat;
		attachments[5].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[5].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[5].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[5].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[5].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[5].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[5].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpassDescriptions[4] = {};
		// First subpass: Fill G-Buffer components
		VkAttachmentReference colorReferences[5] = {};
		VkAttachmentReference depthReference = { 5, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
		VkAttachmentReference inputReferences[4] = {};
		colorReferences[0] = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };	// Output
		colorReferences[1] = { 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };	// Position
		colorReferences[2] = { 2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };	// Normal
		colorReferences[3] = { 3, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };	// Albedo
		colorReferences[4] = { 4, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };	// Tangent
		subpassDescriptions[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpassDescriptions[0].colorAttachmentCount = 5;
		subpassDescriptions[0].pColorAttachments = colorReferences;
		subpassDescriptions[0].pDepthStencilAttachment = &depthReference;
		// Second subpass: Scaled composition
		inputReferences[0] = { 1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };	// input Position
		inputReferences[1] = { 2, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };	// input Normal
		inputReferences[2] = { 3, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };	// input Albedo
		inputReferences[3] = { 4, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };	// input Tangent
		subpassDescriptions[1].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpassDescriptions[1].colorAttachmentCount = 1;
		subpassDescriptions[1].pColorAttachments = colorReferences;
		subpassDescriptions[1].pDepthStencilAttachment = &depthReference;
		subpassDescriptions[1].inputAttachmentCount = 4;
		subpassDescriptions[1].pInputAttachments = inputReferences;
		// Third subpass: non-scaled G-buffer render
		colorReferences[0] = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };	// Output
		colorReferences[1] = { 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };	// Position
		colorReferences[2] = { 2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };	// Normal
		colorReferences[3] = { 3, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };	// Albedo
		colorReferences[4] = { 4, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };	// Tangent
		subpassDescriptions[2].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpassDescriptions[2].colorAttachmentCount = 5;
		subpassDescriptions[2].pColorAttachments = colorReferences;
		subpassDescriptions[2].pDepthStencilAttachment = &depthReference;
		subpassDescriptions[2].inputAttachmentCount = 0;
		subpassDescriptions[2].pInputAttachments = NULL;
		// Fourth subpass: non-scaled scaled composition
		inputReferences[0] = { 1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };	// input Position
		inputReferences[1] = { 2, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };	// input Normal
		inputReferences[2] = { 3, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };	// input Albedo
		inputReferences[3] = { 4, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };	// input Tangent
		subpassDescriptions[3].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpassDescriptions[3].colorAttachmentCount = 1;
		subpassDescriptions[3].pColorAttachments = colorReferences;
		subpassDescriptions[3].pDepthStencilAttachment = &depthReference;
		subpassDescriptions[3].inputAttachmentCount = 4;
		subpassDescriptions[3].pInputAttachments = inputReferences;

		// Subpass dependencies for layout transitions
		VkSubpassDependency dependencies[5];
		dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[0].dstSubpass = 0;
		dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		// This dependency transitions the input attachment from color attachment to shader read
		dependencies[1].srcSubpass = 0;
		dependencies[1].dstSubpass = 1;
		dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		dependencies[2].srcSubpass = 1;
		dependencies[2].dstSubpass = 2;
		dependencies[2].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[2].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		dependencies[2].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		dependencies[2].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[2].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		dependencies[3].srcSubpass = 2;
		dependencies[3].dstSubpass = 3;
		dependencies[3].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[3].dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		dependencies[3].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[3].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		dependencies[3].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		dependencies[4].srcSubpass = 0;
		dependencies[4].dstSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[4].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[4].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[4].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[4].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[4].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		VkRenderPassCreateInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = 6;
		renderPassInfo.pAttachments = attachments;
		renderPassInfo.subpassCount = 4;
		renderPassInfo.pSubpasses = subpassDescriptions;
		renderPassInfo.dependencyCount = 5;
		renderPassInfo.pDependencies = dependencies;
		VK_CHECK_RESULT(vkCreateRenderPass(core->GetViewDevice(), &renderPassInfo, NULL, &renderState.m_renderpass));
	}

	////////////////////////////////////////////////////////////////////////////////
	// Set framebuffers
	////////////////////////////////////////////////////////////////////////////////
	renderState.m_framebufferCount = framebufferCount;
	renderState.m_framebuffers = (VkFramebuffer*)malloc(sizeof(VkFramebuffer)*renderState.m_framebufferCount);

	VkImageView attachments[6];
	VkFramebufferCreateInfo frameBufferCreateInfo = {};
	frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	frameBufferCreateInfo.pNext = NULL;
	frameBufferCreateInfo.renderPass = renderState.m_renderpass;
	frameBufferCreateInfo.attachmentCount = 6;
	frameBufferCreateInfo.pAttachments = attachments;
	frameBufferCreateInfo.layers = 1;
	frameBufferCreateInfo.width = width;
	frameBufferCreateInfo.height = height;
	for (uint32_t i = 0; i < renderState.m_framebufferCount; i++)
	{
		attachments[0] = swapchain->m_buffers[i].view;						// output
		attachments[1] = renderState.m_framebufferAttatchments[0].view;		// position
		attachments[2] = renderState.m_framebufferAttatchments[1].view;		// normal
		attachments[3] = renderState.m_framebufferAttatchments[2].view;		// albedo
		attachments[4] = renderState.m_framebufferAttatchments[3].view;		// tangent
		attachments[5] = core->m_depthStencil.view;							// depth
		VK_CHECK_RESULT(vkCreateFramebuffer(core->GetViewDevice(), &frameBufferCreateInfo, NULL, &renderState.m_framebuffers[i]));
	}

	//make output image

	////////////////////////////////////////////////////////////////////////////////
	// Create the pipelineCache
	////////////////////////////////////////////////////////////////////////////////
	// create a default pipelinecache
	if (!renderState.m_pipelineCache)
	{
		VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
		pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
		VK_CHECK_RESULT(vkCreatePipelineCache(core->GetViewDevice(), &pipelineCacheCreateInfo, NULL, &renderState.m_pipelineCache));
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
		renderState.m_uniformData = (UniformData*)malloc(sizeof(UniformData)*renderState.m_uniformDataCount);
		for (uint32_t i = 0; i < renderState.m_uniformDataCount; i++)
		{
			VKTools::CreateBuffer(core, core->GetViewDevice(),
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				sizeof(DeferredMainRendererUBOFrag),
				NULL,
				&renderState.m_uniformData[i].m_buffer,
				&renderState.m_uniformData[i].m_memory,
				&renderState.m_uniformData[i].m_descriptor);
		}	
	}

	////////////////////////////////////////////////////////////////////////////////
	// Create the descriptorlayout
	////////////////////////////////////////////////////////////////////////////////
	if (!renderState.m_descriptorLayouts)
	{
		renderState.m_descriptorLayoutCount = 3;
		renderState.m_descriptorLayouts = (VkDescriptorSetLayout*)malloc(sizeof(VkDescriptorSetLayout)*renderState.m_descriptorLayoutCount);
		// Dynamic descriptorset
		VkDescriptorSetLayoutBinding layoutbinding0[DEFERRED_MAIN_DESCRIPTOR_MULTIPLE_COUNT];
		VkDescriptorSetLayoutBinding layoutbinding1[DEFERRED_MAIN_DESCRIPTOR_PASS_COUNT];
		VkDescriptorSetLayoutBinding layoutbinding2[1];
		// Binding 0 : Diffuse texture sampled image
		layoutbinding0[DEFERRED_MAIN_DESCRIPTOR_IMAGE_DIFFUSE] =
		{ DEFERRED_MAIN_DESCRIPTOR_IMAGE_DIFFUSE, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL };
		// Binding 1 : Normal texture sampled image
		layoutbinding0[DEFERRED_MAIN_DESCRIPTOR_IMAGE_NORMAL] =
		{ DEFERRED_MAIN_DESCRIPTOR_IMAGE_NORMAL, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL };
		// Binding 2: Opacity texture sampled image
		layoutbinding0[DEFERRED_MAIN_DESCRIPTOR_IMAGE_OPACITY] =
		{ DEFERRED_MAIN_DESCRIPTOR_IMAGE_OPACITY, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL };
		// Binding 0: Fragment uniform buffer
		layoutbinding1[DEFERRED_MAIN_DESCRIPTOR_BUFFER_FRAG] =
		{ DEFERRED_MAIN_DESCRIPTOR_BUFFER_FRAG, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL };
		// Binding 1: Fragment Voxelgrid
		layoutbinding1[DEFERRED_MAIN_DESCRIPTOR_VOXELGRID] =
		{ DEFERRED_MAIN_DESCRIPTOR_VOXELGRID, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER , 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL };
		// Binding 2: output
		layoutbinding1[DEFERRED_MAIN_DESCRIPTOR_OUTPUT] =
		{ DEFERRED_MAIN_DESCRIPTOR_OUTPUT, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE , 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL };
		// Binding 3: Input Position texture
		layoutbinding1[DEFERRED_MAIN_DESCRIPTOR_INPUT_POSITION] =
		{ DEFERRED_MAIN_DESCRIPTOR_INPUT_POSITION, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL };
		// Binding 4: Input Normal texture
		layoutbinding1[DEFERRED_MAIN_DESCRIPTOR_INPUT_NORMAL] =
		{ DEFERRED_MAIN_DESCRIPTOR_INPUT_NORMAL, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL };
		// Binding 5: Input Albedo texture
		layoutbinding1[DEFERRED_MAIN_DESCRIPTOR_INPUT_ALBEDO] =
		{ DEFERRED_MAIN_DESCRIPTOR_INPUT_ALBEDO, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL };
		// Binding 6: Input Tangent texture
		layoutbinding1[DEFERRED_MAIN_DESCRIPTOR_INPUT_TANGENT] =
		{ DEFERRED_MAIN_DESCRIPTOR_INPUT_TANGENT, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL };
		// Binding 1: Fragment Voxelgrid
		layoutbinding2[DEFERRED_MAIN_DESCRIPTOR_INPUT] =
		{ DEFERRED_MAIN_DESCRIPTOR_INPUT, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER , 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL };
		// Create the descriptorlayout0
		VkDescriptorSetLayoutCreateInfo descriptorLayout0 = VKTools::Initializers::DescriptorSetLayoutCreateInfo(0, DEFERRED_MAIN_DESCRIPTOR_MULTIPLE_COUNT, layoutbinding0);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(core->GetViewDevice(), &descriptorLayout0, NULL, &renderState.m_descriptorLayouts[0]));
		// Create the descriptorlayout1
		VkDescriptorSetLayoutCreateInfo descriptorLayout1 = VKTools::Initializers::DescriptorSetLayoutCreateInfo(0, DEFERRED_MAIN_DESCRIPTOR_PASS_COUNT, layoutbinding1);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(core->GetViewDevice(), &descriptorLayout1, NULL, &renderState.m_descriptorLayouts[1]));
		// Create the descriptorlayout2
		VkDescriptorSetLayoutCreateInfo descriptorLayout2 = VKTools::Initializers::DescriptorSetLayoutCreateInfo(0, 1, layoutbinding2);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(core->GetViewDevice(), &descriptorLayout2, NULL, &renderState.m_descriptorLayouts[2]));
	}

	////////////////////////////////////////////////////////////////////////////////
	// Create layout
	////////////////////////////////////////////////////////////////////////////////
	if (!renderState.m_pipelineLayout)
	{
		VkDescriptorSetLayout dLayouts[] = { staticDescLayout, renderState.m_descriptorLayouts[0], renderState.m_descriptorLayouts[1], renderState.m_descriptorLayouts[2]};
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = VKTools::Initializers::PipelineLayoutCreateInfo(0, 4, dLayouts);
		VK_CHECK_RESULT(vkCreatePipelineLayout(core->GetViewDevice(), &pipelineLayoutCreateInfo, NULL, &renderState.m_pipelineLayout));
	}

	////////////////////////////////////////////////////////////////////////////////
	// Create descriptor pool
	////////////////////////////////////////////////////////////////////////////////
	if (!renderState.m_descriptorPool)
	{
		VkDescriptorPoolSize poolSize[DEFERRED_MAIN_DESCRIPTOR_COUNT];
		poolSize[DEFERRED_MAIN_DESCRIPTOR_IMAGE_DIFFUSE] = { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, framebufferCount * DYNAMIC_DESCRIPTOR_SET_COUNT };
		poolSize[DEFERRED_MAIN_DESCRIPTOR_IMAGE_NORMAL] =  { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, framebufferCount * DYNAMIC_DESCRIPTOR_SET_COUNT };
		poolSize[DEFERRED_MAIN_DESCRIPTOR_IMAGE_OPACITY] = { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, framebufferCount * DYNAMIC_DESCRIPTOR_SET_COUNT };
		poolSize[DEFERRED_MAIN_DESCRIPTOR_BUFFER_FRAG + DEFERRED_MAIN_DESCRIPTOR_MULTIPLE_COUNT] = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1};
		poolSize[DEFERRED_MAIN_DESCRIPTOR_VOXELGRID + DEFERRED_MAIN_DESCRIPTOR_MULTIPLE_COUNT] = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1};
		poolSize[DEFERRED_MAIN_DESCRIPTOR_OUTPUT + DEFERRED_MAIN_DESCRIPTOR_MULTIPLE_COUNT] = { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1};
		poolSize[DEFERRED_MAIN_DESCRIPTOR_INPUT_POSITION +DEFERRED_MAIN_DESCRIPTOR_MULTIPLE_COUNT] = { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1};
		poolSize[DEFERRED_MAIN_DESCRIPTOR_INPUT_NORMAL +DEFERRED_MAIN_DESCRIPTOR_MULTIPLE_COUNT] = { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1};
		poolSize[DEFERRED_MAIN_DESCRIPTOR_INPUT_ALBEDO +DEFERRED_MAIN_DESCRIPTOR_MULTIPLE_COUNT] = { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1 };
		poolSize[DEFERRED_MAIN_DESCRIPTOR_INPUT_TANGENT + DEFERRED_MAIN_DESCRIPTOR_MULTIPLE_COUNT] = { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1 };
		poolSize[DEFERRED_MAIN_DESCRIPTOR_PASS_COUNT + DEFERRED_MAIN_DESCRIPTOR_MULTIPLE_COUNT] = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 };
		VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = VKTools::Initializers::DescriptorPoolCreateInfo(0, framebufferCount * DYNAMIC_DESCRIPTOR_SET_COUNT+3, DEFERRED_MAIN_DESCRIPTOR_COUNT, poolSize);
		//create the descriptorPool
		VK_CHECK_RESULT(vkCreateDescriptorPool(core->GetViewDevice(), &descriptorPoolCreateInfo, NULL, &renderState.m_descriptorPool));
	}

	////////////////////////////////////////////////////////////////////////////////
	// Create the descriptor set
	////////////////////////////////////////////////////////////////////////////////
	//allocate the requirered descriptorsets
	if (!renderState.m_descriptorSets)
	{
		renderState.m_descriptorSetCount = DYNAMIC_DESCRIPTOR_SET_COUNT * framebufferCount + 2;
		renderState.m_descriptorSets = (VkDescriptorSet*)malloc(renderState.m_descriptorSetCount * sizeof(VkDescriptorSet));
		for (uint32_t i = 0; i < framebufferCount; i++)
		{
			for (uint32_t j = 0; j < DYNAMIC_DESCRIPTOR_SET_COUNT; j++)
			{
				VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = VKTools::Initializers::DescriptorSetAllocateInfo(renderState.m_descriptorPool, 1, &renderState.m_descriptorLayouts[0]);
				VK_CHECK_RESULT(vkAllocateDescriptorSets(core->GetViewDevice(), &descriptorSetAllocateInfo, &renderState.m_descriptorSets[(i*DYNAMIC_DESCRIPTOR_SET_COUNT) + j + 2]));
			}
		}
		// scaled renderer
		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = VKTools::Initializers::DescriptorSetAllocateInfo(renderState.m_descriptorPool, 1, &renderState.m_descriptorLayouts[1]);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(core->GetViewDevice(), &descriptorSetAllocateInfo, &renderState.m_descriptorSets[0]));
		descriptorSetAllocateInfo = VKTools::Initializers::DescriptorSetAllocateInfo(renderState.m_descriptorPool, 1, &renderState.m_descriptorLayouts[2]);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(core->GetViewDevice(), &descriptorSetAllocateInfo, &renderState.m_descriptorSets[1]));

		///////////////////////////////////////////////////////
		///// Set/Update the image and uniform buffer descriptorsets
		/////////////////////////////////////////////////////// 
		// todo: move this
		// here for now
		FrameBufferAttachment output;
		VKTools::CreateImage(core, VK_FORMAT_R16G16B16A16_SFLOAT, &output, widthScaled, heightScaled);
		//create the texture sampler
		VkSampler sampler;
		VkResult result;
		VkSamplerCreateInfo sci = {};
		sci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		sci.pNext = NULL;
		sci.flags = 0;
		sci.magFilter = VK_FILTER_LINEAR;
		sci.minFilter = VK_FILTER_LINEAR;
		sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		sci.mipLodBias = 0.0f;
		sci.anisotropyEnable = VK_FALSE;
		sci.maxAnisotropy = 16.0f;
		sci.compareEnable = VK_FALSE;
		sci.minLod = 0.0f;
		sci.maxLod = VK_LOD_CLAMP_NONE;
		sci.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		sci.unnormalizedCoordinates = VK_FALSE;
		//create the sampler
		result = vkCreateSampler(core->GetViewDevice(), &sci, NULL, &sampler);

		VkImageSubresourceRange srRange = {};
		srRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		srRange.baseMipLevel = 0;
		srRange.levelCount = 1;
		srRange.baseArrayLayer = 0;
		srRange.layerCount = 1;
		VkCommandBuffer cmd = VKTools::Initializers::CreateCommandBuffer(core->GetGraphicsCommandPool(), core->GetViewDevice(), VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		VKTools::SetImageLayout(cmd, output.image, VK_IMAGE_LAYOUT_PREINITIALIZED, VK_IMAGE_LAYOUT_GENERAL, srRange);
		VKTools::FlushCommandBuffer(cmd, core->GetGraphicsQueue(), core->GetViewDevice(), core->GetGraphicsCommandPool(), true);

		// scaled output
		VkDescriptorImageInfo descriptorOutput =
			VKTools::Initializers::DescriptorImageInfo(
				sampler,
				output.view,
				VK_IMAGE_LAYOUT_GENERAL);
		// position
		VkDescriptorImageInfo descriptorPosition =
			VKTools::Initializers::DescriptorImageInfo(
				NULL,
				renderState.m_framebufferAttatchments[0].view,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		// normals
		VkDescriptorImageInfo descriptorNormal =
			VKTools::Initializers::DescriptorImageInfo(
				NULL,
				renderState.m_framebufferAttatchments[1].view,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		// albedo
		VkDescriptorImageInfo descriptorAlbedo =
			VKTools::Initializers::DescriptorImageInfo(
				NULL,
				renderState.m_framebufferAttatchments[2].view,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		// tangent
		VkDescriptorImageInfo descriptorTangent =
			VKTools::Initializers::DescriptorImageInfo(
				NULL,
				renderState.m_framebufferAttatchments[3].view,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

		VkDescriptorImageInfo voxeldescriptor[1];
		voxeldescriptor[0].imageLayout = avt->m_imageLayout;
		voxeldescriptor[0].imageView = avt->m_descriptor[0].imageView;
		voxeldescriptor[0].sampler = avt->m_conetraceSampler;

		VkWriteDescriptorSet writeDescriptorSets[8] = {};
		// ubo
		writeDescriptorSets[0] =
			VKTools::Initializers::WriteDescriptorSet(
				renderState.m_descriptorSets[0],
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				DEFERRED_MAIN_DESCRIPTOR_BUFFER_FRAG,
				&renderState.m_uniformData[0].m_descriptor),
		// voxel grid
		writeDescriptorSets[1] =
			VKTools::Initializers::WriteDescriptorSet(
				renderState.m_descriptorSets[0],
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				DEFERRED_MAIN_DESCRIPTOR_VOXELGRID,
				voxeldescriptor);
		// Output
		writeDescriptorSets[2] =
			VKTools::Initializers::WriteDescriptorSet(
				renderState.m_descriptorSets[0],
				VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
				DEFERRED_MAIN_DESCRIPTOR_OUTPUT,
				&descriptorOutput);
			// position
		writeDescriptorSets[3] =
			VKTools::Initializers::WriteDescriptorSet(
				renderState.m_descriptorSets[0],
				VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
				DEFERRED_MAIN_DESCRIPTOR_INPUT_POSITION,
				&descriptorPosition);
			// normal
		writeDescriptorSets[4] =
			VKTools::Initializers::WriteDescriptorSet(
				renderState.m_descriptorSets[0],
				VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
				DEFERRED_MAIN_DESCRIPTOR_INPUT_NORMAL,
				&descriptorNormal);
			// albedo
		writeDescriptorSets[5] =
			VKTools::Initializers::WriteDescriptorSet(
				renderState.m_descriptorSets[0],
				VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
				DEFERRED_MAIN_DESCRIPTOR_INPUT_ALBEDO,
				&descriptorAlbedo);
			// tangent
		writeDescriptorSets[6] =
			VKTools::Initializers::WriteDescriptorSet(
				renderState.m_descriptorSets[0],
				VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
				DEFERRED_MAIN_DESCRIPTOR_INPUT_TANGENT,
				&descriptorTangent);

		// input
		writeDescriptorSets[7] =
			VKTools::Initializers::WriteDescriptorSet(
				renderState.m_descriptorSets[1],
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				DEFERRED_MAIN_DESCRIPTOR_INPUT,
				&descriptorOutput);

			vkUpdateDescriptorSets(core->GetViewDevice(), 8, writeDescriptorSets, 0, NULL);
	}

	////////////////////////////////////////////////////////////////////////////////
	// Create Pipeline
	////////////////////////////////////////////////////////////////////////////////
	if (!renderState.m_pipelines)
	{
		renderState.m_pipelineCount = 4;
		renderState.m_pipelines = (VkPipeline*)malloc(renderState.m_pipelineCount * sizeof(VkPipeline));

		// The dynamic state properties themselves are stored in the command buffer
		VkDynamicState dynamicStateEnables[2];
		dynamicStateEnables[0] = VK_DYNAMIC_STATE_VIEWPORT;
		dynamicStateEnables[1] = VK_DYNAMIC_STATE_SCISSOR;
		// 
		{
			// Inputassembly state
			VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = 
				VKTools::Initializers::PipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
			// Rasterization state
			VkPipelineRasterizationStateCreateInfo rasterizationState = 
				VKTools::Initializers::PipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
			rasterizationState.rasterizerDiscardEnable = VK_FALSE;
			rasterizationState.depthBiasEnable = VK_FALSE;
			// Color blend state
			VkPipelineColorBlendAttachmentState blendAttachmentState[5];
			for(uint32_t i = 0; i < 5;i++)
				blendAttachmentState[i] = VKTools::Initializers::PipelineColorBlendAttachmentState(0xf, VK_FALSE);
			VkPipelineColorBlendStateCreateInfo colorBlendState = 
				VKTools::Initializers::PipelineColorBlendStateCreateInfo(5, blendAttachmentState);
			// Viewport state
			VkPipelineViewportStateCreateInfo viewportState = 
				VKTools::Initializers::PipelineViewportStateCreateInfo(1, 1, 0);
			// Dynamic state
			VkPipelineDynamicStateCreateInfo dynamicState = 
				VKTools::Initializers::PipelineDynamicStateCreateInfo(dynamicStateEnables, 2, 0);
			// Depth and stencil state
			VkPipelineDepthStencilStateCreateInfo depthStencilState = 
				VKTools::Initializers::PipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
			depthStencilState.depthBoundsTestEnable = VK_FALSE;
			depthStencilState.back.failOp = VK_STENCIL_OP_KEEP;
			depthStencilState.back.passOp = VK_STENCIL_OP_KEEP;
			depthStencilState.stencilTestEnable = VK_FALSE;
			depthStencilState.front = depthStencilState.back;
			// Multi sampling state
			VkPipelineMultisampleStateCreateInfo multisampleState = 
				VKTools::Initializers::PipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
			// Load shaders
			Shader shaderStages[2];
			shaderStages[0] = VKTools::LoadShader("shaders/diffuse.vert.spv", "main", core->GetViewDevice(), VK_SHADER_STAGE_VERTEX_BIT);
			shaderStages[1] = VKTools::LoadShader("shaders/deferredmainscaledgbuffer.frag.spv", "main", core->GetViewDevice(), VK_SHADER_STAGE_FRAGMENT_BIT);
			// Assign states
			// Assign pipeline state create information
			VkPipelineShaderStageCreateInfo shaderStagesData[2];
			for (uint32_t i = 0; i < 2; i++)
				shaderStagesData[i] = shaderStages[i].m_shaderStage;
			// pipelineinfo for creating the pipeline
			VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
			pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
			pipelineCreateInfo.layout = renderState.m_pipelineLayout;
			pipelineCreateInfo.stageCount = 2;
			pipelineCreateInfo.pStages = shaderStagesData;
			pipelineCreateInfo.pVertexInputState = &vertices->inputState;
			pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
			pipelineCreateInfo.pRasterizationState = &rasterizationState;
			pipelineCreateInfo.pColorBlendState = &colorBlendState;
			pipelineCreateInfo.pMultisampleState = &multisampleState;
			pipelineCreateInfo.pViewportState = &viewportState;
			pipelineCreateInfo.pDepthStencilState = &depthStencilState;
			pipelineCreateInfo.renderPass = renderState.m_renderpass;
			pipelineCreateInfo.pDynamicState = &dynamicState;
			pipelineCreateInfo.subpass = 0;
			// Create rendering pipeline
			VK_CHECK_RESULT(vkCreateGraphicsPipelines(core->GetViewDevice(), renderState.m_pipelineCache, 1, &pipelineCreateInfo, NULL, &renderState.m_pipelines[0]));
			pipelineCreateInfo.subpass = 2;
			VK_CHECK_RESULT(vkCreateGraphicsPipelines(core->GetViewDevice(), renderState.m_pipelineCache, 1, &pipelineCreateInfo, NULL, &renderState.m_pipelines[1]));
		}
		{
			// Pipeline
			VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = 
				VKTools::Initializers::PipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
			VkPipelineRasterizationStateCreateInfo rasterizationState = 
				VKTools::Initializers::PipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE, 0);
			VkPipelineColorBlendAttachmentState blendAttachmentState = 
				VKTools::Initializers::PipelineColorBlendAttachmentState(0xf, VK_FALSE);
			VkPipelineColorBlendStateCreateInfo colorBlendState =
				VKTools::Initializers::PipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
			VkPipelineDepthStencilStateCreateInfo depthStencilState =
				VKTools::Initializers::PipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
			VkPipelineViewportStateCreateInfo viewportState =
				VKTools::Initializers::PipelineViewportStateCreateInfo(1, 1, 0);
			VkPipelineMultisampleStateCreateInfo multisampleState =
				VKTools::Initializers::PipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
			VkPipelineDynamicStateCreateInfo dynamicState =
				VKTools::Initializers::PipelineDynamicStateCreateInfo(dynamicStateEnables, 2, 0);
			// Load shaders
			Shader shaderStages[2];
			shaderStages[0] = VKTools::LoadShader("shaders/deferredmaincomposition.vert.spv", "main", core->GetViewDevice(), VK_SHADER_STAGE_VERTEX_BIT);
			shaderStages[1] = VKTools::LoadShader("shaders/deferredmainscaledcomposition.frag.spv", "main", core->GetViewDevice(), VK_SHADER_STAGE_FRAGMENT_BIT);
			VkPipelineShaderStageCreateInfo shaderStagesData[2];
			for (uint32_t i = 0; i < 2; i++)
				shaderStagesData[i] = shaderStages[i].m_shaderStage;

			VkPipelineVertexInputStateCreateInfo emptyInputState{};
			emptyInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
			// pipelineinfo for creating the pipeline
			VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
			pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
			pipelineCreateInfo.layout = renderState.m_pipelineLayout;
			pipelineCreateInfo.pVertexInputState = &emptyInputState;
			pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
			pipelineCreateInfo.pRasterizationState = &rasterizationState;
			pipelineCreateInfo.pColorBlendState = &colorBlendState;
			pipelineCreateInfo.pMultisampleState = &multisampleState;
			pipelineCreateInfo.pViewportState = &viewportState;
			pipelineCreateInfo.pDepthStencilState = &depthStencilState;
			pipelineCreateInfo.pDynamicState = &dynamicState;
			pipelineCreateInfo.renderPass = renderState.m_renderpass;
			pipelineCreateInfo.stageCount = 2;
			pipelineCreateInfo.pStages = shaderStagesData;
			// Index of the subpass that this pipeline will be used in
			pipelineCreateInfo.subpass = 1;
			VK_CHECK_RESULT(vkCreateGraphicsPipelines(core->GetViewDevice(), renderState.m_pipelineCache, 1, &pipelineCreateInfo, nullptr, &renderState.m_pipelines[2]));

			shaderStages[0] = VKTools::LoadShader("shaders/deferredmaincomposition.vert.spv", "main", core->GetViewDevice(), VK_SHADER_STAGE_VERTEX_BIT);
			shaderStages[1] = VKTools::LoadShader("shaders/deferredmainnonscaledcomposition.frag.spv", "main", core->GetViewDevice(), VK_SHADER_STAGE_FRAGMENT_BIT);

			for (uint32_t i = 0; i < 2; i++)
				shaderStagesData[i] = shaderStages[i].m_shaderStage;
			pipelineCreateInfo.subpass = 3;
			VK_CHECK_RESULT(vkCreateGraphicsPipelines(core->GetViewDevice(), renderState.m_pipelineCache, 1, &pipelineCreateInfo, nullptr, &renderState.m_pipelines[3]));
		}
	}

	////////////////////////////////////////////////////////////////////////////////
	// Build command buffers
	////////////////////////////////////////////////////////////////////////////////
	Parameter* parameter;
	parameter = (Parameter*)malloc(sizeof(Parameter));
	parameter->meshes = meshes;
	parameter->meshCount = meshCount;
	parameter->staticDescriptorSet = staticDescriptorSet;
	parameter->scale = scale;
	renderState.m_cmdBufferParameters = (BYTE*)parameter;

	renderState.m_CreateCommandBufferFunc = &BuildCommandBufferDeferredMainRenderState;
	renderState.m_CreateCommandBufferFunc(&renderState, commandPool, core, framebufferCount, renderState.m_framebuffers, renderState.m_cmdBufferParameters);
}
