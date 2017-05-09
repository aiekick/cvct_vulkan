#ifndef VULKANCORE_H
#define VULKANCORE_H

#include <string>
#include <vulkan.h>
#include <glm/glm.hpp>
#include "Defines.h"
#include "SwapChain.h"
#include "VCTPipelineDefines.h"
#include <GLFW\glfw3.h>

#define VERTEX_BUFFER_BIND_ID 0
#define VK_FLAGS_NONE 0

static void error_callback(int error, const char* description)
{
	fprintf(stderr, "Error %d: %s\n", error, description);
}

// Per device specific properties
struct DeviceQueueIndices
{
	uint32_t graphics;
	uint32_t compute;
	uint32_t transfer;
};
struct DeviceQueues
{
	VkQueue graphics;
	VkQueue compute;
	VkQueue transfer;
};
struct DevicePools
{
	VkCommandPool graphics;
	VkCommandPool compute;
	VkCommandPool transfer;
};
// Used to copy data from host to device memory
struct StagingBuffer 
{
	VkDeviceMemory memory;
	VkBuffer buffer;
};
// Device Information
struct DeviceInformation
{
	// Stores physical device properties (for e.g. checking device limits)
	VkPhysicalDeviceProperties deviceProperties;
	// Stores phyiscal device features (for e.g. checking if a feature is available)
	VkPhysicalDeviceFeatures deviceFeatures;
	// Stores all available memory (type) properties for the physical device
	VkPhysicalDeviceMemoryProperties deviceMemoryProperties;
	// Store the device limits
	VkPhysicalDeviceLimits deviceLimits;
};
// Semaphores within the grahpics render pipeline
struct Semaphores
{
	VkSemaphore presentComplete;
	VkSemaphore renderComplete;
	VkSemaphore textOverlayComplete;
};
// Depth stencil
struct DepthStencil
{
	VkImage image;
	VkDeviceMemory mem;
	VkImageView view;
};
class VulkanCore
{
public:
	VulkanCore(const char* appName, const char* consoleName, glm::uvec2& windowResolution);
	~VulkanCore();
	// Initializes Vulkan
	int32_t InitializeVulkan();
	// Creates a window
	GLFWwindow* InitializeGLFWWindow(bool fullscreen = false);
	// Creates a vulkan instance
	VkResult CreateVulKanInstance(bool enableValidation);
	// Creates semaphores for synchronizations
	void CreateSemaphores();
	// Create a vulkan view device 
	VkResult CreateVulkanDevice(bool enableValidation);
	// Initializes the swapchain
	void InitializeSwapchain();
	// Creates the command pool
	void CreateCommandPools();
	// Create the setup command buffer
	void CreateSetupCommandBuffer();
	// Create and setup depth stencil
	void CreateDepthStencil();
	// Get memory type for a given memory allocation ( flags and bits)
	uint32_t GetMemoryType(uint32_t typeBits, VkFlags properties);
	// Create a default renderpass
	void CreateRenderPass();
	// Create default graphics pipeline cache
	void CreatePipelineCache();
	// Create Framebuffer
	void CreateFrameBuffer();
	// Enable console
	void SetupConsole(std::string title);
	// The prepare function
	void Prepare();
	// Get the index within the queue
	uint32_t GetQueueFamiliyIndex(VkQueueFlagBits flag);
	// Get the Graphics Queue handle
	inline VkQueue GetGraphicsQueue() { return m_deviceQueues.graphics; }
	// Get the Graphics Queue handle
	inline VkQueue GetTransferQueue() { return m_deviceQueues.transfer; }
	// Get the Graphics Queue handle
	inline VkQueue GetComputeQueue() { return m_deviceQueues.compute; }
	// Get graphics command pool handle
	inline VkCommandPool GetGraphicsCommandPool() { return m_devicePools.graphics; }
	// Get compute command pool handle
	inline VkCommandPool GetComputeCommandPool() { return m_devicePools.compute; }
	// Get transfer command pool handle
	inline VkCommandPool GetTransferCommandPool() { return m_devicePools.transfer; }
	// Get the physical GPU
	inline VkPhysicalDevice GetPhysicalGPU() { return m_physicalGPU; }
	// Gett he view device
	inline VkDevice GetViewDevice() { return m_viewDevice; }
	// Get the glfw window
	inline GLFWwindow* GetGLFWWindow() { return m_glfwWindow; }
	// Get swapchain
	inline SwapChain* GetSwapChain() { return &m_swapChain; }
	// Get device info
	inline const DeviceInformation& GetDeviceInformation() { return m_grahpicsDeviceInfo; }
	// Get pipeline cache
	inline const VkPipelineCache GetPipelineCache() { return m_pipelineCache; }
	// Get device queue indices
	inline const DeviceQueueIndices& GetDeviceQueueIndices() { return m_deviceQueueIndices; }
	// Get framebuffers
	inline const VkFramebuffer* GetFramebuffers() { return m_frameBuffers.data(); }
	inline uint32_t GetFramebufferCount() { return (uint32_t)m_frameBuffers.size(); }
	// Get renderpass
	inline const VkRenderPass GetRenderpass() { return m_renderPass; }
	inline const glm::uvec2& GetScreenResolution() { return m_screenResolution; }

	VkFormat m_colorFormat = VK_FORMAT_B8G8R8A8_UNORM;			// Color framebuffer format
	VkFormat m_depthFormat;										// Depth framebuffer format
	DepthStencil m_depthStencil;								// Depthstencil framebuffer
protected:
	std::vector<VkDeviceQueueCreateInfo> m_queueCreateInfos;	// Queue create info 
	std::vector<VkQueueFamilyProperties> m_queueProps;			// List all the queue properties of the physical device
	DeviceQueues m_deviceQueues;								// Container of all the queues
	DevicePools m_devicePools;									// Container for all the device pools
	DeviceQueueIndices	m_deviceQueueIndices;					// Device queue indices
	bool m_enableValidation;									// Debug and validation layer 
	std::string m_applicationName;								// Name of the application
	VkInstance m_vulkanInstance;								// The vulkan instance
	VkPhysicalDevice m_physicalGPU;								// The used physical device	TODO: currently only use one, make use of multiple for compute rendering

	DeviceInformation m_grahpicsDeviceInfo;						// Graphics device info
	VkDevice m_viewDevice;										// Handle application device view abstraction
	SwapChain m_swapChain;										// The swapchain used for dubble buffering, and contains all the buffers
	Semaphores	m_semaphores;									// Semaphores used for synchronization
	VkSubmitInfo m_submitInfo;									// Contains command buffers and semaphores to be presented to the queue
	VkPipelineStageFlags m_submitPipelineStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; 	// Pipeline stage flags for the submit info structure
	//HINSTANCE m_windowInstance;									// The window instance
	//HWND m_window;												// Window
	GLFWwindow* m_glfwWindow;									// GLFW window
	bool m_fullscreen;											// Full screen toggle
	VkRenderPass m_renderPass;									// Global render pass for frame buffer writes
	VkPipelineCache m_pipelineCache;							// Default graphics pipeline cache
	std::vector<VkFramebuffer> m_frameBuffers;					// The framebuffers. Number is depended on the number in the swapchain
	VkPhysicalDeviceMemoryProperties m_deviceMemoryProperties;	// Stores all available memory (type) properties for the physical device
	// Command buffers
	VkCommandBuffer m_setupCommandBuffer = VK_NULL_HANDLE;		// Command buffer used for setup
	VkCommandBuffer m_postPresentCommandBuffer = VK_NULL_HANDLE;// Command buffer used for a post present image barrier
	VkCommandBuffer m_prePresentCommandBuffer = VK_NULL_HANDLE;	// Command buffer used for a pre present image barrier
	
	uint32_t m_currentFrameBuffer; // keep track of the current active framebuffer
	glm::uvec2 m_screenResolution;	//Screen resolution
};

#endif VULKANCORE