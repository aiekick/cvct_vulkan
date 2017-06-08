#ifndef SCENETOOLS_H
#define SCENETOOLS_H

#include <vulkan.h>
#include "DataTypes.h"
#include "AssetManager.h"

class VulkanCore;

// Scene helper function
extern uint32_t CreateVulkanScenes(
	VulkanCore* core,
	VkCommandBuffer commandbuffer,
	VKScene* scenes,
	uint32_t scenecount,
	PerSceneUBO* sceneubo);
extern uint32_t CreateVulkanMeshes(
	VulkanCore* core,
	VkCommandBuffer commandbuffer,
	VKScene* scenes,
	uint32_t scenecount,
	VKMesh* meshes,
	uint32_t& meshcount,
	VKSubMesh* submeshes,
	uint32_t& submeshcount,
	PerSubMeshUBO* submeshubo);
// Create the vulkan meshes
extern uint32_t CreateVulkanTextures(
	VulkanCore* core,
	VkCommandBuffer commandbuffer,
	AssetManager& assetmanager,
	VKScene* scenes,
	uint32_t scenecount,
	VKTexture* textures,
	uint32_t& texturecount,
	VkSampler sampler,
	VkDescriptorSet descriptorset);
// Create the vulkan textures
extern uint32_t CreateTexture(
	VulkanCore* core,
	AssetManager& assetmanager,
	VkCommandBuffer commandbuffer,
	VkDescriptorSet descriptorset,
	VkSampler sampler,
	VKTexture* textures,
	uint32_t texturecount,
	uint32_t index,
	const char* path);
// Create the vulkan buffers
extern void CreateVulkanBuffers(
	VulkanCore* core,
	BufferObject& persceneBO,
	BufferObject& persubmeshBO,
	BufferObject& cameraBO,
	VKScene* scene,
	uint32_t sceneCount);
// Create the AABB for the indirect drawing
extern uint32_t CreateAABB(
	VKScene* scenes,
	uint32_t scenecount,
	VKMesh* meshes,
	uint32_t meshcount,
	InstanceDataAABB* aabb);
// Create the samplers
extern uint32_t CreateSamplers(
	VkDevice device,
	VkDescriptorSet descriptorset,
	VkSampler& sampler);
// Load the scene
extern void LoadScene(
	AssetManager& assetmanager, 
	VKScene* scenes, 
	uint32_t& scenecount, 
	const char* path,
	uint32_t pathLength,
	glm::mat4& model);
// Build mipmaps for the vktextures
extern uint32_t BuildCommandMip(
	VulkanCore* core,
	VKTexture* textures,
	uint32_t texturecount);
// Create the static descriptors
extern void CreateDescriptor(
	VulkanCore* core,
	uint32_t textureDescriptorsetCount,
	VkDescriptorSetLayout& staticdescriptorlayout,
	VkDescriptorSetLayout& texturedescriptorlayout,
	VkDescriptorPool& staticpool,
	VkDescriptorPool& texturepool,
	VkDescriptorSet& staticdescriptorset,
	VkDescriptorSet* texturedescriptorset,
	BufferObject& persceneBO,
	BufferObject& permeshBO,
	BufferObject& cameraBO);
// Upload the AABB colliders
extern uint32_t UploadVulkanBuffers(
	VulkanCore* core,
	BufferObject& persceneBO,
	PerSceneUBO* perscene,
	uint32_t scenecount,
	BufferObject& persubmeshBO,
	PerSubMeshUBO* persubmesh,
	uint32_t submeshcount);
extern uint32_t UpdateDescriptorSets(
	VulkanCore* core,
	VKScene* scenes,
	uint32_t scenecount,
	VKMesh* meshes,
	uint32_t& meshcount,
	VkDescriptorSet staticdescriptorset,
	VkDescriptorSet* texturedescriptorset);
extern uint32_t CreateIndirectDrawBuffer(
	VulkanCore* core,
	VkDrawIndexedIndirectCommand* indirectCommands,
	uint32_t buffersize,
	VKSubMesh* submeshes,
	uint32_t submeshcount);
extern int32_t CreateVoxelSamplers(
	VulkanCore& core,
	VkSampler& linear,
	VkSampler& point);
extern int32_t CreateVoxelTextures(
	VulkanCore& core,
	uint32_t gridresolution,
	uint32_t axisscalar,
	VoxelizerGrid* voxelizergrid,
	uint32_t voxelizergridcount,
	VoxelGrid& voxelgrid,
	uint32_t mipcount,
	uint32_t cascadecount);

// Load asset from assetmanager
extern void LoadAssetStaticManager(AssetManager& manager, char* path, uint32_t pathLenght);

#endif	//SCENETOOLS_H