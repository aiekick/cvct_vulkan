#include "AnisotropicVoxelTexture.h"
#include "VulkanCore.h"
#include "VKTools.h"

#define MAX_LOD 99

int32_t CreateLinearSampler(
	VulkanCore& core,
	VkSampler& sampler)
{
	// Create the conetracer sampler
	VkSamplerCreateInfo linearsampler = VKTools::Initializers::SamplerCreateInfo();
	linearsampler.magFilter = VK_FILTER_LINEAR;								//Enable trilinear filtering
	linearsampler.minFilter = VK_FILTER_LINEAR;								//Enable trilinear filtering
	linearsampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;				//Enable trilinear filtering
	linearsampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	linearsampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	linearsampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	linearsampler.mipLodBias = -0.0f;										// Todo: check this out. Don't add a bias for now.
	linearsampler.compareEnable = VK_FALSE;
	linearsampler.compareOp = VK_COMPARE_OP_NEVER;
	linearsampler.minLod = 0.0f;
	linearsampler.maxLod = (float)MAX_LOD;
	linearsampler.anisotropyEnable = VK_FALSE;
	linearsampler.maxAnisotropy = 1.0f;
	linearsampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	VK_CHECK_RESULT(vkCreateSampler(core.GetViewDevice(), &linearsampler, NULL, &sampler));

	return 0;
}

int32_t CreatePointSampler(
	VulkanCore& core,
	VkSampler& sampler)
{
	// Create the mip-mapper sampler
	VkSamplerCreateInfo pointsampler = VKTools::Initializers::SamplerCreateInfo();
	pointsampler.magFilter = VK_FILTER_NEAREST;
	pointsampler.minFilter = VK_FILTER_NEAREST;
	pointsampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	pointsampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	pointsampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	pointsampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	pointsampler.mipLodBias = 0.0f;
	pointsampler.compareEnable = VK_FALSE;
	pointsampler.compareOp = VK_COMPARE_OP_NEVER;
	pointsampler.minLod = 0.0f;
	pointsampler.maxLod = (float)MAX_LOD;
	pointsampler.anisotropyEnable = VK_FALSE;
	pointsampler.maxAnisotropy = 1.0f;
	pointsampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	VK_CHECK_RESULT(vkCreateSampler(core.GetViewDevice(), &pointsampler, NULL, &sampler));

	return 0;
}

// Create isotropic voxel texture
int32_t CreateIsotropicVoxelTexture(
	VulkanCore& core,
	IsotropicVoxelTexture& ivt,
	uint32_t width, uint32_t height, uint32_t depth, uint32_t mipcount,
	VkFormat format, uint32_t cascadecount)
{
	// Device properties
	VkPhysicalDevice handle = core.GetPhysicalGPU();
	VkDevice view = core.GetViewDevice();

	VkFormatProperties formatProperties;
	// Get device properties for the requested texture format
	vkGetPhysicalDeviceFormatProperties(handle, format, &formatProperties);
	// Check if requested image format supports image storage operations
	assert(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT);
	ivt.width = width, ivt.height = height, ivt.depth = depth, ivt.cascadecount = cascadecount;

	// calculate mip level
	ivt.mipcount = mipcount;
	ivt.format = format;
	ivt.imagelayout = VK_IMAGE_LAYOUT_PREINITIALIZED;

	VkImageCreateInfo imageCreateInfo = VKTools::Initializers::ImageCreateInfo();
	imageCreateInfo.imageType = VK_IMAGE_TYPE_3D;
	imageCreateInfo.flags = 0;
	imageCreateInfo.format = ivt.format;
	imageCreateInfo.extent = { ivt.width, ivt.height * ivt.cascadecount , ivt.depth };
	imageCreateInfo.mipLevels = ivt.mipcount;
	imageCreateInfo.arrayLayers = 1;
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;			//todo might error
	imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCreateInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;		//TODO: might cause problems
	imageCreateInfo.initialLayout = ivt.imagelayout;
	VK_CHECK_RESULT(vkCreateImage(view, &imageCreateInfo, NULL, &ivt.image));

	// Allocate memory on GPU
	VkMemoryAllocateInfo memAllocInfo = VKTools::Initializers::MemoryAllocateCreateInfo();
	VkMemoryRequirements memReqs;
	// Check if the current bit is active in the memory requirements
	vkGetImageMemoryRequirements(view, ivt.image, &memReqs);
	memAllocInfo.memoryTypeIndex = core.GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	memAllocInfo.allocationSize = memReqs.size;
	VK_CHECK_RESULT(vkAllocateMemory(view, &memAllocInfo, NULL, &ivt.deviceMemory));
	VK_CHECK_RESULT(vkBindImageMemory(view, ivt.image, ivt.deviceMemory, 0));

	// Change the layout to VK_IMAGE_LAYOUT_GENERAL
	ivt.imagelayout = VK_IMAGE_LAYOUT_GENERAL;
	VkCommandBuffer changeLayout = VKTools::Initializers::CreateCommandBuffer(core.GetGraphicsCommandPool(), view, VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
	// Image barrier for optimal image (target)
	// Set initial layout for all array layers (faces) of the optimal (target) tiled texture
	VkImageSubresourceRange subresourceRange = {};
	subresourceRange.baseMipLevel = 0;
	subresourceRange.levelCount = cascadecount;
	subresourceRange.layerCount = 1;
	subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	VKTools::SetImageLayout(changeLayout, ivt.image, VK_IMAGE_LAYOUT_UNDEFINED, ivt.imagelayout, subresourceRange);

	// Flush the commands
	VKTools::FlushCommandBuffer(changeLayout, core.GetGraphicsQueue(), view, core.GetGraphicsCommandPool(), true);

	//create the image view
	VkImageViewCreateInfo imageview = VKTools::Initializers::ImageViewCreateInfo();
	imageview.viewType = VK_IMAGE_VIEW_TYPE_3D;
	imageview.format = ivt.format;
	imageview.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
	imageview.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

	imageview.subresourceRange.levelCount = ivt.mipcount;
	imageview.subresourceRange.baseArrayLayer = 0;
	imageview.subresourceRange.layerCount = 1;
	for (uint32_t j = 0; j < ivt.mipcount; j++)
	{
		imageview.subresourceRange.baseMipLevel = j;
		imageview.image = ivt.image;
		VK_CHECK_RESULT(vkCreateImageView(view, &imageview, nullptr, &ivt.view[j]));

		//create the descriptor mip0
		ivt.descriptor[j].imageLayout = ivt.imagelayout;
		ivt.descriptor[j].imageView = ivt.view[j];
		ivt.descriptor[j].sampler = ivt.sampler;
	}

	return 0;	//everything is OK!
}

// Creates anisotropic voxels
int32_t CreateAnisotropicVoxelTexture(
	VulkanCore& core,
	AnisotropicVoxelTexture& avt,
	uint32_t width, uint32_t height, uint32_t depth,
	VkFormat format, uint32_t mipcount, uint32_t cascadecount)
{
	VkFormatProperties formatProperties;
	VkPhysicalDevice handle = core.GetPhysicalGPU();
	VkDevice view = core.GetViewDevice();

	// Get device properties for the requested texture format
	vkGetPhysicalDeviceFormatProperties(handle, format, &formatProperties);
	// Check if requested image format supports image storage operations
	assert(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT);

	avt.width = width, avt.height = height, avt.depth = depth, avt.cascadecount = cascadecount;
	avt.mipcount = mipcount;
	avt.format = format;
	avt.imageLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;

	VkImageCreateInfo imageCreateInfo = VKTools::Initializers::ImageCreateInfo();
	imageCreateInfo.imageType = VK_IMAGE_TYPE_3D;
	imageCreateInfo.flags = 0;
	imageCreateInfo.format = avt.format;
	imageCreateInfo.extent = { avt.width * NUM_DIRECTIONS, avt.height * avt.cascadecount, avt.depth };
	imageCreateInfo.mipLevels = avt.mipcount;
	imageCreateInfo.arrayLayers = 1;
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;			//todo might error
	imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCreateInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;		//TODO: might cause problems
	imageCreateInfo.initialLayout = avt.imageLayout;

	// Load mip map level 0 to linear tiling image
	VK_CHECK_RESULT(vkCreateImage(view, &imageCreateInfo, NULL, &avt.image));

	// Allocate memory on GPU
	VkMemoryAllocateInfo memAllocInfo = VKTools::Initializers::MemoryAllocateCreateInfo();
	VkMemoryRequirements memReqs;
	// Check if the current bit is active in the memory requirements
	vkGetImageMemoryRequirements(view, avt.image, &memReqs);
	memAllocInfo.memoryTypeIndex = core.GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	memAllocInfo.allocationSize = memReqs.size;
	VK_CHECK_RESULT(vkAllocateMemory(view, &memAllocInfo, NULL, &avt.deviceMemory));
	VK_CHECK_RESULT(vkBindImageMemory(view, avt.image, avt.deviceMemory, 0));

	// Change the layout to VK_IMAGE_LAYOUT_GENERAL
	avt.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	VkCommandBuffer changeLayout = VKTools::Initializers::CreateCommandBuffer(core.GetGraphicsCommandPool(), view, VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
	// Image barrier for optimal image (target)
	// Set initial layout for all array layers (faces) of the optimal (target) tiled texture
	VkImageSubresourceRange subresourceRange = {};
	subresourceRange.baseMipLevel = 0;
	subresourceRange.levelCount = mipcount;
	subresourceRange.layerCount = 1;
	subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	VKTools::SetImageLayout(changeLayout, avt.image, VK_IMAGE_LAYOUT_UNDEFINED, avt.imageLayout, subresourceRange);

	// Flush the commands
	VKTools::FlushCommandBuffer(changeLayout, core.GetGraphicsQueue(), view, core.GetGraphicsCommandPool(), true);

	//create the image view
	VkImageViewCreateInfo imageview = VKTools::Initializers::ImageViewCreateInfo();
	imageview.viewType = VK_IMAGE_VIEW_TYPE_3D;
	imageview.format = avt.format;
	imageview.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
	imageview.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

	imageview.subresourceRange.levelCount = avt.mipcount;
	imageview.subresourceRange.baseArrayLayer = 0;
	imageview.subresourceRange.layerCount = 1;
	for (uint32_t j = 0; j < avt.mipcount; j++)
	{
		imageview.subresourceRange.baseMipLevel = j;
		imageview.image = avt.image;
		VK_CHECK_RESULT(vkCreateImageView(view, &imageview, nullptr, &avt.view[j]));

		//create the descriptor mip0
		avt.descriptor[j].imageLayout = avt.imageLayout;
		avt.descriptor[j].imageView = avt.view[j];
		avt.descriptor[j].sampler = avt.sampler;
	}

	return 0;	//everything is OK!
}

void DestroyAnisotropicVoxelTexture(
	AnisotropicVoxelTexture& avt,
	VkDevice view)
{
	// Destroy samplers
	vkDestroySampler(view, avt.sampler, NULL);
	vkDestroySampler(view, avt.conetraceSampler, NULL);
	// Destroy images
	vkDestroyImage(view, avt.image, NULL);
	for (uint32_t i = 0; i < avt.mipcount; i++)
	{
		// Destroy imageview
		vkDestroyImageView(view, avt.view[i], NULL);
		// Destroy descriptors
		avt.descriptor[i].imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		avt.descriptor[i].imageView = VK_NULL_HANDLE;
		avt.descriptor[i].sampler = VK_NULL_HANDLE;
	}
	// Destroy imageview
	//vkDestroyImageView(view, avt->m_alphaView, NULL);
	// Destroy descriptors
	//avt->m_alphaDescriptor.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	//avt->m_alphaDescriptor.imageView = VK_NULL_HANDLE;
	//avt->m_alphaDescriptor.sampler = VK_NULL_HANDLE;
	// free memory
	vkFreeMemory(view, avt.deviceMemory, NULL);
	//vkFreeMemory(view, avt->m_alphaDeviceMemory, NULL);
	avt.format = VK_FORMAT_UNDEFINED;
	avt.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	avt.width, avt.height, avt.depth, avt.mipcount, avt.cascadecount = 0;
}

uint32_t ClearAnisotropicVoxelTexture(
	AnisotropicVoxelTexture& avt,
	VkCommandBuffer cmdbuffer)
{
	VkClearColorValue clearVal = { { 0.0f, 0.0f, 0.0f, 0.0f } };
	VkImageSubresourceRange sr;
	sr.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	sr.baseMipLevel = 0;
	sr.levelCount = avt.mipcount;
	sr.baseArrayLayer = 0;
	sr.layerCount = 1;
	vkCmdClearColorImage(cmdbuffer, avt.image, avt.imageLayout, &clearVal, 1, &sr);

	return 0;
}