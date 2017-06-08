#include "VulkanCore.h"
#include <vector>
#include <assert.h>
#include "VulkanDebug.h"
#include "VKTools.h"
#include <array>


VulkanCore::VulkanCore(const char* appName, const char* consoleName, glm::uvec2& windowResolution)
{
	m_applicationName = appName;
#ifdef NDEBUG
	m_enableValidation = false;
#else
	m_enableValidation = true;
#endif
	
	m_screenResolution = windowResolution;

	InitializeVulkan();
	SetupConsole(consoleName);
}


VulkanCore::~VulkanCore()
{
}

void VulkanCore::Prepare()
{
	if(m_enableValidation)	//enable validation layers
		VKDebug::SetupDebugging(m_vulkanInstance, VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT, VK_NULL_HANDLE);

	CreateCommandPools();
	CreateSetupCommandBuffer();
	m_swapChain.CreateSwapChain(m_setupCommandBuffer, &m_screenResolution.x, &m_screenResolution.y);
	// Create the framebuffer
	CreateDepthStencil();
	CreateRenderPass();
	CreatePipelineCache();
	CreateFrameBuffer();
	// Flush the setup
	VKTools::FlushCommandBuffer(m_setupCommandBuffer, m_deviceQueues.graphics, m_viewDevice, m_devicePools.graphics, false);
}

uint32_t VulkanCore::GetQueueFamiliyIndex(VkQueueFlagBits flag)
{
	// Dedicated queue for compute
	// Try to find a queue family index that supports compute but not graphics
	if (flag & VK_QUEUE_COMPUTE_BIT)
	{
		for (uint32_t i = 0; i < static_cast<uint32_t>(m_queueProps.size()); i++)
		{
			if ((m_queueProps[i].queueFlags & flag) && ((m_queueProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0))
			{
				return i;
				break;
			}
		}
	}
	// Dedicated queue for transfer
	// Try to find a queue family index that supports transfer but not graphics and compute
	if (flag & VK_QUEUE_TRANSFER_BIT)
	{
		for (uint32_t i = 0; i < static_cast<uint32_t>(m_queueProps.size()); i++)
		{
			if ((m_queueProps[i].queueFlags & flag) && ((m_queueProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) && ((m_queueProps[i].queueFlags & VK_QUEUE_COMPUTE_BIT) == 0))
			{
				return i;
				break;
			}
		}
	}
	// For other queue types or if no separate compute queue is present, return the first one to support the requested flags
	for (uint32_t i = 0; i < static_cast<uint32_t>(m_queueProps.size()); i++)
	{
		if (m_queueProps[i].queueFlags & flag)
		{
			return i;
			break;
		}
	}
	
	return 0;
}

int VulkanCore::InitializeVulkan()
{
	CreateVulKanInstance(m_enableValidation);
	VkResult err;
	// Number of physical active GPUs
	uint32_t numGPU = 0;
	// Get number of GPU's in current system TODO: set a minimum of two, for computing
	err = vkEnumeratePhysicalDevices(m_vulkanInstance, &numGPU, nullptr);
	if(err != 0)
		RETURN_ERROR(-1, "Error while enumerating devices for count(0x%08X)", (int32_t)-1);

	if(!numGPU)	//assert if there no GPUs active
		RETURN_ERROR(-1, "No GPU active (0x%08X)", (int32_t)-1);

	//Enumerate devices
	std::vector<VkPhysicalDevice> physicalDevices(numGPU);		//reserve the number of array
	err = vkEnumeratePhysicalDevices(m_vulkanInstance, &numGPU, physicalDevices.data());	//receive data
	if(err)
		RETURN_ERROR(-1, "Error while enumerating devices for data (0x%08X)", (int32_t)-1);

	// TODO: Currently only use the first physical gpu in the list. Add support for second GPU
	// to use for compute calculations
	m_physicalGPU = physicalDevices[0];

	// Find a queue in the selected GPU to support graphics queue
	uint32_t queueCount;				//number of queues
	vkGetPhysicalDeviceQueueFamilyProperties(m_physicalGPU, &queueCount, NULL);	//get number of queues of physical device
	assert(queueCount >= 1);		//if there are less than one queue count
	
	m_queueProps.resize(queueCount);		//resize the array according to the number of queues supported by the GPU
	vkGetPhysicalDeviceQueueFamilyProperties(m_physicalGPU, &queueCount, m_queueProps.data());	//fill the list with queue properties

	VkQueueFlags requestedQueueTypes = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
	// Get queue family indices for the requested queue family types
	// Note that the indices may overlap depending on the implementation

	const float defaultQueuePriority(0.0f);

	// Graphics queue
	if (requestedQueueTypes & VK_QUEUE_GRAPHICS_BIT)
	{
		m_deviceQueueIndices.graphics = GetQueueFamiliyIndex(VK_QUEUE_GRAPHICS_BIT);
		VkDeviceQueueCreateInfo queueInfo{};
		queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueInfo.queueFamilyIndex = m_deviceQueueIndices.graphics;
		queueInfo.queueCount = 1;
		queueInfo.pQueuePriorities = &defaultQueuePriority;
		m_queueCreateInfos.push_back(queueInfo);
	}
	else
	{
		m_deviceQueueIndices.graphics = VK_NULL_HANDLE;
	}

	// Dedicated compute queue
	if (requestedQueueTypes & VK_QUEUE_COMPUTE_BIT)
	{
		m_deviceQueueIndices.compute = GetQueueFamiliyIndex(VK_QUEUE_COMPUTE_BIT);
		if (m_deviceQueueIndices.compute != m_deviceQueueIndices.graphics)
		{
			// If compute family index differs, we need an additional queue create info for the compute queue
			VkDeviceQueueCreateInfo queueInfo{};
			queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueInfo.queueFamilyIndex = m_deviceQueueIndices.compute;
			queueInfo.queueCount = 1;
			queueInfo.pQueuePriorities = &defaultQueuePriority;
			m_queueCreateInfos.push_back(queueInfo);
		}
	}
	else
	{
		// Else we use the same queue
		m_deviceQueueIndices.compute = m_deviceQueueIndices.graphics;
	}

	// Dedicated transfer queue
	if (requestedQueueTypes & VK_QUEUE_TRANSFER_BIT)
	{
		m_deviceQueueIndices.transfer = GetQueueFamiliyIndex(VK_QUEUE_TRANSFER_BIT);
		if ((m_deviceQueueIndices.transfer != m_deviceQueueIndices.graphics) && (m_deviceQueueIndices.transfer != m_deviceQueueIndices.compute))
		{
			// If compute family index differs, we need an additional queue create info for the compute queue
			VkDeviceQueueCreateInfo queueInfo{};
			queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueInfo.queueFamilyIndex = m_deviceQueueIndices.transfer;
			queueInfo.queueCount = 1;
			queueInfo.pQueuePriorities = &defaultQueuePriority;
			m_queueCreateInfos.push_back(queueInfo);
		}
	}
	else
	{
		// Else we use the same queue
		m_deviceQueueIndices.transfer = m_deviceQueueIndices.graphics;
	}

	assert(!err);
	// Store gpu device properties
	vkGetPhysicalDeviceProperties(m_physicalGPU, &m_grahpicsDeviceInfo.deviceProperties);
	// Store gpu device features
	vkGetPhysicalDeviceFeatures(m_physicalGPU, &m_grahpicsDeviceInfo.deviceFeatures);
	// Gather physical device memory properties
	vkGetPhysicalDeviceMemoryProperties(m_physicalGPU, &m_grahpicsDeviceInfo.deviceMemoryProperties);
	// Create the vulkan view device
	err = CreateVulkanDevice(m_enableValidation);
	// Get the graphics queue
	vkGetDeviceQueue(m_viewDevice, m_deviceQueueIndices.graphics, 0, &m_deviceQueues.graphics);
	// Get the transfer queue
	vkGetDeviceQueue(m_viewDevice, m_deviceQueueIndices.transfer, 0, &m_deviceQueues.transfer);
	// Get the compute queue
	vkGetDeviceQueue(m_viewDevice, m_deviceQueueIndices.compute, 0, &m_deviceQueues.compute);
	// Find a suitable depth format
	VkBool32 validDepthFormat = VKTools::GetSupportedDepthFormat(m_physicalGPU, &m_depthFormat);
	assert(validDepthFormat);
	// Initialize the swapchain
	m_swapChain.Initialize(m_vulkanInstance, m_physicalGPU, m_viewDevice);
	// Create semaphores for synchronization
	CreateSemaphores();
	// Set submit info TODO: read more into this
	m_submitInfo = VKTools::Initializers::SubmitInfo();
	m_submitInfo.pWaitDstStageMask = &m_submitPipelineStages;
	m_submitInfo.waitSemaphoreCount = 1;
	m_submitInfo.pWaitSemaphores = &m_semaphores.presentComplete;
	m_submitInfo.signalSemaphoreCount = 1;
	m_submitInfo.pSignalSemaphores = &m_semaphores.renderComplete;
	// Set framebuffer
	m_currentFrameBuffer = 0;

	return 0;
}

GLFWwindow* VulkanCore::InitializeGLFWWindow(bool fullscreen /*false*/)
{
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	m_glfwWindow = glfwCreateWindow(m_screenResolution.x, m_screenResolution.y, m_applicationName.c_str(), NULL, NULL);
	return m_glfwWindow;
}

VkResult VulkanCore::CreateVulKanInstance(bool enableValidation)
{
	// Enable validation layers
	this->m_enableValidation = enableValidation;

	// Set application info, used for instance
	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;		//this value needs to be set
	appInfo.pNext = nullptr;								//not using right now
	appInfo.pApplicationName = m_applicationName.c_str();	//set applicaiton name
	appInfo.pEngineName = m_applicationName.c_str();		//set engine name
	appInfo.apiVersion = VK_API_VERSION_1_0;				//set version

	std::vector<const char*> enabledExtensions;
	// Enable GLFW extension (surface extension)
	uint32_t glfw_extensions_count;
	const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extensions_count);
	for (uint32_t i = 0; i < glfw_extensions_count; i++)
		enabledExtensions.push_back(glfw_extensions[i]);

	// Set up the instance info
	VkInstanceCreateInfo instanceCreateInfo = {};
	instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;	// mandatory type
	instanceCreateInfo.pNext = NULL;		//mandatory value
	instanceCreateInfo.pApplicationInfo = &appInfo;
	// Enable extensions if any available
	if (enabledExtensions.size() > 0)
	{
		if (m_enableValidation)
		{
			enabledExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
		}
		instanceCreateInfo.enabledExtensionCount = (uint32_t)enabledExtensions.size();
		instanceCreateInfo.ppEnabledExtensionNames = enabledExtensions.data();
	}
	// Set the valication layer information
	if (m_enableValidation)
	{
		instanceCreateInfo.enabledLayerCount = VKDebug::validationLayerCount;
		instanceCreateInfo.ppEnabledLayerNames = VKDebug::validationLayerNames;
	}
	// Create the vulkan instance
	return vkCreateInstance(&instanceCreateInfo, nullptr, &m_vulkanInstance);
}

void VulkanCore::CreateSemaphores()
{
	// Create synchronization objects
	VkSemaphoreCreateInfo semaphoreCreateInfo = VKTools::Initializers::SemaphoreCreateInfo();
	VK_CHECK_RESULT(vkCreateSemaphore(m_viewDevice, &semaphoreCreateInfo, nullptr, &m_semaphores.presentComplete));
	VK_CHECK_RESULT(vkCreateSemaphore(m_viewDevice, &semaphoreCreateInfo, nullptr, &m_semaphores.renderComplete));
	VK_CHECK_RESULT(vkCreateSemaphore(m_viewDevice, &semaphoreCreateInfo, nullptr, &m_semaphores.textOverlayComplete));
}

VkResult VulkanCore::CreateVulkanDevice( bool enableValidation)
{
	// Create the logical device representation
	std::vector<const char*> deviceExtensions;
	//if (useSwapChain)	always use the swapchain
	{
		// If the device will be used for presenting to a display via a swapchain we need to request the swapchain extension
		deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
	}

	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(m_queueCreateInfos.size());
	deviceCreateInfo.pQueueCreateInfos = m_queueCreateInfos.data();
	deviceCreateInfo.pEnabledFeatures = &m_grahpicsDeviceInfo.deviceFeatures;

	if (deviceExtensions.size() > 0)
	{
		deviceCreateInfo.enabledExtensionCount = (uint32_t)deviceExtensions.size();
		deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
	}

	//enable swapchain extension for rendering and double buffering
	std::vector<const char*> enabledExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	//if extensions 
	if (enabledExtensions.size() > 0)
	{
		deviceCreateInfo.enabledExtensionCount = (uint32_t)enabledExtensions.size();
		deviceCreateInfo.ppEnabledExtensionNames = enabledExtensions.data();
	}
	if (enableValidation)
	{
		deviceCreateInfo.enabledLayerCount = VKDebug::validationLayerCount;
		deviceCreateInfo.ppEnabledLayerNames = VKDebug::validationLayerNames;
	}
	//create the vulkan view device
	return vkCreateDevice(m_physicalGPU, &deviceCreateInfo, nullptr, &m_viewDevice);
}

void VulkanCore::InitializeSwapchain()
{
	m_swapChain.InitializeSurface(m_glfwWindow);
}

void VulkanCore::CreateCommandPools()
{
	// Create the command pool
	VkCommandPoolCreateInfo cmdPoolInfo = {};
	cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	// Create the graphics command pool
	cmdPoolInfo.queueFamilyIndex = m_deviceQueueIndices.graphics;
	VK_CHECK_RESULT(vkCreateCommandPool(m_viewDevice, &cmdPoolInfo, nullptr, &m_devicePools.graphics));
	// Create the compute command pool
	cmdPoolInfo.queueFamilyIndex = m_deviceQueueIndices.compute;
	VK_CHECK_RESULT(vkCreateCommandPool(m_viewDevice, &cmdPoolInfo, nullptr, &m_devicePools.compute));
	// Create the transfer command pool
	cmdPoolInfo.queueFamilyIndex = m_deviceQueueIndices.transfer;
	VK_CHECK_RESULT(vkCreateCommandPool(m_viewDevice, &cmdPoolInfo, nullptr, &m_devicePools.transfer));
}


void VulkanCore::CreateSetupCommandBuffer( )
{
	//free the command buffer first
	if (m_setupCommandBuffer != VK_NULL_HANDLE)
	{
		vkFreeCommandBuffers(m_viewDevice, m_devicePools.graphics, 1, &m_setupCommandBuffer);
		m_setupCommandBuffer = VK_NULL_HANDLE;
	}
	//set up the commandbuffer allocate info 
	VkCommandBufferAllocateInfo cmdBufAllocateInfo = VKTools::Initializers::CommandBufferAllocateInfo(
		m_devicePools.graphics, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);
	
	//allocate the command buffers using the allocateinfo
	VK_CHECK_RESULT(vkAllocateCommandBuffers(m_viewDevice, &cmdBufAllocateInfo, &m_setupCommandBuffer));

	VkCommandBufferBeginInfo cmdBufInfo = {};
	cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	//begin recording for the setup command buffer
	VK_CHECK_RESULT(vkBeginCommandBuffer(m_setupCommandBuffer, &cmdBufInfo));
}

void VulkanCore::CreateDepthStencil()
{
	VkImageCreateInfo image = {};
	image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image.pNext = NULL;
	image.imageType = VK_IMAGE_TYPE_2D;
	image.format = m_depthFormat;
	image.extent = { m_screenResolution.x, m_screenResolution.y, 1 };
	image.mipLevels = 1;
	image.arrayLayers = 1;
	image.samples = VK_SAMPLE_COUNT_1_BIT;
	image.tiling = VK_IMAGE_TILING_OPTIMAL;
	image.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	image.flags = 0;

	VkMemoryAllocateInfo mem_alloc = {};
	mem_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	mem_alloc.pNext = NULL;
	mem_alloc.allocationSize = 0;
	mem_alloc.memoryTypeIndex = 0;

	VkImageViewCreateInfo depthStencilView = {};
	depthStencilView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	depthStencilView.pNext = NULL;
	depthStencilView.viewType = VK_IMAGE_VIEW_TYPE_2D;
	depthStencilView.format = m_depthFormat;
	depthStencilView.flags = 0;
	depthStencilView.subresourceRange = {};
	depthStencilView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	depthStencilView.subresourceRange.baseMipLevel = 0;
	depthStencilView.subresourceRange.levelCount = 1;
	depthStencilView.subresourceRange.baseArrayLayer = 0;
	depthStencilView.subresourceRange.layerCount = 1;

	VkMemoryRequirements memReqs;

	VK_CHECK_RESULT(vkCreateImage(m_viewDevice, &image, nullptr, &m_depthStencil.image));
	vkGetImageMemoryRequirements(m_viewDevice, m_depthStencil.image, &memReqs);
	mem_alloc.allocationSize = memReqs.size;
	mem_alloc.memoryTypeIndex = GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(m_viewDevice, &mem_alloc, nullptr, &m_depthStencil.mem));

	VK_CHECK_RESULT(vkBindImageMemory(m_viewDevice, m_depthStencil.image, m_depthStencil.mem, 0));

	depthStencilView.image = m_depthStencil.image;
	VK_CHECK_RESULT(vkCreateImageView(m_viewDevice, &depthStencilView, nullptr, &m_depthStencil.view));
}

uint32_t VulkanCore::GetMemoryType(uint32_t typeBits, VkFlags properties)
{
	for (uint32_t i = 0; i < 32; i++)
	{
		if ((typeBits & 1) == 1)
		{
			if ((m_grahpicsDeviceInfo.deviceMemoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
			{
				return i;
			}
		}
		typeBits >>= 1;
	}

	RETURN_ERROR(-1, "Memory type is not active in the properties (0x%08X)", (uint32_t)0);
	return 0;
}

void VulkanCore::CreateRenderPass()
{
	VkAttachmentDescription attachments[2] = {};

	// Color attachment
	attachments[0].format = m_colorFormat;
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;									// We don't use multi sampling in this example
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;							// Clear this attachment at the start of the render pass
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;							// Keep it's contents after the render pass is finished (for displaying it)
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;					// We don't use stencil, so don't care for load
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;				// Same for store
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;						// Layout at render pass start. Initial doesn't matter, so we use undefined
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;					// Layout to which the attachment is transitioned when the render pass is finished
																					// As we want to present the color buffer to the swapchain, we transition to PRESENT_KHR	
	attachments[1].format = m_depthFormat;
	attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;							// Clear depth at start of first subpass
	attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;						// We don't need depth after render pass has finished (DONT_CARE may result in better performance)
	attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;					// No stencil
	attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;				// No Stencil
	attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;						// Layout at render pass start. Initial doesn't matter, so we use undefined
	attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;	// Transition to depth/stencil attachment

	VkAttachmentReference colorReference = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
	VkAttachmentReference depthReference = {1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.flags = 0;
	subpass.inputAttachmentCount = 0;
	subpass.pInputAttachments = NULL;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorReference;
	subpass.pResolveAttachments = NULL;
	subpass.pDepthStencilAttachment = &depthReference;
	subpass.preserveAttachmentCount = 0;
	subpass.pPreserveAttachments = NULL;

	VkSubpassDependency dependencies[2];

	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.pNext = NULL;
	renderPassInfo.attachmentCount = 2;
	renderPassInfo.pAttachments = attachments;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 2;
	renderPassInfo.pDependencies = dependencies;

	VK_CHECK_RESULT(vkCreateRenderPass(m_viewDevice, &renderPassInfo, NULL, &m_renderPass));
}

void VulkanCore::CreatePipelineCache()
{
	// create a default pipelinecache
	VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
	pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
	VK_CHECK_RESULT(vkCreatePipelineCache(m_viewDevice, &pipelineCacheCreateInfo, nullptr, &m_pipelineCache));
}

void VulkanCore::CreateFrameBuffer()
{
	//two attatchment ( 0 for image view, 1 for depth stencil)
	VkImageView attachments[2];

	// Depth/Stencil attachment is the same for all frame buffers
	attachments[1] = m_depthStencil.view;
	//setup create info for framebuffers
	VkFramebufferCreateInfo frameBufferCreateInfo = {};
	frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	frameBufferCreateInfo.pNext = NULL;
	frameBufferCreateInfo.renderPass = m_renderPass;
	frameBufferCreateInfo.attachmentCount = 2;
	frameBufferCreateInfo.pAttachments = attachments;
	frameBufferCreateInfo.width = m_screenResolution.x;
	frameBufferCreateInfo.height = m_screenResolution.y;
	frameBufferCreateInfo.layers = 1;

	// Create frame buffers for every swap chain image
	m_frameBuffers.resize(m_swapChain.m_imageCount);
	for (uint32_t i = 0; i < m_frameBuffers.size(); i++)
	{
		//set the correct view per attatchment per swapchain
		attachments[0] = m_swapChain.m_buffers[i].view;
		//create
		VK_CHECK_RESULT(vkCreateFramebuffer(m_viewDevice, &frameBufferCreateInfo, NULL, &m_frameBuffers[i]));
	}
}

// Win32 : Sets up a console window and redirects standard output to it
void VulkanCore::SetupConsole(std::string title)
{
	AllocConsole();
	AttachConsole(GetCurrentProcessId());
	FILE *stream;
	freopen_s(&stream, "CONOUT$", "w+", stdout);
	SetConsoleTitle(TEXT(title.c_str()));
	if (m_enableValidation)
	{
		std::cout << "Validation enabled:\n";
	}
}
