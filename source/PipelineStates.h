#ifndef PIPELINESTATES_H
#define PIPELINESTATES_H

#include "VCTPipelineDefines.h"

struct AnisotropicVoxelTexture;
class Camera;

//// ImGUI rendrer pipeline state
//extern void CreateImgGUIPipelineState(
//	RenderState& renderState,
//	uint32_t framebufferCount,
//	VulkanCore* core,
//	VkCommandPool commandPool,
//	SwapChain* swapchain);
// Create Grid culling state
extern void StateGridCulling(
	RenderState& renderState,
	VulkanCore* core,
	InstanceDataAABB* aabb,
	uint32_t submeshcount,
	InstanceStatistic& statistic,
	VkDrawIndexedIndirectCommand* indirectbuffer,
	uint32_t debugboxcount);
// Create Forward Renderer state
extern void StateForwardRenderer(
	RenderState& renderState,
	VulkanCore* core,
	VkDescriptorSet staticDescriptorSet,
	VkDescriptorSetLayout staticDescLayout,
	VkDescriptorSetLayout textureDescLayout,
	VKScene* scenes,
	uint32_t scenecount);
// Create debugging renderer state
extern void StateDebugRenderer(
	RenderState& renderstate,
	VulkanCore* core,
	VkDescriptorSetLayout staticDescLayout,
	BufferObject& debugbufferobject);
// Create voxelizer state
extern void StateVoxelizer(
	RenderState& renderState,
	VulkanCore* core,
	VkDescriptorSet staticDescriptorSet,
	VkDescriptorSetLayout staticDescLayout,
	VkDescriptorSetLayout textureDescLayout,
	VKScene* scenes,
	uint32_t scenecount,
	uint32_t griduniformresolution,
	VoxelizerGrid* voxelizergrid);
// Create Post voxelizer state
extern void StatePostVoxelizer(
	RenderState& renderState,
	VulkanCore* core,
	uint32_t uniformGridSize,
	VoxelizerGrid* voxelgrids,
	uint32_t voxelgridcount,
	VoxelGrid* voxelgrid,
	VkSampler sampler);

// Create the command buffers
extern void CommandGridCulling(
	RenderState& renderstate,
	VulkanCore* core,
	uint32_t meshcount,
	glm::vec3 gridPosition,
	glm::vec3 gridSize);
// Create the forward renderer 
extern void CommandForwardRenderer(
	RenderState& renderstate,
	VulkanCore* core,
	VKScene* scenes,
	uint32_t sceneCount,
	VKMesh* meshes,
	uint32_t meshCount,
	VkDescriptorSet staticdescriptorset,
	VkDescriptorSet* texturedescriptorset);
// Create indirect forward renderer
extern void CommandIndirectForwardRender(
	RenderState& renderstate,
	VulkanCore* core,
	VKScene* scenes,
	uint32_t sceneCount,
	VKMesh* meshes,
	uint32_t meshCount,
	VkDescriptorSet staticdescriptorset,
	VkDescriptorSet* texturedescriptorset,
	VkBuffer indirectcommandbuffer);
// create indirect command debug renderer
extern void CommandIndirectDebugRenderer(
	RenderState& renderstate,
	VulkanCore* core,
	VkDescriptorSet staticDescriptorSet,
	VkBuffer indirectcommandbuffer);
// create indirect command voxelizer
extern void CommandIndirectVoxelizer(
	RenderState& renderstate,
	VulkanCore* core,
	VKScene* scenes,
	uint32_t sceneCount,
	VKMesh* meshes,
	uint32_t meshCount,
	VkDescriptorSet staticdescriptorset,
	VkDescriptorSet* texturedescriptorset,
	VkBuffer indirectcommandbuffer);
// create indirect command post voxelizer
extern void CommandIndirectPostVoxelizer(
	RenderState& renderstate,
	VulkanCore* core,
	VoxelGrid* voxelgrid,
	glm::ivec3 gridresolution,
	uint32_t cascadenum);

// Update dynamic uniform buffer object voxelizer
extern void UpdateDUBOVoxelizer(
	RenderState& renderstate,
	VulkanCore& core,
	glm::vec4 voxelregionworld,
	glm::ivec3 voxelresolution);

// Pipeline helper functions
extern void DestroyRenderStates(RenderState& rs, VulkanCore* core, VkCommandPool commandpool);
extern void DestroyCommandBuffer(RenderState& rs, VulkanCore* core, VkCommandPool commandpool);
extern void BuildCommandBuffer(RenderState& rs, VkCommandPool commandpool, VulkanCore* core, uint32_t framebufferCount, VkFramebuffer* framebuffers);
extern void CreateBasicRenderstate(
	RenderState& rs,
	VulkanCore* core,
	uint32_t querycount,
	uint32_t framebufferCount,
	uint32_t bufferCount,
	uint32_t semaphoreCount,
	bool pipelinecache,
	uint32_t descriptorlayoutCount,
	uint32_t descriptorsetCount,
	uint32_t pipelineCount);


#endif //PIPELINESTATES_H