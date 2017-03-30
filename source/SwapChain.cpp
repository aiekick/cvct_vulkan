#include "SwapChain.h"
#include <assert.h>
#include <VKTools.h>
#include <vector>
#include "Defines.h"

SwapChain::SwapChain()
{
}


SwapChain::~SwapChain()
{
}

void SwapChain::Initialize(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device)
{
	m_instance= instance;
	m_physicalDevice = physicalDevice;
	m_viewDevice = device;
	//set the function pointers
	GET_INSTANCE_PROC_ADDR(instance, GetPhysicalDeviceSurfaceSupportKHR);
	GET_INSTANCE_PROC_ADDR(instance, GetPhysicalDeviceSurfaceCapabilitiesKHR);
	GET_INSTANCE_PROC_ADDR(instance, GetPhysicalDeviceSurfaceFormatsKHR);
	GET_INSTANCE_PROC_ADDR(instance, GetPhysicalDeviceSurfacePresentModesKHR);
	GET_DEVICE_PROC_ADDR(device, CreateSwapchainKHR);
	GET_DEVICE_PROC_ADDR(device, DestroySwapchainKHR);
	GET_DEVICE_PROC_ADDR(device, GetSwapchainImagesKHR);
	GET_DEVICE_PROC_ADDR(device, AcquireNextImageKHR);
	GET_DEVICE_PROC_ADDR(device, QueuePresentKHR);
}

void SwapChain::InitializeSurface(GLFWwindow* window)
{
	VkResult err;

	err = glfwCreateWindowSurface(m_instance, window, NULL, &m_surface);

	//get number of queues				// Get available queue family properties
	uint32_t queueCount;
	vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueCount, NULL);
	assert(queueCount >= 1);

	//put properties of queue in a vector
	std::vector<VkQueueFamilyProperties> queueProps(queueCount);
	vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueCount, queueProps.data());

	// Iterate over each queue to learn whether it supports presenting:
	// Find a queue with present support
	// Will be used to present the swap chain images to the windowing system
	std::vector<VkBool32> supportsPresent(queueCount);
	for (uint32_t i = 0; i < queueCount; i++)
	{
		vkGetPhysicalDeviceSurfaceSupportKHR(m_physicalDevice, i, m_surface, &supportsPresent[i]);
	}

	// Search for a graphics and a present queue in the array of queue
	// families, try to find one that supports both
	uint32_t graphicsQueueNodeIndex = UINT32_MAX;
	uint32_t presentQueueNodeIndex = UINT32_MAX;
	for (uint32_t i = 0; i < queueCount; i++)
	{
		if ((queueProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
		{
			if (graphicsQueueNodeIndex == UINT32_MAX)
			{
				graphicsQueueNodeIndex = i;
			}

			if (supportsPresent[i] == VK_TRUE)
			{
				graphicsQueueNodeIndex = i;
				presentQueueNodeIndex = i;
				break;
			}
		}
	}
	if (presentQueueNodeIndex == UINT32_MAX)
	{
		// If there's no queue that supports both present and graphics
		// try to find a separate present queue
		for (uint32_t i = 0; i < queueCount; ++i)
		{
			if (supportsPresent[i] == VK_TRUE)
			{
				presentQueueNodeIndex = i;
				break;
			}
		}
	}

	// Exit if either a graphics or a presenting queue hasn't been found
	if (graphicsQueueNodeIndex == UINT32_MAX || presentQueueNodeIndex == UINT32_MAX)
	{
		VKTools::ExitFatal("Could not find a graphics and/or presenting queue!", "Fatal error");
	}

	// todo : Add support for separate graphics and presenting queue
	if (graphicsQueueNodeIndex != presentQueueNodeIndex)
	{
		VKTools::ExitFatal("Separate graphics and presenting queues are not supported yet!", "Fatal error");
	}

	m_queueNodeIndex = graphicsQueueNodeIndex;

	// Get list of supported surface formats
	uint32_t formatCount;
	err = fpGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &formatCount, NULL);
	assert(!err);
	assert(formatCount > 0);

	std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
	err = fpGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &formatCount, surfaceFormats.data());
	assert(!err);

	// If the surface format list only includes one entry with VK_FORMAT_UNDEFINED,
	// there is no preferered format, so we assume VK_FORMAT_B8G8R8A8_UNORM
	if ((formatCount == 1) && (surfaceFormats[0].format == VK_FORMAT_UNDEFINED))
	{
		m_colorFormat = VK_FORMAT_B8G8R8A8_UNORM;
	}
	else
	{
		// Always select the first available color format
		// If you need a specific format (e.g. SRGB) you'd need to
		// iterate over the list of available surface format and
		// check for it's presence
		m_colorFormat = surfaceFormats[0].format;
	}
	m_colorSpace = surfaceFormats[0].colorSpace;
}

void SwapChain::CreateSwapChain(VkCommandBuffer commandBuffer, uint32_t *width, uint32_t* height)
{
	VkResult err;
	VkSwapchainKHR oldSwapchain = m_swapChain;

	// Get physical device surface properties and formats
	VkSurfaceCapabilitiesKHR surfCaps;
	err = fpGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, m_surface, &surfCaps);
	assert(!err);

	// Get available present modes
	uint32_t presentModeCount;
	err = fpGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_surface, &presentModeCount, NULL);
	assert(!err);
	assert(presentModeCount > 0);

	std::vector<VkPresentModeKHR> presentModes(presentModeCount);

	err = fpGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_surface, &presentModeCount, presentModes.data());
	assert(!err);

	VkExtent2D swapchainExtent = {};
	// width and height are either both -1, or both not -1.
	if (surfCaps.currentExtent.width == -1)
	{
		// If the surface size is undefined, the size is set to
		// the size of the images requested.
		swapchainExtent.width = *width;
		swapchainExtent.height = *height;
	}
	else
	{
		// If the surface size is defined, the swap chain size must match
		swapchainExtent = surfCaps.currentExtent;
		*width = surfCaps.currentExtent.width;
		*height = surfCaps.currentExtent.height;
	}

	m_width = *width;
	m_height = *height;

	// Prefer mailbox mode if present, it's the lowest latency non-tearing present  mode
	VkPresentModeKHR swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;
	for (size_t i = 0; i < presentModeCount; i++)
	{
		if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			swapchainPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
			break;
		}
		if ((swapchainPresentMode != VK_PRESENT_MODE_MAILBOX_KHR) && (presentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR))
		{
			swapchainPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
		}
	}

	// Determine the number of images
	uint32_t desiredNumberOfSwapchainImages = surfCaps.minImageCount + 1;
	if ((surfCaps.maxImageCount > 0) && (desiredNumberOfSwapchainImages > surfCaps.maxImageCount))
	{
		desiredNumberOfSwapchainImages = surfCaps.maxImageCount;
	}

	VkSurfaceTransformFlagsKHR preTransform;
	if (surfCaps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
	{
		preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	}
	else
	{
		preTransform = surfCaps.currentTransform;
	}

	VkSwapchainCreateInfoKHR swapchainCI = {};
	swapchainCI.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchainCI.pNext = NULL;
	swapchainCI.surface = m_surface;
	swapchainCI.minImageCount = desiredNumberOfSwapchainImages;
	swapchainCI.imageFormat = m_colorFormat;
	swapchainCI.imageColorSpace = m_colorSpace;
	swapchainCI.imageExtent = { swapchainExtent.width, swapchainExtent.height };
	swapchainCI.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
	swapchainCI.preTransform = (VkSurfaceTransformFlagBitsKHR)preTransform;
	swapchainCI.imageArrayLayers = 1;
	swapchainCI.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	swapchainCI.queueFamilyIndexCount = 0;
	swapchainCI.pQueueFamilyIndices = NULL;
	swapchainCI.presentMode = swapchainPresentMode;
	swapchainCI.oldSwapchain = oldSwapchain;
	swapchainCI.clipped = true;
	swapchainCI.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

	err = fpCreateSwapchainKHR(m_viewDevice, &swapchainCI, NULL, &m_swapChain);
	assert(!err);

	// If an existing sawp chain is re-created, destroy the old swap chain
	// This also cleans up all the presentable images
	if (oldSwapchain != VK_NULL_HANDLE)
	{
		
		for (uint32_t i = 0; i < m_imageCount; i++)
		{
			// Destroy the image views
			vkDestroyImageView(m_viewDevice, m_buffers[i].view, NULL);
		}
		// Destroy the swapchain
		fpDestroySwapchainKHR(m_viewDevice, oldSwapchain, NULL);
		// Destroy the sampler
		vkDestroySampler(m_viewDevice, m_sampler, NULL);
	}

	err = fpGetSwapchainImagesKHR(m_viewDevice, m_swapChain, &m_imageCount, NULL);
	assert(!err);

	// Get the swap chain images
	m_images.resize(m_imageCount);
	err = fpGetSwapchainImagesKHR(m_viewDevice, m_swapChain, &m_imageCount, m_images.data());
	assert(!err);

	// Create sampler
	//create the texture sampler
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
	sci.anisotropyEnable = VK_TRUE;
	sci.maxAnisotropy = 16.0f;
	sci.compareEnable = VK_FALSE;
	sci.minLod = 0.0f;
	sci.maxLod = VK_LOD_CLAMP_NONE;
	sci.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	sci.unnormalizedCoordinates = VK_FALSE;
	//create the sampler
	result = vkCreateSampler(m_viewDevice, &sci, NULL, &m_sampler);
	if (result != VK_SUCCESS)
		ERROR_VOID("Failed to create sampler");

	// Get the swap chain buffers containing the image and imageview
	m_buffers.resize(m_imageCount);
	m_descriptors.resize(m_imageCount);
	for (uint32_t i = 0; i < m_imageCount; i++)
	{
		VkImageViewCreateInfo colorAttachmentView = {};
		colorAttachmentView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		colorAttachmentView.pNext = NULL;
		colorAttachmentView.format = m_colorFormat;
		colorAttachmentView.components = {
			VK_COMPONENT_SWIZZLE_R,
			VK_COMPONENT_SWIZZLE_G,
			VK_COMPONENT_SWIZZLE_B,
			VK_COMPONENT_SWIZZLE_A
		};
		colorAttachmentView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		colorAttachmentView.subresourceRange.baseMipLevel = 0;
		colorAttachmentView.subresourceRange.levelCount = 1;
		colorAttachmentView.subresourceRange.baseArrayLayer = 0;
		colorAttachmentView.subresourceRange.layerCount = 1;
		colorAttachmentView.viewType = VK_IMAGE_VIEW_TYPE_2D;
		colorAttachmentView.flags = 0;

		m_buffers[i].image = m_images[i];

		colorAttachmentView.image = m_buffers[i].image;

		err = vkCreateImageView(m_viewDevice, &colorAttachmentView, NULL, &m_buffers[i].view);

		// Create descriptorset
		m_descriptors[i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		m_descriptors[i].imageView = m_buffers[i].view;
		m_descriptors[i].sampler = m_sampler;
		
		assert(!err);
	}
}

VkResult SwapChain::AcquireNextImage(VkSemaphore presentCompleteSemaphore, uint32_t* currentBuffer)
{
	return fpAcquireNextImageKHR(m_viewDevice, m_swapChain, UINT64_MAX, presentCompleteSemaphore, (VkFence)nullptr, currentBuffer);
}

// Present the current image to the queue
VkResult SwapChain::QueuePresent(VkQueue queue, uint32_t currentBuffer)
{
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pNext = NULL;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &m_swapChain;
	presentInfo.pImageIndices = &currentBuffer;
	return fpQueuePresentKHR(queue, &presentInfo);
}

// Present the current image to the queue
VkResult  SwapChain::QueuePresent(VkQueue queue, uint32_t currentBuffer, VkSemaphore waitSemaphore)
{
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pNext = NULL;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &m_swapChain;
	presentInfo.pImageIndices = &currentBuffer;
	if (waitSemaphore != VK_NULL_HANDLE)
	{
		presentInfo.pWaitSemaphores = &waitSemaphore;
		presentInfo.waitSemaphoreCount = 1;
	}
	return fpQueuePresentKHR(queue, &presentInfo);
}

void SwapChain::ClearImages(VkCommandBuffer cmdbuf,uint32_t index)
{
	VkClearColorValue clearVal = { { 0.0f, 0.0f, 0.0f, 0.0f } };
	VkImageSubresourceRange sr;
	sr.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	sr.baseMipLevel = 0;
	sr.levelCount = 1;
	sr.baseArrayLayer = 0;
	sr.layerCount = 1;

	VKTools::SetImageLayout(
		cmdbuf,
		m_images[index],
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_GENERAL,
		sr);

	vkCmdClearColorImage(cmdbuf, m_images[index], VK_IMAGE_LAYOUT_GENERAL, &clearVal, 1, &sr);
}