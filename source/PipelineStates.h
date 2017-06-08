#ifndef PIPELINESTATES_H
#define PIPELINESTATES_H

#include "VCTPipelineDefines.h"

struct AnisotropicVoxelTexture;
class Camera;

// ImGUI rendrer pipeline state
extern void CreateImgGUIPipelineState(
	RenderState& renderState,
	uint32_t framebufferCount,
	VulkanCore* core,
	VkCommandPool commandPool,
	SwapChain* swapchain);
// Forward renderer pipeline state
extern void CreateForwardRenderState(
	RenderState& renderState,
	VkFramebuffer* framebuffers,
	uint32_t framebufferCount,
	VulkanCore* core,
	VkCommandPool commandPool,
	SwapChain* swapchain,
	VkDescriptorSet staticDescriptorSet,
	Vertices* vertices,
	vk_mesh_s* meshes,
	uint32_t meshCount,
	VkRenderPass renderpass,
	VkDescriptorSetLayout staticDescLayout);
// Voxelizer renderer pipeline state
extern void CreateVoxelizerState(
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
	AnisotropicVoxelTexture* avt);
// Voxel debug renderer pipeline state
extern void CreateVoxelRenderDebugState(	// Voxel renderer (debugging purposes) pipeline state
	RenderState& renderState,
	VkFramebuffer* framebuffers,
	uint32_t framebufferCount,
	VulkanCore* core,
	VkCommandPool commandPool,
	SwapChain* swapchain,
	VkDescriptorSet staticDescriptorSet,
	VkDescriptorSetLayout staticDescLayout,
	Camera* camera,
	AnisotropicVoxelTexture* avt);
// voxel mipmapper pipeline state
extern void CreateMipMapperState(
	RenderState& renderState,
	VulkanCore* core,
	VkCommandPool commandPool,
	VkDevice device,
	SwapChain* swapchain,
	AnisotropicVoxelTexture* avt
);
// Cone trace pipeline state ( screenspace
extern void CreateConeTraceState(
	RenderState& renderState,
	uint32_t framebufferCount,
	VulkanCore* core,
	VkCommandPool commandPool,
	SwapChain* swapchain,
	AnisotropicVoxelTexture* avts
);
// Post voxelizer pipeline state
extern void CreatePostVoxelizerState(
	RenderState& renderState,
	VulkanCore* core,
	VkCommandPool commandPool,
	VkDevice device,
	SwapChain* swapchain,
	AnisotropicVoxelTexture* avt
);
// Main Renderer pipeline state (Cascaded Voxel Cone Tracing
extern void CreateForwardMainRendererState(
	RenderState& renderState,
	VkFramebuffer* framebuffers,
	uint32_t framebufferCount,
	VulkanCore* core,
	VkCommandPool commandPool,
	VkDevice device,
	SwapChain* swapchain,
	VkDescriptorSet staticDescriptorSet,
	Vertices* vertices,
	vk_mesh_s* meshes,
	uint32_t meshCount,
	VkRenderPass renderpass,
	VkDescriptorSetLayout staticDescLayout,
	AnisotropicVoxelTexture* avts
);
// DeferredMainRendererState
extern void CreateDeferredMainRenderState
(
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
	float scale
);
//Shadow map pipeline state
/*
extern void CreateShadowMapState(
	RenderState& renderState,
	VkCommandBuffer setupCommandBuffer,
	VulkanCore* core,
	VkCommandPool commandPool,
	VkDevice device,
	VkFormat depthFormat,
	VkFormat colorFormat,
	SwapChain* swapchain,
	VkDescriptorSet staticDescriptorSet,
	Vertices* vertices,
	Indices* indices,
	vk_mesh_s* meshes,
	uint32_t meshCount,
	VkDescriptorSetLayout staticDescLayout,
	VkCommandBuffer* drawcommandBuffer);			// shadowmap pipeline state
*/

extern void DestroyRenderStates(RenderState& rs, VulkanCore* core, VkCommandPool commandpool);
extern void DestroyCommandBuffer(RenderState& rs, VulkanCore* core, VkCommandPool commandpool);
extern void BuildCommandBuffer(RenderState& rs, VkCommandPool commandpool, VulkanCore* core, uint32_t framebufferCount, VkFramebuffer* framebuffers);

#endif //PIPELINESTATES_H