#include "AnisotropicVoxelTexture.h"
#include "VulkanCore.h"
#include "VKTools.h"

//creates anisotropic voxels
int32_t CreateAnisotropicVoxelTexture(
	AnisotropicVoxelTexture* avt,
	uint32_t width, uint32_t height, uint32_t depth,
	VkFormat format, uint32_t numMipMaps, uint32_t numCascades,
	VkPhysicalDevice deviceHandle, VkDevice viewDevice, VulkanCore* vulkanCore)
{
	VkFormatProperties formatProperties;

	// Get device properties for the requested texture format
	vkGetPhysicalDeviceFormatProperties(vulkanCore->GetPhysicalGPU(), format, &formatProperties);
	// Check if requested image format supports image storage operations
	assert(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT);

	avt->m_width = width, avt->m_height = height, avt->m_depth = depth, avt->m_cascadeCount = numCascades;
	avt->m_mipNum = numMipMaps;
	avt->m_format = format;
	avt->m_imageLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;

	VkImageCreateInfo imageCreateInfo = VKTools::Initializers::ImageCreateInfo();
	imageCreateInfo.imageType = VK_IMAGE_TYPE_3D;
	imageCreateInfo.flags = 0;
	imageCreateInfo.format = avt->m_format;
	imageCreateInfo.extent = { avt->m_width * NUM_DIRECTIONS, avt->m_height * avt->m_cascadeCount, avt->m_depth };
	imageCreateInfo.mipLevels = avt->m_mipNum;
	imageCreateInfo.arrayLayers = 1;
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;			//todo might error
	imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCreateInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;		//TODO: might cause problems
	imageCreateInfo.initialLayout = avt->m_imageLayout;

	// Load mip map level 0 to linear tiling image
	VK_CHECK_RESULT(vkCreateImage(viewDevice, &imageCreateInfo, NULL, &avt->m_image));

	// create the alpha maps
	imageCreateInfo.mipLevels = 1;
	VK_CHECK_RESULT(vkCreateImage(viewDevice, &imageCreateInfo, NULL, &avt->m_imageAlpha));

	// Allocate memory on GPU
	VkMemoryAllocateInfo memAllocInfo = VKTools::Initializers::MemoryAllocateCreateInfo();
	VkMemoryRequirements memReqs;

	// Check if the current bit is active in the memory requirements
	vkGetImageMemoryRequirements(viewDevice, avt->m_image, &memReqs);
	memAllocInfo.memoryTypeIndex = vulkanCore->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	memAllocInfo.allocationSize = memReqs.size;
	VK_CHECK_RESULT(vkAllocateMemory(viewDevice, &memAllocInfo, NULL, &avt->m_deviceMemory));
	VK_CHECK_RESULT(vkBindImageMemory(viewDevice, avt->m_image, avt->m_deviceMemory, 0));

	// Create alpha memory
	vkGetImageMemoryRequirements(viewDevice, avt->m_imageAlpha, &memReqs);
	memAllocInfo.memoryTypeIndex = vulkanCore->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	memAllocInfo.allocationSize = memReqs.size;
	VK_CHECK_RESULT(vkAllocateMemory(viewDevice, &memAllocInfo, NULL, &avt->m_alphaDeviceMemory));
	VK_CHECK_RESULT(vkBindImageMemory(viewDevice, avt->m_imageAlpha, avt->m_alphaDeviceMemory, 0));

	// Change the layout to VK_IMAGE_LAYOUT_GENERAL
	avt->m_imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	VkCommandBuffer changeLayout = VKTools::Initializers::CreateCommandBuffer(vulkanCore->GetGraphicsCommandPool(), viewDevice, VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
	// Image barrier for optimal image (target)
	// Set initial layout for all array layers (faces) of the optimal (target) tiled texture
	VkImageSubresourceRange subresourceRange = {};
	subresourceRange.baseMipLevel = 0;
	subresourceRange.levelCount = 3;
	subresourceRange.layerCount = 1;
	subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	VKTools::SetImageLayout(changeLayout, avt->m_image, VK_IMAGE_LAYOUT_UNDEFINED, avt->m_imageLayout, subresourceRange);
	subresourceRange.levelCount = 1;
	VKTools::SetImageLayout(changeLayout, avt->m_imageAlpha, VK_IMAGE_LAYOUT_UNDEFINED, avt->m_imageLayout, subresourceRange);

	// Flush the commands
	VKTools::FlushCommandBuffer(changeLayout, vulkanCore->GetGraphicsQueue(), viewDevice, vulkanCore->GetGraphicsCommandPool(), true);

	// Create the mip-mapper sampler
	VkSamplerCreateInfo sampler = VKTools::Initializers::SamplerCreateInfo();
	sampler.magFilter = VK_FILTER_NEAREST;
	sampler.minFilter = VK_FILTER_NEAREST;
	sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler.mipLodBias = 0.0f;
	sampler.compareEnable = VK_FALSE;
	sampler.compareOp = VK_COMPARE_OP_NEVER;
	sampler.minLod = 0.0f;
	sampler.maxLod = (float)avt->m_mipNum;
	sampler.anisotropyEnable = VK_FALSE;
	sampler.maxAnisotropy = 1.0f;
	sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	VK_CHECK_RESULT(vkCreateSampler(viewDevice, &sampler, NULL, &avt->m_sampler));

	// Create the conetracer sampler
	sampler.magFilter = VK_FILTER_LINEAR;							//Enable trilinear filtering
	sampler.minFilter = VK_FILTER_LINEAR;							//Enable trilinear filtering
	sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;				//Enable trilinear filtering
	sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler.mipLodBias = -0.0f;										// Todo: check this out. Don't add a bias for now.
	sampler.compareEnable = VK_FALSE;
	sampler.compareOp = VK_COMPARE_OP_NEVER;
	sampler.minLod = 0.0f;
	sampler.maxLod = (float)avt->m_mipNum;
	sampler.anisotropyEnable = VK_FALSE;
	sampler.maxAnisotropy = 1.0f;
	sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	VK_CHECK_RESULT(vkCreateSampler(viewDevice, &sampler, NULL, &avt->m_conetraceSampler));

	//create the image view
	VkImageViewCreateInfo view = VKTools::Initializers::ImageViewCreateInfo();
	view.viewType = VK_IMAGE_VIEW_TYPE_3D;
	view.format = avt->m_format;
	view.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
	view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

	view.subresourceRange.levelCount = avt->m_mipNum;
	view.subresourceRange.baseArrayLayer = 0;
	view.subresourceRange.layerCount = 1;
	for (uint32_t j = 0; j < GRIDMIPMAP; j++)
	{
		view.subresourceRange.baseMipLevel = j;
		view.image = avt->m_image;
		VK_CHECK_RESULT(vkCreateImageView(viewDevice, &view, nullptr, &avt->m_view[j]));

		//create the descriptor mip0
		avt->m_descriptor[j].imageLayout = avt->m_imageLayout;
		avt->m_descriptor[j].imageView = avt->m_view[j];
		avt->m_descriptor[j].sampler = avt->m_sampler;
	}

	// Create the alpha descriptor
	view.subresourceRange.levelCount = 1;
	view.subresourceRange.baseMipLevel = 0;
	view.image = avt->m_imageAlpha;
	VK_CHECK_RESULT(vkCreateImageView(viewDevice, &view, nullptr, &avt->m_alphaView));
	avt->m_alphaDescriptor.imageLayout = avt->m_imageLayout;
	avt->m_alphaDescriptor.imageView = avt->m_alphaView;
	avt->m_alphaDescriptor.sampler = avt->m_sampler;

	return 0;	//everything is OK!
}

void DestroyAnisotropicVoxelTexture(
	AnisotropicVoxelTexture* avt,
	VkDevice view)
{
	// Destroy samplers
	vkDestroySampler(view, avt->m_sampler, NULL);
	vkDestroySampler(view, avt->m_conetraceSampler, NULL);
	// Destroy images
	vkDestroyImage(view, avt->m_image, NULL);
	vkDestroyImage(view, avt->m_imageAlpha, NULL);
	for (uint32_t i = 0; i < GRIDMIPMAP; i++)
	{
		// Destroy imageview
		vkDestroyImageView(view, avt->m_view[i], NULL);
		// Destroy descriptors
		avt->m_descriptor[i].imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		avt->m_descriptor[i].imageView = VK_NULL_HANDLE;
		avt->m_descriptor[i].sampler = VK_NULL_HANDLE;
	}
	// Destroy imageview
	vkDestroyImageView(view, avt->m_alphaView, NULL);
	// Destroy descriptors
	avt->m_alphaDescriptor.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	avt->m_alphaDescriptor.imageView = VK_NULL_HANDLE;
	avt->m_alphaDescriptor.sampler = VK_NULL_HANDLE;
	// free memory
	vkFreeMemory(view, avt->m_deviceMemory, NULL);
	vkFreeMemory(view, avt->m_alphaDeviceMemory, NULL);
	avt->m_format = VK_FORMAT_UNDEFINED;
	avt->m_imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	avt->m_width, avt->m_height, avt->m_depth, avt->m_mipNum, avt->m_cascadeCount = 0;
}

uint32_t ClearAnisotropicVoxelTexture(
	AnisotropicVoxelTexture* avt,
	VkCommandBuffer cmdbuffer
)
{
	VkClearColorValue clearVal = { { 0.0f, 0.0f, 0.0f, 0.0f } };
	VkImageSubresourceRange sr;
	sr.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	sr.baseMipLevel = 0;
	sr.levelCount = avt->m_mipNum;
	sr.baseArrayLayer = 0;
	sr.layerCount = 1;
	vkCmdClearColorImage(cmdbuffer, avt->m_image, avt->m_imageLayout, &clearVal, 1, &sr);

	sr.levelCount = 1;
	vkCmdClearColorImage(cmdbuffer, avt->m_imageAlpha, avt->m_imageLayout, &clearVal, 1, &sr);

	return 0;
}