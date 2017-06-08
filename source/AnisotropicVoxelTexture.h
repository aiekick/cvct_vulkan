#ifndef ANISOTROPICVOXELTEXTURE_H
#define ANISOTROPICVOXELTEXTURE_H

#include <malloc.h>
#include <vulkan.h>

#include "Defines.h"
#include "VKTools.h"

class VulkanCore;

#define GRIDMIPMAP 3

struct AnisotropicVoxelTexture
{
	uint32_t				m_width, m_height, m_depth, m_mipNum, m_cascadeCount;
	//glm::vec4				m_voxelRegionWorld;
	VkSampler				m_sampler;
	VkSampler				m_conetraceSampler;
	VkImage					m_image;
	VkImage					m_imageAlpha;
	VkImageLayout			m_imageLayout;
	// ordered as dir1mip0,dir1mip1,dir1mip2, dir2mip0,dir2mip1
	VkImageView				m_view[GRIDMIPMAP];
	VkImageView				m_alphaView;
	VkDescriptorImageInfo	m_descriptor[GRIDMIPMAP];
	VkDescriptorImageInfo	m_alphaDescriptor;
	VkFormat				m_format;
	VkDeviceMemory			m_deviceMemory;
	VkDeviceMemory			m_alphaDeviceMemory;
};


struct SparseAnisotropicVoxelTexture
{
	uint32_t				m_width, m_height, m_depth, m_mipNum;
	VkSampler				m_sampler;
	VkImage					m_image[VoxelDirections::NUM_DIRECTIONS];
	VkImageLayout			m_imageLayout;
	VkImageView				m_view;
	VkDescriptorImageInfo	m_descriptor;
	VkFormat				m_format;
	VkDeviceMemory			m_deviceMemory;
};

//creates anisotropic voxels
extern int32_t CreateAnisotropicVoxelTexture(
	AnisotropicVoxelTexture* avt,
	uint32_t width, uint32_t height, uint32_t depth,
	VkFormat format, uint32_t numMipMaps, uint32_t numCascades,
	VkPhysicalDevice deviceHandle, VkDevice viewDevice, VulkanCore* vulkanCore);

extern void DestroyAnisotropicVoxelTexture(
	AnisotropicVoxelTexture* avt,
	VkDevice view);

extern uint32_t ClearAnisotropicVoxelTexture(
	AnisotropicVoxelTexture* avt,
	VkCommandBuffer cmdbuffer);


#endif	//anisotropicvoxeltexture_h