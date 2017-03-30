#ifndef ANISOTROPICVOXELTEXTURE_H
#define ANISOTROPICVOXELTEXTURE_H

#include <malloc.h>
#include <vulkan.h>

#include "Defines.h"
#include "VKTools.h"

class VulkanCore;

// create the voxel samplers
extern int32_t CreateLinearSampler(
	VulkanCore& core,
	VkSampler& sampler);

extern int32_t CreatePointSampler(
	VulkanCore& core,
	VkSampler& sampler);

//creates anisotropic voxels
extern int32_t CreateAnisotropicVoxelTexture(
	VulkanCore& core,
	AnisotropicVoxelTexture& avt,
	uint32_t width, uint32_t height, uint32_t depth,
	VkFormat format, uint32_t mipcount, uint32_t cascadecount);

extern int32_t CreateIsotropicVoxelTexture(
	VulkanCore& core,
	IsotropicVoxelTexture& ivt,
	uint32_t width, uint32_t height, uint32_t depth, uint32_t mipcount,
	VkFormat format, uint32_t cascadecount);

extern void DestroyAnisotropicVoxelTexture(
	AnisotropicVoxelTexture& avt,
	VkDevice view);

extern uint32_t ClearAnisotropicVoxelTexture(
	AnisotropicVoxelTexture& avt,
	VkCommandBuffer cmdbuffer);


#endif	//anisotropicvoxeltexture_h