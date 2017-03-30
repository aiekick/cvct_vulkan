#ifndef SWAPCHAIN_H
#define SWAPCHAIN_H

#include <vulkan.h>
#include <string>
#include <vector>
#include "Defines.h"

#include <GLFW\glfw3.h>

// Macro to get a procedure address based on a vulkan instance
#define GET_INSTANCE_PROC_ADDR(inst, entrypoint)                        \
{                                                                       \
    fp##entrypoint = (PFN_vk##entrypoint) vkGetInstanceProcAddr(inst, "vk"#entrypoint); \
    if (fp##entrypoint == NULL)                                         \
	{																    \
        exit(1);                                                        \
    }                                                                   \
}

// Macro to get a procedure address based on a vulkan device
#define GET_DEVICE_PROC_ADDR(dev, entrypoint)                           \
{                                                                       \
    fp##entrypoint = (PFN_vk##entrypoint) vkGetDeviceProcAddr(dev, "vk"#entrypoint);   \
    if (fp##entrypoint == NULL)                                         \
	{																    \
        exit(1);                                                        \
    }                                                                   \
}

struct SwapChainBuffer 
{
	VkImage image;
	VkImageView view;
};

class SwapChain
{
public:
	SwapChain();
	~SwapChain();

	//initialize the swapchain
	void Initialize(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device);
	void InitializeSurface(GLFWwindow* window);
	void CreateSwapChain(VkCommandBuffer commandBuffer, uint32_t *width, uint32_t* height);
	VkResult AcquireNextImage(VkSemaphore presentCompleteSemaphore, uint32_t* currentBuffer);
	VkResult QueuePresent(VkQueue queue, uint32_t currentBuffer);
	VkResult QueuePresent(VkQueue queue, uint32_t currentBuffer, VkSemaphore waitSemaphore);
	std::string applicationName;
	void ClearImages(VkCommandBuffer cmdbuf, uint32_t index);
private:
	// Function pointers
	PFN_vkGetPhysicalDeviceSurfaceSupportKHR fpGetPhysicalDeviceSurfaceSupportKHR;
	PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR fpGetPhysicalDeviceSurfaceCapabilitiesKHR;
	PFN_vkGetPhysicalDeviceSurfaceFormatsKHR fpGetPhysicalDeviceSurfaceFormatsKHR;
	PFN_vkGetPhysicalDeviceSurfacePresentModesKHR fpGetPhysicalDeviceSurfacePresentModesKHR;
	PFN_vkCreateSwapchainKHR fpCreateSwapchainKHR;
	PFN_vkDestroySwapchainKHR fpDestroySwapchainKHR;
	PFN_vkGetSwapchainImagesKHR fpGetSwapchainImagesKHR;
	PFN_vkAcquireNextImageKHR fpAcquireNextImageKHR;
	PFN_vkQueuePresentKHR fpQueuePresentKHR;

	// Vulkan instance
	VkInstance m_instance;
	// Physical device aka used gpu
	VkPhysicalDevice m_physicalDevice;
	// The abstract view device layer
	VkDevice m_viewDevice;
	// The surface to use
	VkSurfaceKHR m_surface;
	// Color format used for the surfaces
	VkFormat m_colorFormat;
	// Colorspace of the surface
	VkColorSpaceKHR m_colorSpace;
	// Vulkan swapchain handle
	VkSwapchainKHR m_swapChain = VK_NULL_HANDLE;
public:
	// descriptors
	std::vector<VkDescriptorImageInfo> m_descriptors;
	VkSampler m_sampler;								//image sampler
	// Index queue with grahpics and presenting queue of the detected graphics device,
	uint32_t m_queueNodeIndex = UINT32_MAX;
	// Number of images in the swapchain
	uint32_t m_imageCount;
	// Images of the swapchain
	std::vector<VkImage> m_images;
	// The buffers of the images in the swapchain
	std::vector<SwapChainBuffer> m_buffers;
	//width, height of the framebuffers
	uint32_t m_width, m_height;
};

#endif	//SWAPCHAIN_H