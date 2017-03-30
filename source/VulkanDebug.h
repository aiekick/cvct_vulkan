#ifndef VULKANDEBUG_H
#define VULKANDEBUG_H

#include <vulkan.h>

namespace VKDebug
{
	// Default validation layers
	extern int validationLayerCount;
	extern const char *validationLayerNames[];

	// Default debug callback
	VkBool32 MessageCallback(
		VkDebugReportFlagsEXT flags,
		VkDebugReportObjectTypeEXT objType,
		uint64_t srcObject,
		size_t location,
		int32_t msgCode,
		const char* pLayerPrefix,
		const char* pMsg,
		void* pUserData);

	// Load debug function pointers and set debug callback
	// if callBack is NULL, default message callback will be used
	void SetupDebugging(
		VkInstance instance,
		VkDebugReportFlagsEXT flags,
		VkDebugReportCallbackEXT callBack);
	// Clear debug callback
	void freeDebugCallback(VkInstance instance);

	//SetupDebugMarkers
	// Set up the debug marker function pointers
	//void SetupDebugMarkers(VkDevice device);
}


#endif	//VULKANDEBUG_H