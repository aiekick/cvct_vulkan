#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <vector>
#include <array>
#include <fcntl.h>
#include <io.h> 
#include <chrono>
#include <ctime>

#define GLM_FORCE_RADIANS

#pragma comment(linker, "/subsystem:windows")
#include <vulkan.h>
#include <imgui.h>

#include "Shader.h"
#include "VulkanCore.h"
#include "VKTools.h"
#include "AssetManager.h"
#include "Camera.h"
#include "VCTPipelineDefines.h"
#include "PipelineStates.h"
#include "AnisotropicVoxelTexture.h"
#include "imgui_impl_glfw_vulkan.h"

// glm
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>

///////////////////////////////////////////////////////
// Defines
/////////////////////////////////////////////////////// 
// Vulkan Defines
#define WIDTH_DEFAULT 1280
#define HEIGHT_DEFAULT 720
#define FRAME_BUFFER_COUNT 2
#define APPVERSION 0.95f
// Path defines
#define SPONZAPATH "Assets/sponza.ogex"
#define ASSETPATH "Assets/"
// Texture defines
#define MAXTEXTURES 256
#define MAXMESHES 512
// Cascade voxel grid defines
#define GRIDSIZE 128			// number of voxels per cascade
#define GRIDMIPMAP 3			// Number of mipmap per cascade
#define GRIDREGION 6.0f			// Base grid reigon
#define CASCADECOUNT 3			// Number of sascades
#define CONECOUNT 15			// Default number of cones
#define DEFERRED_DEFAULT 0			// Default deferred renderer
#define DEFERRED_SCALE 0.5f

// Forward declare
class CVCT;

///////////////////////////////////////////////////////
// Global Members
///////////////////////////////////////////////////////
// Members
CVCT* m_cvct;
AssetManager					m_assetManager;				// Asset manager
scene_s*						m_scene = NULL;				// Scene
vk_mesh_s						m_meshes[MAXMESHES];		// Vulkan mesh list
vk_texture_s					m_textures[MAXTEXTURES];	// Vulkan texture list
uint32_t						m_meshCount = 0;			// Mesh Counter
uint32_t						m_textureCount = 0;			// Texture Counter
AnisotropicVoxelTexture			m_avt;						// Grid instance containing the voxels in 3D texture
uint32_t						m_renderFlags = 0;			// Render flags
CVCTSettings					m_cvctSettings = {};		// Cascade settings
RenderStatesTimeStamps			m_timeStamps = {};			// state timestamps

// todo clean later
bool hideGUi = false;

// Renderstates
RenderState ImGUIState = {};				// ImGUI state
RenderState VoxelizerState = {};			// Voxelizer state
RenderState VoxelMipMapperState = {};		// Voxel mipmapper state
RenderState ForwardRendererState = {};		// Forward state
RenderState VoxelDebugState = {};			// Voxel debug state
RenderState ConeTraceState = {};			// Cone tracer state
RenderState PostVoxelizerState = {};		// Post voxelizer state
RenderState ForwardMainRenderState = {};	// Forward main renderer state
RenderState DeferredMainRenderState = {};	// Deferred main renderer state
VkCommandBuffer m_uploadCommandBuffer = {};
VkCommandBuffer m_clearCommandBuffer = {};

// Asset helper functions
void LoadAssetStaticManager(char* path, uint32_t pathLenght)
{
	m_assetManager.LoadAsset(path, pathLenght);
}
asset_s* GetAssetStaticManager(char* path)
{
	asset_s* asset = nullptr;
	m_assetManager.GetAsset(path, &asset);
	if (!asset) 
		printf("asset not loaded");
	return asset;
}

// predefine
extern void resize(GLFWwindow* window, int w, int h);

class CVCT : public VulkanCore
{
public:
	CVCT(const char* appName, const char* consoleName) : VulkanCore(appName, consoleName, glm::uvec2((uint32_t)WIDTH_DEFAULT, (uint32_t)HEIGHT_DEFAULT))
	{
		// Turn this on by default, always
		m_renderFlags |= RenderFlags::RENDER_VOXELIZE;
		// enable forward rendering by default
		m_renderFlags |= RenderFlags::RENDER_FORWARD;
		//m_renderFlags |= RenderFlags::RENDER_FORWARDMAIN;
		//m_renderFlags |= RenderFlags::RENDER_DEFERREDMAIN;
		// Set the default values
		m_cvctSettings.cascadeCount = CASCADECOUNT;
		m_cvctSettings.cascadeNum = 0;
		m_cvctSettings.gridRegion = GRIDREGION;
		m_cvctSettings.gridSize = GRIDSIZE;
		m_cvctSettings.deferredScale = DEFERRED_SCALE;
		m_cvctSettings.conecount = CONECOUNT;
		m_cvctSettings.deferredRender = DEFERRED_DEFAULT;
	}

private:
	// Descriptorset
	VkDescriptorSetLayout m_staticDescriptorSetLayout;
	VkDescriptorPool m_staticDescriptorPool;
	VkDescriptorPool m_imguiDescriptorPool;				// Descriptorpool specifically for imgui
	VkDescriptorSet m_staticDescriptorSet;
	VkSampler m_sampler;
	bool m_prepared = {};
	uint32_t m_destWidth = 0;
	uint32_t m_destHeight = 0;
	float m_deltaTime = 0;		// in seconds
	bool m_paused = {};
	glm::dvec2 m_mousePosition;
	glm::vec2 m_prevMousePosition;
	uint32_t m_currentBuffer = 0;
	Camera* m_camera;

public:
	// Cascade helper funcitons
	void VoxelizeCascade(uint32_t cascade)
	{
		m_cvctSettings.cascadeNum = cascade;
		UpdateUniformBuffers();
	}

	// Change voxelgrid size
	void ChangeVoxelGridSizeAndCascade(uint32_t size,uint32_t cascade)
	{
		m_cvctSettings.gridSize = size;
		m_cvctSettings.cascadeCount = cascade;
		UpdateUniformBuffers();
		// clear anisotropic voxel
		DestroyAnisotropicVoxelTexture(&m_avt, GetViewDevice());
		// rebuild voxel
		CreateAnisotropicVoxelTexture(&m_avt, m_cvctSettings.gridSize, m_cvctSettings.gridSize, m_cvctSettings.gridSize, VK_FORMAT_R8G8B8A8_UNORM, GRIDMIPMAP, m_cvctSettings.cascadeCount, m_physicalGPU, m_viewDevice, this);
		//destroy all influenced states
		DestroyRenderStates(ForwardMainRenderState, (VulkanCore*)this, GetGraphicsCommandPool());	// Forward main render
		DestroyRenderStates(DeferredMainRenderState, (VulkanCore*)this, GetGraphicsCommandPool());	// Deferred main render
		DestroyRenderStates(VoxelizerState, (VulkanCore*)this, GetGraphicsCommandPool());			// Voxelizer
		DestroyRenderStates(VoxelDebugState, (VulkanCore*)this, GetGraphicsCommandPool());			// Voxelizerpipelinestate
		DestroyRenderStates(VoxelMipMapperState, (VulkanCore*)this, GetComputeCommandPool());		// Mipmap
		DestroyRenderStates(PostVoxelizerState, (VulkanCore*)this, GetComputeCommandPool());		// Post voxelizer
		DestroyRenderStates(ConeTraceState, (VulkanCore*)this, GetComputeCommandPool());			// Cone trace
		//rebuild all states
		// Voxelizer pipeline state
		CreateVoxelizerState(
			VoxelizerState,
			(VulkanCore*)this,
			m_devicePools.graphics,
			&m_swapChain,
			m_staticDescriptorSet,
			&m_vertices,
			m_meshes,
			m_meshCount,
			m_staticDescriptorSetLayout,
			m_camera,
			&m_avt);
		// Post voxelizer state
		CreatePostVoxelizerState(
			PostVoxelizerState,
			(VulkanCore*)this,
			GetComputeCommandPool(),
			m_viewDevice,
			&m_swapChain,
			&m_avt);
		// Voxelizer mipmapper
		CreateMipMapperState(
			VoxelMipMapperState,
			(VulkanCore*)this,
			GetComputeCommandPool(),
			m_viewDevice,
			&m_swapChain,
			&m_avt);
		// Voxel renderer debug pipeline state
		CreateVoxelRenderDebugState(
			VoxelDebugState,
			m_frameBuffers.data(),
			m_swapChain.m_imageCount,
			(VulkanCore*)this,
			m_devicePools.graphics,
			&m_swapChain,
			m_staticDescriptorSet,
			m_staticDescriptorSetLayout,
			m_camera,
			&m_avt);
		// Cone tracer pipeline state
		CreateConeTraceState(
			ConeTraceState,
			m_swapChain.m_imageCount,
			(VulkanCore*)this,
			m_devicePools.compute,
			&m_swapChain,
			&m_avt);
		// Forward main renderer pipeline state
		CreateForwardMainRendererState(
			ForwardMainRenderState,
			m_frameBuffers.data(),
			m_swapChain.m_imageCount,
			(VulkanCore*)this,
			m_devicePools.graphics,
			m_viewDevice,
			&m_swapChain,
			m_staticDescriptorSet,
			&m_vertices,
			m_meshes,
			m_meshCount,
			m_renderPass,
			m_staticDescriptorSetLayout,
			&m_avt);
		// Deferred main renderer pipeline state
		CreateDeferredMainRenderState(
			DeferredMainRenderState,
			m_swapChain.m_imageCount,
			(VulkanCore*)this,
			GetGraphicsCommandPool(),
			&m_swapChain,
			m_staticDescriptorSet,
			&m_vertices,
			m_meshes,
			m_meshCount,
			&m_avt,
			m_staticDescriptorSetLayout,
			m_cvctSettings.deferredScale);
	}

	// Change grid region size
	// todo: need to check if this works
	void ChangeRegionSize(float regionsize)
	{
		m_cvctSettings.gridRegion = regionsize;
	}

	float GetTimeStamp(RenderState& renderstate,uint32_t start, uint32_t end, uint32_t first, uint32_t second)
	{
		if (renderstate.m_queryPool == VK_NULL_HANDLE) return (float)0;

		VkResult res = vkGetQueryPoolResults(
			GetViewDevice(),
			renderstate.m_queryPool,
			start,
			end,
			sizeof(uint64_t)*renderstate.m_queryCount,
			renderstate.m_queryResults,
			sizeof(uint64_t),
			VK_QUERY_RESULT_64_BIT);

		// Difference in nano-seconds
		float diff = (float)(renderstate.m_queryResults[second] - renderstate.m_queryResults[first]);
		// Convert to milliseconds
		diff /= 1000000;		
		return diff;
	}

	// Resize window
	void WindowResize(GLFWwindow* window, int width, int height)
	{
		// Set the new resolution
		m_screenResolution = glm::uvec2(width, height);
		CreateSetupCommandBuffer();
		m_swapChain.CreateSwapChain(m_setupCommandBuffer, &m_screenResolution.x, &m_screenResolution.y);
		
		// recreate the depth stencil
		vkDestroyImageView(GetViewDevice(), m_depthStencil.view, NULL);
		vkDestroyImage(GetViewDevice(), m_depthStencil.image, NULL);
		vkFreeMemory(GetViewDevice(),m_depthStencil.mem, NULL);
		CreateDepthStencil();

		// recreate framebuffer
		for (uint32_t i = 0; i < m_frameBuffers.size(); i++)
			vkDestroyFramebuffer(GetViewDevice(), m_frameBuffers[i], NULL);
		CreateFrameBuffer();
		VKTools::FlushCommandBuffer(m_setupCommandBuffer, m_deviceQueues.graphics, m_viewDevice, m_devicePools.graphics, false);

		// Destroy the command buffers
		DestroyCommandBuffer(ForwardRendererState, (VulkanCore*)this, GetGraphicsCommandPool());	// Forward
		DestroyCommandBuffer(VoxelDebugState, (VulkanCore*)this, GetGraphicsCommandPool());			// Voxeldebug
		DestroyCommandBuffer(ForwardMainRenderState, (VulkanCore*)this, GetGraphicsCommandPool());	// Forward main forward renderer
		// whole state got influened. Destroy and rebuild
		DestroyRenderStates(ConeTraceState, (VulkanCore*)this, GetComputeCommandPool());			// Cone Tracer
		DestroyRenderStates(DeferredMainRenderState, (VulkanCore*)this, GetGraphicsCommandPool());	// Deferred main forward renderer
		//Rebuild the command buffers
		BuildCommandBuffer(ForwardRendererState, GetGraphicsCommandPool(), (VulkanCore*)this, (uint32_t)m_frameBuffers.size(), m_frameBuffers.data());
		BuildCommandBuffer(VoxelDebugState, GetGraphicsCommandPool(), (VulkanCore*)this, (uint32_t)m_frameBuffers.size(), m_frameBuffers.data());
		BuildCommandBuffer(ForwardMainRenderState, GetGraphicsCommandPool(), (VulkanCore*)this, (uint32_t)m_frameBuffers.size(), m_frameBuffers.data());
		// Cone tracer pipeline state
		CreateConeTraceState(
			ConeTraceState,
			m_swapChain.m_imageCount,
			(VulkanCore*)this,
			m_devicePools.compute,
			&m_swapChain,
			&m_avt);
		CreateDeferredMainRenderState(
			DeferredMainRenderState,
			m_swapChain.m_imageCount,
			(VulkanCore*)this,
			GetGraphicsCommandPool(),
			&m_swapChain,
			m_staticDescriptorSet,
			&m_vertices,
			m_meshes,
			m_meshCount,
			&m_avt,
			m_staticDescriptorSetLayout,
			m_cvctSettings.deferredScale);

		vkDeviceWaitIdle(GetViewDevice());
	}

	void DrawImGUI()
	{
		ImGUIParameters* par = (ImGUIParameters*)ImGUIState.m_cmdBufferParameters;
		par->camera = m_camera;
		par->currentBuffer = m_currentBuffer;
		par->renderflags = &m_renderFlags;
		par->avt = &m_avt;
		par->appversion = APPVERSION;
		par->swapchain = &m_swapChain;
		par->screenres = &m_screenResolution;
		par->dt = m_deltaTime;
		par->settings = &m_cvctSettings;
		par->timeStamps = &m_timeStamps;

		BuildCommandBuffer(ImGUIState, GetGraphicsCommandPool(), (VulkanCore*)this, (uint32_t)m_frameBuffers.size(), m_frameBuffers.data());
	}

	void Draw()
	{
		m_submitInfo.commandBufferCount = 1;
		m_submitInfo.pWaitDstStageMask = &m_submitPipelineStages;
		VkPipelineStageFlags flag = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		VK_CHECK_RESULT(m_swapChain.AcquireNextImage(m_semaphores.presentComplete, &m_currentBuffer));
		m_submitInfo.pWaitSemaphores = &m_semaphores.presentComplete;
		
		// Clear the current frame and depth buffer
		VkCommandBufferBeginInfo cmdBufInfo = {};
		cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		
		// Clear the swapchain images textures
		VK_CHECK_RESULT(vkBeginCommandBuffer(m_clearCommandBuffer, &cmdBufInfo));
		m_swapChain.ClearImages(m_clearCommandBuffer, m_currentBuffer);
		VKTools::FlushCommandBuffer(m_clearCommandBuffer, m_deviceQueues.graphics, m_viewDevice, m_devicePools.graphics, false);
		// Clear the anisotroipc voxel texture
		VK_CHECK_RESULT(vkBeginCommandBuffer(m_clearCommandBuffer, &cmdBufInfo));
		ClearAnisotropicVoxelTexture(&m_avt, m_clearCommandBuffer);
		VKTools::FlushCommandBuffer(m_clearCommandBuffer, m_deviceQueues.graphics, m_viewDevice, m_devicePools.graphics, false);

		// Voxel building pass
		float accVoxelizer = 0;
		float accPostVoxelizer = 0;
		float accMipmapper = 0;
		if ((m_renderFlags & RenderFlags::RENDER_VOXELIZE))
		{
			for (uint32_t i = 0; i < m_cvctSettings.cascadeCount; i++)
			{
				VoxelizeCascade(i);

				// Voxelizer rendering
				m_submitInfo.pSignalSemaphores = &VoxelizerState.m_semaphores[i];
				m_submitInfo.pCommandBuffers = &VoxelizerState.m_commandBuffers[i];
				VK_CHECK_RESULT(vkQueueSubmit(m_deviceQueues.graphics, 1, &m_submitInfo, VK_NULL_HANDLE));
				m_submitInfo.pWaitSemaphores = &VoxelizerState.m_semaphores[i];

				// Post voxelizer
				m_submitInfo.pSignalSemaphores = &PostVoxelizerState.m_semaphores[i];
				m_submitInfo.pCommandBuffers = &PostVoxelizerState.m_commandBuffers[i];
				VK_CHECK_RESULT(vkQueueSubmit(m_deviceQueues.compute, 1, &m_submitInfo, VK_NULL_HANDLE));
				m_submitInfo.pWaitSemaphores = &PostVoxelizerState.m_semaphores[i];

				// Voxel mipmapping
				m_submitInfo.pSignalSemaphores = &VoxelMipMapperState.m_semaphores[i];
				m_submitInfo.pCommandBuffers = &VoxelMipMapperState.m_commandBuffers[i];
				VK_CHECK_RESULT(vkQueueSubmit(m_deviceQueues.compute, 1, &m_submitInfo, VK_NULL_HANDLE));
				m_submitInfo.pWaitSemaphores = &VoxelMipMapperState.m_semaphores[i];

				vkDeviceWaitIdle(m_viewDevice);

				accVoxelizer += GetTimeStamp(VoxelizerState, 0, 2, 0, 1);
				accPostVoxelizer += GetTimeStamp(PostVoxelizerState, 0, 2, 0, 1);
				accMipmapper += GetTimeStamp(VoxelMipMapperState, 0, 2, 0, 1);
			}
		}

		// Pick one of the renderers
		if((m_renderFlags & RenderFlags::RENDER_FORWARD))
		{
			//Scene rendering
			m_submitInfo.pSignalSemaphores = &ForwardRendererState.m_semaphores[0];
			m_submitInfo.pCommandBuffers = &ForwardRendererState.m_commandBuffers[m_currentBuffer];
			VK_CHECK_RESULT(vkQueueSubmit(m_deviceQueues.graphics, 1, &m_submitInfo, VK_NULL_HANDLE));
			m_submitInfo.pWaitSemaphores = &ForwardRendererState.m_semaphores[0];
		}
		else if ((m_renderFlags & RenderFlags::RENDER_CONETRACE))
		{
			// Voxelizer Debug rendering
			m_submitInfo.pSignalSemaphores = &ConeTraceState.m_semaphores[0];
			m_submitInfo.pCommandBuffers = &ConeTraceState.m_commandBuffers[m_currentBuffer];
			VK_CHECK_RESULT(vkQueueSubmit(m_deviceQueues.compute, 1, &m_submitInfo, VK_NULL_HANDLE));
			m_submitInfo.pWaitSemaphores = &ConeTraceState.m_semaphores[0];
		}
		else if ((m_renderFlags & RenderFlags::RENDER_FORWARDMAIN))
		{
			// Scene rendering
			m_submitInfo.pSignalSemaphores = &ForwardMainRenderState.m_semaphores[0];
			m_submitInfo.pCommandBuffers = &ForwardMainRenderState.m_commandBuffers[m_currentBuffer];
			VK_CHECK_RESULT(vkQueueSubmit(m_deviceQueues.graphics, 1, &m_submitInfo, VK_NULL_HANDLE));
			m_submitInfo.pWaitSemaphores = &ForwardMainRenderState.m_semaphores[0];
		}
		else if ((m_renderFlags & RenderFlags::RENDER_DEFERREDMAIN))
		{
			// Scene rendering
			m_submitInfo.pSignalSemaphores = &DeferredMainRenderState.m_semaphores[0];
			m_submitInfo.pCommandBuffers = &DeferredMainRenderState.m_commandBuffers[m_currentBuffer];
			VK_CHECK_RESULT(vkQueueSubmit(m_deviceQueues.graphics, 1, &m_submitInfo, VK_NULL_HANDLE));
			m_submitInfo.pWaitSemaphores = &DeferredMainRenderState.m_semaphores[0];
		}
		// nothing writing tot he framebuffer at all
		else
		{
			VkImageSubresourceRange srRange = {};
			srRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			srRange.baseMipLevel = 0;
			srRange.levelCount = 1;
			srRange.baseArrayLayer = 0;
			srRange.layerCount = 1;
			//Set image layout
			VK_CHECK_RESULT(vkBeginCommandBuffer(m_clearCommandBuffer, &cmdBufInfo));
			VKTools::SetImageLayout(
				m_clearCommandBuffer,
				m_swapChain.m_images[m_currentBuffer],
				VK_IMAGE_LAYOUT_GENERAL,
				VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
				srRange);
			VKTools::FlushCommandBuffer(m_clearCommandBuffer, m_deviceQueues.graphics, m_viewDevice, m_devicePools.graphics, false);
		}
	
		if ((m_renderFlags & RenderFlags::RENDER_VOXELDEBUG) && (m_renderFlags & RenderFlags::RENDER_FORWARD))
		{
			// Voxelizer Debug rendering
			m_submitInfo.pSignalSemaphores = &VoxelDebugState.m_semaphores[0];
			// offsets in commandbuffer. order: fb1[cc1,cc2,cc3],fb2[cc1,cc2,cc3] etc etc..
			m_submitInfo.pCommandBuffers = &VoxelDebugState.m_commandBuffers[m_currentBuffer * m_avt.m_cascadeCount];
			VK_CHECK_RESULT(vkQueueSubmit(m_deviceQueues.graphics, 1, &m_submitInfo, VK_NULL_HANDLE));
			m_submitInfo.pWaitSemaphores = &VoxelDebugState.m_semaphores[0];
		}

		// Imgui pass
		if(!hideGUi)
			DrawImGUI();

		//Query here in milliseconds
		float forwardTimestamp = 0;
		float conetracerTimestamp = 0;
		float forwardMainTimestamp = 0;
		float deferredMainTimestamp = 0;

		if (m_renderFlags & RenderFlags::RENDER_FORWARD) 
			forwardTimestamp = GetTimeStamp(ForwardRendererState, 0, 2, 0, 1);
		if (m_renderFlags & RenderFlags::RENDER_CONETRACE) 
			conetracerTimestamp = GetTimeStamp(ConeTraceState, 0, 2, 0, 1);
		if (m_renderFlags & RenderFlags::RENDER_FORWARDMAIN)
			forwardMainTimestamp  = GetTimeStamp(ForwardMainRenderState, 0, 2, 0, 1);
		if (m_renderFlags & RenderFlags::RENDER_DEFERREDMAIN)
			deferredMainTimestamp = GetTimeStamp(DeferredMainRenderState, 0, 2, 0, 1);

		m_timeStamps = RenderStatesTimeStamps{ accVoxelizer, accPostVoxelizer, accMipmapper, forwardTimestamp, conetracerTimestamp, forwardMainTimestamp, deferredMainTimestamp };
		
		// Wait for the last queue to finish and present it
		if (!m_renderFlags)
		{
			VK_CHECK_RESULT(m_swapChain.QueuePresent(m_deviceQueues.graphics, m_currentBuffer, m_semaphores.presentComplete));
		}
		else
		{
			VK_CHECK_RESULT(m_swapChain.QueuePresent(m_deviceQueues.graphics, m_currentBuffer, *m_submitInfo.pWaitSemaphores));
		}
	}

	void SwitchRenderer(RenderFlags renderer, bool enableVoxelization = true)
	{
		m_renderFlags = renderer;
		if(enableVoxelization)
			m_renderFlags |= RenderFlags::RENDER_VOXELIZE;
	}

	//todo:: clean this, ugly
	uint32_t gridChange = GRIDSIZE;
	uint32_t cascadeChange = CASCADECOUNT;
	void Render()
	{
		if (!m_prepared)
			return;

		//todo: ugly, temporary
		if (gridChange != m_cvctSettings.gridSize)
		{
			gridChange = m_cvctSettings.gridSize;
			ChangeVoxelGridSizeAndCascade(m_cvctSettings.gridSize, m_cvctSettings.cascadeCount);
		}
		if (cascadeChange != m_cvctSettings.cascadeCount)
		{
			cascadeChange = m_cvctSettings.cascadeCount;
			ChangeVoxelGridSizeAndCascade(m_cvctSettings.gridSize, m_cvctSettings.cascadeCount);
		}
			

		//UpdateUniformBuffers();
		Draw();
		vkDeviceWaitIdle(m_viewDevice);
	}

	void RenderLoop()
	{
		while (!glfwWindowShouldClose(m_glfwWindow) && m_prepared)
		{
			glfwPollEvents();
			HandleMessages();

			// Get current time
			float start = Ctime();
			// Render frame here
			Render();

			// Time, Deltatime
			float end = Ctime();
			float diff = end - start;
			m_deltaTime = (float)diff;
			float framerate = 1.0f / m_deltaTime;
			int asda = 0;
		}
	}

	void CreateStaticDescriptorSet()
	{
		///////////////////////////////////////////////////////
		///// Create descriptorset
		/////////////////////////////////////////////////////// 
		// Create the static descriptorset
		VkDescriptorSetAllocateInfo staticDescriptorAllocInfo = {};
		staticDescriptorAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		staticDescriptorAllocInfo.pNext = NULL;
		staticDescriptorAllocInfo.descriptorPool = m_staticDescriptorPool;
		staticDescriptorAllocInfo.descriptorSetCount = 1;
		staticDescriptorAllocInfo.pSetLayouts = &m_staticDescriptorSetLayout;
		//allocate the descriptorset with the pool
		VK_CHECK_RESULT(vkAllocateDescriptorSets(m_viewDevice, &staticDescriptorAllocInfo, &m_staticDescriptorSet));

		///////////////////////////////////////////////////////
		///// Set the uniform buffers
		/////////////////////////////////////////////////////// 
		VkWriteDescriptorSet writeDescriptorSet = {};
		// Binding 0 : Static descriptorset
		writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSet.pNext = NULL;
		writeDescriptorSet.dstSet = m_staticDescriptorSet;
		writeDescriptorSet.dstBinding = STATIC_DESCRIPTOR_BUFFER;
		writeDescriptorSet.dstArrayElement = 0;
		writeDescriptorSet.descriptorCount = 1;
		writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		writeDescriptorSet.pImageInfo = NULL;
		writeDescriptorSet.pBufferInfo = &m_uboData.descriptor;
		writeDescriptorSet.pTexelBufferView = NULL;
		// Update the static uniform
		vkUpdateDescriptorSets(m_viewDevice, 1, &writeDescriptorSet, 0, NULL);
	}

	void CreateStaticDescriptorPool()
	{
		///////////////////////////////////////////////////////
		///// Create Descriptor Pool
		/////////////////////////////////////////////////////// 
		// Static Descriptor Pool
		VkDescriptorPoolSize staticPoolSize[STATIC_DESCRIPTOR_COUNT];
		staticPoolSize[STATIC_DESCRIPTOR_BUFFER] = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC , 1};
		staticPoolSize[STATIC_DESCRIPTOR_SAMPLER] = { VK_DESCRIPTOR_TYPE_SAMPLER , 1 };
		staticPoolSize[STATIC_DESCRIPTOR_IMAGE] = { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE , MAXTEXTURES };
		VkDescriptorPoolCreateInfo staticDescriptorCreateInfo = {};
		staticDescriptorCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		staticDescriptorCreateInfo.pNext = NULL;
		staticDescriptorCreateInfo.flags = 0;
		staticDescriptorCreateInfo.maxSets = 1;
		staticDescriptorCreateInfo.poolSizeCount = STATIC_DESCRIPTOR_COUNT;
		staticDescriptorCreateInfo.pPoolSizes = staticPoolSize;
		VK_CHECK_RESULT(vkCreateDescriptorPool(m_viewDevice, &staticDescriptorCreateInfo, nullptr, &m_staticDescriptorPool));
	}

	void CreateStaticDescriptorSetLayout()
	{
		///////////////////////////////////////////////////////
		/////static and dynamic descriptors
		/////////////////////////////////////////////////////// 
		// Static descriptorset
		VkDescriptorSetLayoutBinding staticLayoutBinding[StaticDescriptorLayout::STATIC_DESCRIPTOR_COUNT];
		// Binding 0 : Uniform buffer (Vertex shader) ( MVP matrix uniforms )
		staticLayoutBinding[STATIC_DESCRIPTOR_BUFFER] =
		{ STATIC_DESCRIPTOR_BUFFER, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT, NULL };
		// Binding 1 : image samplers
		staticLayoutBinding[STATIC_DESCRIPTOR_SAMPLER] =
		{ STATIC_DESCRIPTOR_SAMPLER, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL };
		// Binding 2: image descriptor sampler
		staticLayoutBinding[STATIC_DESCRIPTOR_IMAGE] =
		{ STATIC_DESCRIPTOR_IMAGE, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, MAXTEXTURES, VK_SHADER_STAGE_FRAGMENT_BIT, NULL };

		VkDescriptorSetLayoutCreateInfo staticDescriptorLayout = VKTools::Initializers::DescriptorSetLayoutCreateInfo(0, StaticDescriptorLayout::STATIC_DESCRIPTOR_COUNT, staticLayoutBinding);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_viewDevice, &staticDescriptorLayout, NULL, &m_staticDescriptorSetLayout));
	}

	void CreateUniformBuffers()
	{
		VkMemoryRequirements memReqs;
		// Vertex shader uniform buffer block
		VkBufferCreateInfo bufferInfo = {};
		VkMemoryAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.pNext = NULL;
		allocInfo.allocationSize = 0;
		allocInfo.memoryTypeIndex = 0;

		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = sizeof(m_uboVS);
		bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

		// Create a new buffer
		VK_CHECK_RESULT(vkCreateBuffer(m_viewDevice, &bufferInfo, NULL, &m_uboData.buffer));
		// Get memory requirements including size, alignment and memory type 
		vkGetBufferMemoryRequirements(m_viewDevice, m_uboData.buffer, &memReqs);
		allocInfo.allocationSize = memReqs.size;
		// Get the memory type index that supports host visibile memory access
		// Most implementations offer multiple memory tpyes and selecting the 
		// correct one to allocate memory from is important
		allocInfo.memoryTypeIndex = GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
		// Allocate memory for the uniform buffer
		VK_CHECK_RESULT(vkAllocateMemory(m_viewDevice, &allocInfo, NULL, &(m_uboData.memory)));
		// Bind memory to buffer
		VK_CHECK_RESULT(vkBindBufferMemory(m_viewDevice, m_uboData.buffer, m_uboData.memory, 0));

		// Store information in the uniform's descriptor
		m_uboData.descriptor.buffer = m_uboData.buffer;
		m_uboData.descriptor.offset = 0;
		m_uboData.descriptor.range = sizeof(m_uboVS);
	}

	ConeTracerUBOComp coneTracerUBO;
	ForwardMainRendererUBOFrag forwardMainrendererUBO;
	DeferredMainRendererUBOFrag deferredMainRendererUBO;
	void UpdateUniformBuffers()
	{
		// Forward rendering members
		glm::vec3 translation = { 0,0,0 };
		float angle = 0;
		glm::vec3 rotation = { 1,1,1 };
		glm::vec3 scale = { 0.01f,0.01f,0.01f };
		// Correction matrix for perspective matrix for models being upside down
		glm::mat4 correction;
		correction[1][1] = -1;
		correction[2][2] = 0.5;
		correction[3][2] = 0.5;
		// Calculate the projection, view and model matrix
		m_uboVS.projectionMatrix = correction * glm::perspective(45.0f, (float)m_screenResolution.x / (float)m_screenResolution.y, 0.1f, 100.0f);
		m_uboVS.viewMatrix = m_camera->GetViewMatrix();
		m_uboVS.modelMatrix = glm::scale(scale);
		// Voxel based members
		float voxelBaseRegion = m_cvctSettings.gridRegion * (float)glm::pow(2, m_cvctSettings.cascadeNum);		// size of the voxel region of the first cascade
		glm::vec3 camPos = m_camera->GetPosition();
		glm::vec4 voxelRegionWorld = glm::vec4(camPos - glm::vec3(voxelBaseRegion / 2.0f), (voxelBaseRegion));
		float worldSize = voxelRegionWorld.w;
		float halfSize = worldSize / 2.0f;
		float voxelSize = worldSize / m_avt.m_width;
		glm::vec3 bMin = glm::vec3(voxelRegionWorld);
		glm::vec3 bMax = bMin + worldSize;
		glm::vec3 bMid = (bMin + bMax) / 2.0f;
		glm::mat4 orthoProjection = glm::ortho(-halfSize, halfSize, -halfSize, halfSize, 0.0f, worldSize);
		uint32_t gridResolution = m_avt.m_width;

		VoxelizerDebugUBOGeom ubo;
		ubo.gridResolution = gridResolution;
		ubo.voxelSize = voxelSize;
		ubo.mipmap = m_cvctSettings.currentMipMap;
		ubo.side = m_cvctSettings.currentSide;
		ubo.voxelRegionWorld = voxelRegionWorld;

		VoxelizerUBOGeom uboGeom;
		uboGeom.viewProjectionXY = (correction * orthoProjection) * glm::lookAt(glm::vec3(bMid.x, bMid.y, bMin.z), glm::vec3(bMid.x, bMid.y, bMax.z), glm::vec3(0, 1, 0));
		uboGeom.viewProjectionXZ = (correction * orthoProjection) * glm::lookAt(glm::vec3(bMid.x, bMin.y, bMid.z), glm::vec3(bMid.x, bMax.y, bMid.z), glm::vec3(1, 0, 0));
		uboGeom.viewProjectionYZ = (correction * orthoProjection) * glm::lookAt(glm::vec3(bMin.x, bMid.y, bMid.z), glm::vec3(bMax.x, bMid.y, bMid.z), glm::vec3(0, 0, 1));

		VoxelizerUBOFrag uboFrag;
		uboFrag.voxelRegionWorld = voxelRegionWorld;
		uboFrag.voxelResolution = m_avt.m_width;
		uboFrag.cascadeCount = m_avt.m_cascadeCount;

		VoxelMipMapperUBOComp voxelMipMapperUBO;
		voxelMipMapperUBO.srcMipLevel = 0;
		voxelMipMapperUBO.numMipLevels = 2;
		voxelMipMapperUBO.cascadeCount = m_avt.m_cascadeCount;
		voxelMipMapperUBO.texelSize = glm::vec3(1.0f / (m_avt.m_width*0.5f * NUM_DIRECTIONS), 1.0f / (m_avt.m_height*0.5f * m_avt.m_cascadeCount), 1.0/(m_avt.m_depth*0.5f));
		voxelMipMapperUBO.gridSize = (uint32_t)((float)m_avt.m_width*0.5f);

		coneTracerUBO.voxelRegionWorld[m_cvctSettings.cascadeNum] = voxelRegionWorld;
		coneTracerUBO.cameraPosition = m_camera->GetPosition();
		coneTracerUBO.fovy = 45.0f;
		coneTracerUBO.cameraLookAt = glm::normalize(m_camera->GetForwardVector());
		coneTracerUBO.aspect = (float)m_swapChain.m_width / m_swapChain.m_height;
		coneTracerUBO.cameraUp = glm::normalize(m_camera->GetUpVector());
		coneTracerUBO.cascadeCount = m_cvctSettings.cascadeCount;
		coneTracerUBO.screenres = glm::vec2(m_swapChain.m_width, m_swapChain.m_height);
		coneTracerUBO.voxelGridResolution = glm::vec3(m_avt.m_width, m_avt.m_height, m_avt.m_depth);
		
		forwardMainrendererUBO.voxelRegionWorld[m_cvctSettings.cascadeNum] = voxelRegionWorld;
		forwardMainrendererUBO.cameraPosition = m_camera->GetPosition();
		forwardMainrendererUBO.voxelGridResolution = m_avt.m_width;
		forwardMainrendererUBO.cascadeCount = m_cvctSettings.cascadeCount;

		deferredMainRendererUBO.voxelRegionWorld[m_cvctSettings.cascadeNum] = voxelRegionWorld;
		deferredMainRendererUBO.cameraPosition = m_camera->GetPosition();
		deferredMainRendererUBO.voxelGridResolution = m_avt.m_width;
		deferredMainRendererUBO.cascadeCount = m_cvctSettings.cascadeCount;
		deferredMainRendererUBO.scaledWidth = m_cvctSettings.deferredScale * m_screenResolution.x;
		deferredMainRendererUBO.scaledHeight = m_cvctSettings.deferredScale * m_screenResolution.y;
		deferredMainRendererUBO.conecount = m_cvctSettings.conecount;
		deferredMainRendererUBO.deferredRenderer = m_cvctSettings.deferredRender;

		// Mapping Forward renderer
		uint8_t *pData;
		VK_CHECK_RESULT(vkMapMemory(m_viewDevice, m_uboData.memory, 0, sizeof(m_uboVS), 0, (void**)&pData));
		memcpy(pData, &m_uboVS, sizeof(StaticUBO));
		vkUnmapMemory(m_viewDevice, m_uboData.memory);
		// Voxelizer
		// Geometry shader map data
		VK_CHECK_RESULT(vkMapMemory(m_viewDevice, VoxelizerState.m_uniformData[0].m_memory, 0, sizeof(VoxelizerUBOGeom), 0, (void**)&pData));
		memcpy(pData, &uboGeom, sizeof(VoxelizerUBOGeom));
		vkUnmapMemory(m_viewDevice, VoxelizerState.m_uniformData[0].m_memory);
		// Fragment shader map data
		VK_CHECK_RESULT(vkMapMemory(m_viewDevice, VoxelizerState.m_uniformData[1].m_memory, 0, sizeof(VoxelizerUBOFrag), 0, (void**)&pData));
		memcpy(pData, &uboFrag, sizeof(VoxelizerUBOFrag));
		vkUnmapMemory(m_viewDevice, VoxelizerState.m_uniformData[1].m_memory);
		// Voxel MipMapper
		// Compute shader
		VK_CHECK_RESULT(vkMapMemory(m_viewDevice, VoxelMipMapperState.m_uniformData[0].m_memory, 0, sizeof(VoxelMipMapperUBOComp), 0, (void**)&pData));
		memcpy(pData, &voxelMipMapperUBO, sizeof(VoxelMipMapperUBOComp));
		vkUnmapMemory(m_viewDevice, VoxelMipMapperState.m_uniformData[0].m_memory);
		// Mapping VoelizerDebug
		// Geometry shader map data
		VK_CHECK_RESULT(vkMapMemory(m_viewDevice, VoxelDebugState.m_uniformData[0].m_memory, 0, sizeof(VoxelizerDebugUBOGeom), 0, (void**)&pData));
		memcpy(pData, &ubo, sizeof(VoxelizerDebugUBOGeom));
		vkUnmapMemory(m_viewDevice, VoxelDebugState.m_uniformData[0].m_memory);
		// ConeTracer
		// Compute shader
		VK_CHECK_RESULT(vkMapMemory(m_viewDevice, ConeTraceState.m_uniformData[0].m_memory, 0, sizeof(ConeTracerUBOComp), 0, (void**)&pData));
		memcpy(pData, &coneTracerUBO, sizeof(ConeTracerUBOComp));
		vkUnmapMemory(m_viewDevice, ConeTraceState.m_uniformData[0].m_memory);
		// Main renderer
		// Fragment shader
		VK_CHECK_RESULT(vkMapMemory(m_viewDevice, ForwardMainRenderState.m_uniformData[0].m_memory, 0, sizeof(ForwardMainRendererUBOFrag), 0, (void**)&pData));
		memcpy(pData, &forwardMainrendererUBO, sizeof(ForwardMainRendererUBOFrag));
		vkUnmapMemory(m_viewDevice, ForwardMainRenderState.m_uniformData[0].m_memory);
		// Deferred renderer
		// Fragment shader
		VK_CHECK_RESULT(vkMapMemory(m_viewDevice, DeferredMainRenderState.m_uniformData[0].m_memory, 0, sizeof(DeferredMainRendererUBOFrag), 0, (void**)&pData));
		memcpy(pData, &deferredMainRendererUBO, sizeof(DeferredMainRendererUBOFrag));
		vkUnmapMemory(m_viewDevice, DeferredMainRenderState.m_uniformData[0].m_memory);
	}

	
	void HandleMessages()
	{
		if (glfwGetKey(m_glfwWindow, GLFW_KEY_P) == GLFW_PRESS)
		{
			hideGUi = !hideGUi;
		}
			
		// GLFW window termination
		if (glfwGetKey(m_glfwWindow, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwDestroyWindow(m_glfwWindow), glfwTerminate(), m_prepared = false;
		// Camera movement properties
		float camspeed = 1;
		glm::vec3 cameraVelocity = {};
		// Camera translation
		if (glfwGetKey(m_glfwWindow, GLFW_KEY_W) == GLFW_PRESS) cameraVelocity.z -= m_deltaTime * camspeed;
		if (glfwGetKey(m_glfwWindow, GLFW_KEY_A) == GLFW_PRESS) cameraVelocity.x -= m_deltaTime * camspeed;
		if (glfwGetKey(m_glfwWindow, GLFW_KEY_S) == GLFW_PRESS) cameraVelocity.z += m_deltaTime * camspeed;
		if (glfwGetKey(m_glfwWindow, GLFW_KEY_D) == GLFW_PRESS) cameraVelocity.x += m_deltaTime * camspeed;
		if (glfwGetKey(m_glfwWindow, GLFW_KEY_Q) == GLFW_PRESS) cameraVelocity.y -= m_deltaTime * camspeed;
		if (glfwGetKey(m_glfwWindow, GLFW_KEY_E) == GLFW_PRESS) cameraVelocity.y += m_deltaTime * camspeed;
		m_camera->Translate(cameraVelocity);
		// Camera rotation
		if (glfwGetMouseButton(m_glfwWindow, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS && !ImGui::IsMouseHoveringAnyWindow())
		{
			glm::dvec2 mousePos = {};
			glfwGetCursorPos(m_glfwWindow, &mousePos.x, &mousePos.y);
			glm::dvec2 mouseDelta = mousePos - m_mousePosition;
			glm::quat rotX = glm::angleAxis<double>(glm::radians(mouseDelta.y) * 0.5f, glm::dvec3(-1, 0, 0));
			glm::quat rotY = glm::angleAxis<double>(glm::radians(mouseDelta.x) * 0.5f, glm::dvec3(0, -1, 0));
			glm::quat rotation = m_camera->GetRotation() * rotX;
			rotation = rotY*rotation;
			m_camera->SetRotation(rotation);
		}
		glfwGetCursorPos(m_glfwWindow, &m_mousePosition.x, &m_mousePosition.y);
		// Mipmap controlls
		if (glfwGetKey(m_glfwWindow, GLFW_KEY_1) == GLFW_PRESS) m_cvctSettings.currentMipMap = 0;
		if (glfwGetKey(m_glfwWindow, GLFW_KEY_2) == GLFW_PRESS) m_cvctSettings.currentMipMap = 1;
		if (glfwGetKey(m_glfwWindow, GLFW_KEY_3) == GLFW_PRESS) m_cvctSettings.currentMipMap = 2;
		// Direction controll
		if (glfwGetKey(m_glfwWindow, GLFW_KEY_UP) == GLFW_PRESS)	m_cvctSettings.currentSide = VoxelDirections::POSY;
		if (glfwGetKey(m_glfwWindow, GLFW_KEY_LEFT) == GLFW_PRESS)	m_cvctSettings.currentSide = VoxelDirections::NEGX;
		if (glfwGetKey(m_glfwWindow, GLFW_KEY_DOWN) == GLFW_PRESS)	m_cvctSettings.currentSide = VoxelDirections::NEGY;
		if (glfwGetKey(m_glfwWindow, GLFW_KEY_RIGHT) == GLFW_PRESS) m_cvctSettings.currentSide = VoxelDirections::POSY;
		if (glfwGetKey(m_glfwWindow, GLFW_KEY_DOWN) == GLFW_PRESS && 
			glfwGetKey(m_glfwWindow, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) m_cvctSettings.currentSide = VoxelDirections::POSZ;
		if (glfwGetKey(m_glfwWindow, GLFW_KEY_UP) == GLFW_PRESS && 
			glfwGetKey(m_glfwWindow, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) m_cvctSettings.currentSide = VoxelDirections::NEGZ;
	}

	inline void SetScene(scene_s* scene){m_scene = scene;}
	inline void SetScene(asset_s* asset)
	{
		scene_s* scene = (scene_s*)asset->data;
		SetScene(scene);
	}

	uint32_t CreateScene()
	{
		VkResult result;
		uint64_t totalBufferSize = m_scene->vertexDataSizeInBytes + m_scene->indexDataSizeInBytes;

		///////////////////////////////////////////////////////
		/////Create Vulkan specific buffers
		/////////////////////////////////////////////////////// 
		// Create the device specific and staging buffer
		VkBufferCreateInfo bufferInfo = {};
		VkBuffer sceneBuffer, sceneStageBuffer;
		VkMemoryRequirements bufferMemoryRequirements, stagingMemoryRequirements;

		// Create the stagingd
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.pNext = NULL;
		bufferInfo.size = totalBufferSize;
		bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		bufferInfo.flags = 0;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		result = vkCreateBuffer(m_viewDevice, &bufferInfo, NULL, &sceneStageBuffer);

		// Create the device buffer
		bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
		result = vkCreateBuffer(m_viewDevice, &bufferInfo, NULL, &sceneBuffer);

		// Set memory requirements
		vkGetBufferMemoryRequirements(m_viewDevice, sceneBuffer, &bufferMemoryRequirements);
		vkGetBufferMemoryRequirements(m_viewDevice, sceneStageBuffer, &stagingMemoryRequirements);
		uint32_t bufferTypeIndex = GetMemoryType(bufferMemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		uint32_t stagingTypeIndex = GetMemoryType(stagingMemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

		if (!bufferTypeIndex || !stagingTypeIndex)
			RETURN_ERROR(-1, "No compatible memory type");

		// Creating the memory
		VkDeviceMemory bufferDeviceMemory, stagingDeviceMemory;
		VkMemoryAllocateInfo allocInfo;

		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.pNext = NULL;
		allocInfo.allocationSize = bufferMemoryRequirements.size;
		allocInfo.memoryTypeIndex = bufferTypeIndex;
		result = vkAllocateMemory(m_viewDevice, &allocInfo, NULL, &bufferDeviceMemory);
		if (result != VkResult::VK_SUCCESS)
			RETURN_ERROR(-1, "not able to allocate buffer device memory");

		allocInfo.allocationSize = stagingMemoryRequirements.size;
		allocInfo.memoryTypeIndex = stagingTypeIndex;
		result = vkAllocateMemory(m_viewDevice,&allocInfo,NULL,&stagingDeviceMemory);
		if (result != VkResult::VK_SUCCESS)
			RETURN_ERROR(-1, "not able to allocate staging device memory");

		// Bind all the memory
		result = vkBindBufferMemory(m_viewDevice, sceneBuffer, bufferDeviceMemory,0);
		if (result != VkResult::VK_SUCCESS)
			RETURN_ERROR(-1, "Not able to bind buffer with device memory");

		result = vkBindBufferMemory(m_viewDevice, sceneStageBuffer, stagingDeviceMemory, 0);
		if (result != VkResult::VK_SUCCESS)
			RETURN_ERROR(-1, "Not able to bind staging with device memory");

		// Map memory dest to pointer
		uint8_t* dst = NULL;
		result = vkMapMemory(m_viewDevice, stagingDeviceMemory, 0, totalBufferSize, 0, (void**)&dst);

		if (result != VK_SUCCESS)
			RETURN_ERROR(-1, "unable to map destination pointer");

		//copy the vertex and indices data to the destination
		memcpy(dst, m_scene->vertexData, m_scene->vertexDataSizeInBytes);
		memcpy(dst + m_scene->vertexDataSizeInBytes, m_scene->indexData, m_scene->indexDataSizeInBytes);
		vkUnmapMemory(m_viewDevice, stagingDeviceMemory);

		///////////////////////////////////////////////////////
		/////create the vulkan meshes
		/////////////////////////////////////////////////////// 
		//set up all the meshes
		//vk_mesh_s* mesh = m_meshes;
		m_meshCount = m_scene->meshCount;
		uint32_t meshNum = 0;
		for (uint32_t r = 0; r < m_scene->modelReferenceCount; r++)
		{
			model_ref_s* modelRef = &m_scene->modelRefs[r];
			model_s* model = &m_scene->models[modelRef->modelIndex];
			for (uint32_t m = 0; m < model->meshCount; m++)
			{
				vk_mesh_s meshData = { 0 };
				mesh_s* mesh = &m_scene->meshes[model->meshStartIndex + m];

				//calculate/assign texture ID to meshes
				for (uint32_t k = 0; k < modelRef->materialIndexCount; k++)
				{
					material_s* material = &m_scene->materials[modelRef->materialIndices[k]];
					for (uint32_t t = 0; t < material->textureReferenceCount; t++)
					{
						texture_ref_s* textureRef = &m_scene->textureRefs[material->textureReferenceStart + t];
						const char* attrib = m_scene->stringData + textureRef->attribOffset;

						uint32_t flag = 0;
						if (strcmp(attrib, "diffuse") == 0)
							flag |= 1, meshData.submeshes[k].textureIndex[DIFFUSE_TEXTURE] = material->textureReferenceStart + t;
						else if(strcmp(attrib, "normal") == 0)
							flag |= 2, meshData.submeshes[k].textureIndex[NORMAL_TEXTURE] = material->textureReferenceStart + t;
						else if (strcmp(attrib, "opacity") == 0)
							flag |= 4, meshData.submeshes[k].textureIndex[OPACITY_TEXTURE] = material->textureReferenceStart + t;
					}
				}

				//assign vertex id to meshes
				if (mesh->vertexBufferCount > 8)
					RETURN_ERROR(-1, "Number of vertex buffers is too high");
				if (mesh->indexBufferCount > 4)
					RETURN_ERROR(-1, "Numer of submeshes is too high");

				meshData.vbvCount = mesh->vertexBufferCount;
				meshData.submeshCount = mesh->indexBufferCount;
				meshData.vertexCount = mesh->vertexCount;

				for (uint32_t j = 0; j < mesh->vertexBufferCount; j++)
				{
					vertex_buffer_s* vb = &m_scene->vertexBuffers[mesh->vertexBufferStartIndex + j];
					uint32_t idx = 0xFFFFFFF;
					const char* attrib = m_scene->stringData + vb->attribStringOffset;

					if (strcmp(attrib, "position") == 0)
						idx = ATTRIBUTE_POSITION;
					else if (strcmp(attrib, "texcoord") == 0)
						idx = ATTRIBUTE_TEXCOORD;
					else if (strcmp(attrib, "tangent") == 0)
						idx = ATTRIBUTE_TANGENT;
					else if (strcmp(attrib, "bitangent") == 0)
						idx = ATTRIBUTE_BITANGENT;
					else if (strcmp(attrib, "normal") == 0)
						idx = ATTRIBUTE_NORMAL;

					if (idx != 0xFFFFFFFF)
					{
						meshData.vertexResources[idx] = sceneBuffer;
						meshData.vertexOffsets[idx] = vb->vertexOffset;
						meshData.vertexStrides[idx] = vb->totalSize / vb->vertexCount;
					}
					else
						RETURN_ERROR(-1, "Attribute is not defined");
				}

				//set the current index buffers ( AKA submeshes )
				for (uint32_t j = 0; j < mesh->indexBufferCount; j++)
				{
					//get the current indexbuffer ( submesh index )
					index_buffer_s* ib = &m_scene->indexBuffers[mesh->indexBufferStartIndex + j];

					vk_ib_s vkib;
					vkib.buffer = sceneBuffer;
					vkib.format = (ib->indexByteSize == 4) ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16;
					vkib.offset = m_scene->vertexDataSizeInBytes + ib->indexOffset;
					vkib.count = ib->indexCount;

					meshData.submeshes[j].ibv = vkib;
				}
				m_meshes[meshNum++] = meshData;
			}
		}
			
		///////////////////////////////////////////////////////
		/////stage buffer to the GPU
		/////////////////////////////////////////////////////// 
		// Vertex buffer+Indices buffer
		VkBufferCopy bufferCopy = {0,0, totalBufferSize };
		vkCmdCopyBuffer(m_uploadCommandBuffer,sceneStageBuffer,sceneBuffer,1,&bufferCopy);

		///////////////////////////////////////////////////////
		/////Set bindings
		/////////////////////////////////////////////////////// 
		// Set binding description
		m_vertices.bindingDescriptions.resize(ATTRIBUTE_COUNT);
		// Location 0 : Position
		m_vertices.bindingDescriptions[0].binding = (uint32_t)ATTRIBUTE_POSITION;
		m_vertices.bindingDescriptions[0].stride = 3 * sizeof(float);
		m_vertices.bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		// Location 1 : Position
		m_vertices.bindingDescriptions[1].binding = (uint32_t)ATTRIBUTE_TEXCOORD;
		m_vertices.bindingDescriptions[1].stride = 2 * sizeof(float);
		m_vertices.bindingDescriptions[1].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		// Location 2 : Position
		m_vertices.bindingDescriptions[2].binding = (uint32_t)ATTRIBUTE_NORMAL;
		m_vertices.bindingDescriptions[2].stride = 3 * sizeof(float);
		m_vertices.bindingDescriptions[2].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		// Location 3 : Position
		m_vertices.bindingDescriptions[3].binding = (uint32_t)ATTRIBUTE_TANGENT;
		m_vertices.bindingDescriptions[3].stride = 3 * sizeof(float);
		m_vertices.bindingDescriptions[3].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		// Location 4 : Position
		m_vertices.bindingDescriptions[4].binding = (uint32_t)ATTRIBUTE_BITANGENT;
		m_vertices.bindingDescriptions[4].stride = 3 * sizeof(float);
		m_vertices.bindingDescriptions[4].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		// Attribute descriptions
		// Describes memory layout and shader attribute locations
		m_vertices.attributeDescriptions.resize(ATTRIBUTE_COUNT);
		// Location 0 : Position
		m_vertices.attributeDescriptions[0].binding = (uint32_t)ATTRIBUTE_POSITION;
		m_vertices.attributeDescriptions[0].location = ATTRIBUTE_POSITION;
		m_vertices.attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		m_vertices.attributeDescriptions[0].offset = 0;
		// Location 1 : Texcoord
		m_vertices.attributeDescriptions[1].binding = (uint32_t)ATTRIBUTE_TEXCOORD;
		m_vertices.attributeDescriptions[1].location = ATTRIBUTE_TEXCOORD;
		m_vertices.attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
		m_vertices.attributeDescriptions[1].offset = 0;
		//location 2 : Normal
		m_vertices.attributeDescriptions[2].binding = (uint32_t)ATTRIBUTE_NORMAL;
		m_vertices.attributeDescriptions[2].location = ATTRIBUTE_NORMAL;
		m_vertices.attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
		m_vertices.attributeDescriptions[2].offset = 0;
		//location 3 : Tangent
		m_vertices.attributeDescriptions[3].binding = (uint32_t)ATTRIBUTE_TANGENT;
		m_vertices.attributeDescriptions[3].location = ATTRIBUTE_TANGENT;
		m_vertices.attributeDescriptions[3].format = VK_FORMAT_R32G32B32_SFLOAT;
		m_vertices.attributeDescriptions[3].offset = 0;
		//location 4 : Bitangent
		m_vertices.attributeDescriptions[4].binding = (uint32_t)ATTRIBUTE_BITANGENT;
		m_vertices.attributeDescriptions[4].location = ATTRIBUTE_BITANGENT;
		m_vertices.attributeDescriptions[4].format = VK_FORMAT_R32G32B32_SFLOAT;
		m_vertices.attributeDescriptions[4].offset = 0;

		// Assign to vertex input state
		m_vertices.inputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		m_vertices.inputState.pNext = NULL;
		m_vertices.inputState.flags = VK_FLAGS_NONE;
		m_vertices.inputState.vertexBindingDescriptionCount = (uint32_t)m_vertices.bindingDescriptions.size();
		m_vertices.inputState.pVertexBindingDescriptions = m_vertices.bindingDescriptions.data();
		m_vertices.inputState.vertexAttributeDescriptionCount = (uint32_t)m_vertices.attributeDescriptions.size();
		m_vertices.inputState.pVertexAttributeDescriptions = m_vertices.attributeDescriptions.data();
		
		m_vertices.buf = sceneBuffer;
		m_vertices.mem = bufferDeviceMemory;

		m_indices.buf = sceneBuffer;
		m_indices.mem = bufferDeviceMemory;

		// Destroy staging buffers
		//TODO: Delete the staging buffer
		//vkDestroyBuffer(m_viewDevice, sceneStageBuffer, nullptr);
		//vkFreeMemory(m_viewDevice, stagingDeviceMemory, nullptr);

		return 0;	//everything is uploaded
	}

	uint32_t CreateTexture(uint32_t index, const char* path)
	{
		if (index >= MAXTEXTURES)
			RETURN_ERROR(-1, "Number of textures exceed limit");
		
		VkResult result;
		asset_s* image;
		image = GetAssetStaticManager((char*)path);

		image_desc_s* imageDesc = (image_desc_s*)image->data;
		if (!imageDesc)
			RETURN_ERROR(-1, "Image could not find between the descriptors");

		uint8_t* pixelData = (uint8_t*)(imageDesc->mips + imageDesc->mipCount);

		uint64_t pixelSize = 0;
		for (uint32_t i = 0; i < imageDesc->mipCount; i++)
			pixelSize += imageDesc->mips[i].width * imageDesc->mips[i].height * sizeof(uint32_t);	//size of one pixel is 4 bytes

		//////////////////////////////////////////////////
		//// Set the VK Texture
		///////////////////////////////////////////////// 
		vk_texture_s* vktexture = &m_textures[index];
		vktexture->width = imageDesc->width;
		vktexture->height = imageDesc->height;
		vktexture->mipCount = (uint32_t)floor(log2(std::max(imageDesc->width, imageDesc->height))) + 1;
		vktexture->descriptorSetCount = (uint32_t)ceil((float)(vktexture->mipCount-1)/4);
		vktexture->sampler = m_sampler;
		// TODO: THIS MIGHT ERROR
		vktexture->imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		vktexture->view = (VkImageView*)malloc(sizeof(VkImageView) * vktexture->mipCount);
		vktexture->descriptor = (VkDescriptorImageInfo*)malloc(sizeof(VkDescriptorImageInfo) * vktexture->mipCount);

		//////////////////////////////////////////////////
		////Create buffer resources
		/////////////////////////////////////////////////
		VkBuffer stagingBuffer;

		VkBufferCreateInfo bufferInfo = {};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.pNext = NULL;
		bufferInfo.flags = 0;
		bufferInfo.size = pixelSize;
		bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		// create the staging buffer
		result = vkCreateBuffer(m_viewDevice, &bufferInfo, NULL, &stagingBuffer);

		VkImageCreateInfo imageInfo = {};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.pNext = NULL;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
		//imageInfo.mipLevels = imageDesc->mipCount;
		imageInfo.mipLevels = vktexture->mipCount;
		imageInfo.flags = 0;
		imageInfo.arrayLayers = 1;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		VkExtent3D extent;
		extent.width = imageDesc->width;
		extent.height = imageDesc->height;
		extent.depth = 1;
		imageInfo.extent = extent;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
		//create the image
		result = vkCreateImage(m_viewDevice, &imageInfo, NULL, &vktexture->image);
		if (result != VkResult::VK_SUCCESS)
			RETURN_ERROR(-1, "vkCreateImage failed (0x%08X)", (uint32_t)result);

		VkMemoryRequirements imageMemoryRequirements, stagingMemoryRequirements;
		vkGetImageMemoryRequirements(m_viewDevice, vktexture->image, &imageMemoryRequirements);
		vkGetBufferMemoryRequirements(m_viewDevice, stagingBuffer, &stagingMemoryRequirements);
		uint32_t imageTypeIndex = GetMemoryType(imageMemoryRequirements.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		uint32_t stagingTypeIndex = GetMemoryType(stagingMemoryRequirements.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

		if(!imageTypeIndex || !stagingTypeIndex)
			RETURN_ERROR(-1, "No compatible memory type found for image");

		//////////////////////////////////////////////////
		//// Allocate memory
		/////////////////////////////////////////////////
		VkDeviceMemory imageDeviceMemory, stagingDeviceMemory;
		VkMemoryAllocateInfo allocateInfo = {};

		allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocateInfo.pNext = NULL;
		allocateInfo.allocationSize = imageMemoryRequirements.size;
		allocateInfo.memoryTypeIndex = imageTypeIndex;
		result = vkAllocateMemory(m_viewDevice, &allocateInfo,NULL,&imageDeviceMemory);
		if (result != VK_SUCCESS)
			RETURN_ERROR(-1, "vkAllocateMemory failed (0x%08X)", (uint32_t)result);

		allocateInfo.allocationSize = stagingMemoryRequirements.size;
		allocateInfo.memoryTypeIndex = stagingTypeIndex;
		result = vkAllocateMemory(m_viewDevice, &allocateInfo, NULL, &stagingDeviceMemory);
		if(result != VK_SUCCESS)
			RETURN_ERROR(-1, "vkAllocateMemory failed (0x%08X)", (uint32_t)result);

		//////////////////////////////////////////////////
		//// Bind memory
		/////////////////////////////////////////////////
		result = vkBindImageMemory(m_viewDevice, vktexture->image, imageDeviceMemory, 0);
		if (result != VK_SUCCESS)	RETURN_ERROR(-1, "vkBindMemory failed(0x%08X)", (uint32_t)result);

		result = vkBindBufferMemory(m_viewDevice, stagingBuffer, stagingDeviceMemory, 0);
		if(result != VK_SUCCESS)	RETURN_ERROR(-1, "vkBindMemory failed(0x%08X)", (uint32_t)result);

		//////////////////////////////////////////////////
		//// Map memory/copy memory
		/////////////////////////////////////////////////
		uint8_t* dst = NULL;
		result = vkMapMemory(m_viewDevice,stagingDeviceMemory,0,pixelSize,0,(void**)&dst);
		memcpy(dst, pixelData, pixelSize);
		vkUnmapMemory(m_viewDevice, stagingDeviceMemory);

		//////////////////////////////////////////////////
		//// copy buffer to image
		/////////////////////////////////////////////////
#define MAX_MIPS 16
		VkBufferImageCopy mipCopies[MAX_MIPS];
		if (imageDesc->mipCount >= MAX_MIPS)
			RETURN_ERROR(-1, "Number of mips is too high");
#undef MAX_MIPS
		// Set the mip maps
		for (uint32_t i = 0; i < imageDesc->mipCount; i++)
		{
			VkImageSubresourceLayers imgSubResource = {};
			imgSubResource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imgSubResource.mipLevel = i;
			imgSubResource.layerCount = 1;

			VkBufferImageCopy imgCopy;
			imgCopy.bufferOffset = imageDesc->mips[i].offset;
			imgCopy.bufferRowLength = imageDesc->mips[i].width;
			imgCopy.bufferImageHeight = imageDesc->mips[i].height;
			imgCopy.imageOffset = { 0,0,0 };
			imgCopy.imageSubresource = imgSubResource;
			imgCopy.imageExtent = { imageDesc->mips[i].width, imageDesc->mips[i].height, 1 };
			mipCopies[i] = imgCopy;
		}

		//////////////////////////////////////////////////
		//// Set up pipeline memory barriers
		/////////////////////////////////////////////////
		// Optimal image will be used as destination for the copy
		VkImageSubresourceRange srRange = {};
		srRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		srRange.levelCount = imageDesc->mipCount;
		srRange.layerCount = 1;

		VKTools::SetImageLayout(
			m_uploadCommandBuffer,
			vktexture->image,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			srRange);

		vkCmdCopyBufferToImage(m_uploadCommandBuffer, stagingBuffer, vktexture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, imageDesc->mipCount, mipCopies);

		// Change texture image layout to shader read after all mip levels have been copied
		VKTools::SetImageLayout(
			m_uploadCommandBuffer,
			vktexture->image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_GENERAL,
			srRange);

		//////////////////////////////////////////////////
		//// Create Image View
		/////////////////////////////////////////////////
		VkImageView imageView = {};
		VkImageViewCreateInfo viewCreateInfo = {};
		viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewCreateInfo.pNext = NULL;
		viewCreateInfo.flags = 0;
		viewCreateInfo.image = vktexture->image;
		viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
		viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewCreateInfo.subresourceRange.baseArrayLayer = 0;
		viewCreateInfo.subresourceRange.layerCount = 1;
		// create the imageview
		for (uint32_t i = 0; i < vktexture->mipCount; i++)
		{
			viewCreateInfo.subresourceRange.baseMipLevel = i;
			viewCreateInfo.subresourceRange.levelCount = vktexture->mipCount - i;
			result = vkCreateImageView(m_viewDevice, &viewCreateInfo, NULL, &vktexture->view[i]);
			if (result != VK_SUCCESS)
				RETURN_ERROR(-1, "vkcreateImageView Failed (0x%08X)", (uint32_t)result);

			//////////////////////////////////////////////////
			//// Update the descriptorset. bind image
			/////////////////////////////////////////////////
			vktexture->descriptor[i].sampler = vktexture->sampler;
			vktexture->descriptor[i].imageView = vktexture->view[i];
			vktexture->descriptor[i].imageLayout = vktexture->imageLayout;
		}

		// Create the uniform buffers per descriptorsetCount
		vktexture->ubo = (TextureMipMapperUBOComp*)malloc(sizeof(TextureMipMapperUBOComp) * vktexture->descriptorSetCount);
		vktexture->uboDescriptor = (UniformData*)malloc(sizeof(UniformData) * vktexture->descriptorSetCount);
		uint32_t mipcount = vktexture->mipCount - 1;
		int32_t mipRemainder = mipcount;
		for (uint32_t i = 0; i < vktexture->descriptorSetCount; i++)
		{
			// Create the uniform data
			uint32_t base = mipcount - mipRemainder;
			TextureMipMapperUBOComp ubo;
			ubo.srcMipLevel = base;
			ubo.numMipLevels = glm::min(4, mipRemainder);
			float msize = (float)(glm::max(vktexture->width, vktexture->height) * glm::pow(0.5f, base+1));
			ubo.texelSize = glm::vec2(1.0f / msize);
			vktexture->ubo[i] = ubo;

			mipRemainder -= 4;
			glm::abs(mipRemainder);

			// Create the descriptor data
			VKTools::CreateBuffer((VulkanCore*)this, m_viewDevice,
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				sizeof(TextureMipMapperUBOComp),
				NULL,
				&vktexture->uboDescriptor[i].m_buffer,
				&vktexture->uboDescriptor[i].m_memory,
				&vktexture->uboDescriptor[i].m_descriptor);

			// Mapping Forward renderer
			uint8_t *pData;
			VK_CHECK_RESULT(vkMapMemory(m_viewDevice, vktexture->uboDescriptor[i].m_memory, 0, sizeof(TextureMipMapperUBOComp), 0, (void**)&pData));
			memcpy(pData, &ubo, sizeof(TextureMipMapperUBOComp));
			vkUnmapMemory(m_viewDevice, vktexture->uboDescriptor[i].m_memory);
		}

		VkWriteDescriptorSet wds = {};
		wds.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		wds.pNext = NULL;
		wds.dstSet = m_staticDescriptorSet;
		wds.dstBinding = STATIC_DESCRIPTOR_IMAGE;
		wds.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		wds.descriptorCount = 1;
		wds.dstArrayElement = index;
		wds.pImageInfo = &vktexture->descriptor[0];
		//update the descriptorset
		vkUpdateDescriptorSets(m_viewDevice, 1, &wds, 0, NULL);

		return 0;
	}

	uint32_t CreateSamplers()
	{
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
			RETURN_ERROR(-1, "Failed to create sampler");

		// Create the texture sampler
		VkWriteDescriptorSet ds = {};
		VkDescriptorImageInfo dii = {};
		dii.sampler = m_sampler;
		ds.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		ds.dstSet = m_staticDescriptorSet;
		ds.dstBinding = STATIC_DESCRIPTOR_SAMPLER;
		ds.dstArrayElement = 0;
		ds.descriptorCount = 1;
		ds.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
		ds.pImageInfo = &dii;

		vkUpdateDescriptorSets(m_viewDevice, 1, &ds, 0, NULL);	 

		return 0;
	}
	 
	uint32_t CreateCommandBuffer()
	{
		m_uploadCommandBuffer = VKTools::Initializers::CreateCommandBuffer(m_devicePools.graphics, m_viewDevice, VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		m_clearCommandBuffer = VKTools::Initializers::CreateCommandBuffer(m_devicePools.graphics, m_viewDevice, VK_COMMAND_BUFFER_LEVEL_PRIMARY, false);
		return 0;
	}

	uint32_t UploadData()
	{
		VK_CHECK_RESULT(vkEndCommandBuffer(m_uploadCommandBuffer));

		// Submit copies to the queue
		VkSubmitInfo copySubmitInfo = {};
		copySubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		copySubmitInfo.pNext = NULL;
		VkPipelineStageFlags pipestageFlags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		copySubmitInfo.pWaitDstStageMask = &pipestageFlags;
		copySubmitInfo.commandBufferCount = 1;
		copySubmitInfo.pCommandBuffers = &m_uploadCommandBuffer;
		copySubmitInfo.signalSemaphoreCount = 0;
		copySubmitInfo.pSignalSemaphores = NULL;

		VK_CHECK_RESULT(vkQueueSubmit(m_deviceQueues.graphics, 1, &copySubmitInfo, VK_NULL_HANDLE));
		VK_CHECK_RESULT(vkQueueWaitIdle(m_deviceQueues.graphics));

		vkFreeCommandBuffers(m_viewDevice, m_devicePools.graphics, 1, &m_uploadCommandBuffer);
		return 0;
	}

	uint32_t LoadTextures()
	{
		if (!m_scene)	RETURN_ERROR(-1, "Textures trying to load before scene is assigned");

		m_textureCount = m_scene->textureCount;
		int32_t high = -1;		//TODO: UGLY, FIX LATER
		// Calculate/assign texture ID to meshes
		for (uint32_t r = 0; r < m_scene->modelReferenceCount; r++)
		{
			model_ref_s* modelRef = &m_scene->modelRefs[r];
			for (uint32_t k = 0; k < modelRef->materialIndexCount; k++)
			{
				material_s* material = &m_scene->materials[modelRef->materialIndices[k]];
				for (uint32_t t = 0; t < material->textureReferenceCount; t++)
				{
					texture_ref_s* textureRef = &m_scene->textureRefs[material->textureReferenceStart + t];
					texture_s* texture = &m_scene->textures[textureRef->textureIndex];
					
					char totallPath[512];
					const char* path = m_scene->stringData + texture->pathOffset;
					strncpy(totallPath, ASSETPATH, sizeof(totallPath));
					strncat(totallPath, path, sizeof(totallPath));
					
					if (int32_t(material->textureReferenceStart + t) > high)
					{
						high = material->textureReferenceStart + t;
						CreateTexture(material->textureReferenceStart + t, totallPath);
					}
				}
			}
		}

		return 0;
	}

	uint32_t BuildCommandMip()
	{
		uint32_t totalDescriptorSetCount = 0;
		for (uint32_t i = 0; i < m_textureCount; i++)
			totalDescriptorSetCount += m_textures[i].descriptorSetCount;

		////////////////////////////////////////////////////////////////////////////////
		// Create commandbuffers and semaphores
		////////////////////////////////////////////////////////////////////////////////
		// Create the semaphore
		VkSemaphore mipSemaphore;
		VkSemaphoreCreateInfo semInfo = VKTools::Initializers::SemaphoreCreateInfo();
		vkCreateSemaphore(m_viewDevice, &semInfo, NULL, &mipSemaphore);
		// Create the command buffer
		VkCommandBuffer mipCommandBuffer = VKTools::Initializers::CreateCommandBuffer(GetComputeCommandPool(), m_viewDevice, VK_COMMAND_BUFFER_LEVEL_PRIMARY, false);
		// Uniform data
		UniformData uniformData;
		VKTools::CreateBuffer((VulkanCore*)this, m_viewDevice,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			sizeof(TextureMipMapperUBOComp),
			NULL,
			&uniformData.m_buffer,
			&uniformData.m_memory,
			&uniformData.m_descriptor);

		////////////////////////////////////////////////////////////////////////////////
		// Set the descriptorset layout
		////////////////////////////////////////////////////////////////////////////////
		// Dynamic descriptorset
		VkDescriptorSetLayoutBinding layoutBinding[3];
		// Binding 0 : Diffuse texture sampled image
		layoutBinding[0] =
		{ 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL };
		layoutBinding[1] =
		{ 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 4, VK_SHADER_STAGE_COMPUTE_BIT, NULL };
		layoutBinding[2] =
		{ 2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL };
		// Create the descriptorlayout
		VkDescriptorSetLayout layout;
		VkDescriptorSetLayoutCreateInfo descriptorLayoutCreateInfo = VKTools::Initializers::DescriptorSetLayoutCreateInfo(0, 3, layoutBinding);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_viewDevice, &descriptorLayoutCreateInfo, NULL, &layout));

		////////////////////////////////////////////////////////////////////////////////
		// Create pipeline layout
		////////////////////////////////////////////////////////////////////////////////
		VkPipelineLayout pipelineLayout;
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = VKTools::Initializers::PipelineLayoutCreateInfo(0, 1, &layout);
		VK_CHECK_RESULT(vkCreatePipelineLayout(m_viewDevice, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));

		////////////////////////////////////////////////////////////////////////////////
		// Create descriptor pool
		////////////////////////////////////////////////////////////////////////////////
		VkDescriptorPool descriptorPool;
		VkDescriptorPoolSize poolSize[3];
		poolSize[0] = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 * totalDescriptorSetCount };
		poolSize[1] = { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 4 * totalDescriptorSetCount };
		poolSize[2] = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 * totalDescriptorSetCount };
		VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = VKTools::Initializers::DescriptorPoolCreateInfo(0, totalDescriptorSetCount, 3, poolSize);
		//create the descriptorPool
		VK_CHECK_RESULT(vkCreateDescriptorPool(m_viewDevice, &descriptorPoolCreateInfo, NULL, &descriptorPool));

		////////////////////////////////////////////////////////////////////////////////
		// Create the descriptor set
		////////////////////////////////////////////////////////////////////////////////
		VkDescriptorSet descriptorSet[MAXTEXTURES * 10];
		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = VKTools::Initializers::DescriptorSetAllocateInfo(descriptorPool, 1, &layout);
		for (uint32_t i = 0; i < totalDescriptorSetCount; i++)
		{
			//allocate the descriptorset with the pool
			VK_CHECK_RESULT(vkAllocateDescriptorSets(m_viewDevice, &descriptorSetAllocateInfo, &descriptorSet[i]));
			//int j = 0;
		}

		///////////////////////////////////////////////////////
		///// Create the compute pipeline
		/////////////////////////////////////////////////////// 
		// Create pipeline		
		VkPipeline mipPipeline;
		VkComputePipelineCreateInfo computePipelineCreateInfo = VKTools::Initializers::ComputePipelineCreateInfo(pipelineLayout, VK_FLAGS_NONE);
		// Shaders are loaded from the SPIR-V format, which can be generated from glsl
		Shader shaderStage;
		shaderStage = VKTools::LoadShader("shaders/texturemipmapper.comp.spv", "main", m_viewDevice, VK_SHADER_STAGE_COMPUTE_BIT);
		computePipelineCreateInfo.stage = shaderStage.m_shaderStage;

		VK_CHECK_RESULT(vkCreateComputePipelines(m_viewDevice, m_pipelineCache, 1, &computePipelineCreateInfo, nullptr, &mipPipeline));

		////////////////////////////////////////////////////////////////////////////////
		// Build command buffers
		////////////////////////////////////////////////////////////////////////////////
		VkCommandBufferBeginInfo cmdBufInfo = {};
		cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		cmdBufInfo.pNext = NULL;

		VK_CHECK_RESULT(vkBeginCommandBuffer(mipCommandBuffer, &cmdBufInfo));
		vkCmdBindPipeline(mipCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, mipPipeline);

		uint32_t desCount = 0;
		for (uint32_t i = 0; i < m_textureCount; i++)
		{
			vk_texture_s* tex = &m_textures[i];
			uint32_t mipcount = tex->mipCount - 1;
			int32_t mipRemainder = mipcount;
			for (uint32_t j = 0; j < tex->descriptorSetCount; j++)
			{
				uint32_t base = mipcount - mipRemainder;
				VkDescriptorImageInfo ii[4];
				for (uint32_t b = 0; b < 4; b++)
				{
					if (b < (uint32_t)glm::min(4, mipRemainder))
						ii[b] = tex->descriptor[base + 1 + b];
					else
						ii[b] = tex->descriptor[base + 1];
				}

				VkWriteDescriptorSet wds = {};
				// Bind the read texture
				wds.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				wds.pNext = NULL;
				wds.dstSet = descriptorSet[desCount];
				wds.dstBinding = 0;
				wds.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				wds.descriptorCount = 1;
				wds.dstArrayElement = 0;
				wds.pImageInfo = &tex->descriptor[base];
				vkUpdateDescriptorSets(m_viewDevice, 1, &wds, 0, NULL);
				// Bind the mipmaps
				wds.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				wds.pNext = NULL;
				wds.dstSet = descriptorSet[desCount];
				wds.dstBinding = 1;
				wds.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
				wds.descriptorCount = 4;//glm::min(4, mipRemainder);
				wds.dstArrayElement = 0;
				wds.pImageInfo = ii;
				vkUpdateDescriptorSets(m_viewDevice, 1, &wds, 0, NULL);
				// Bind the mipmaps
				wds.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				wds.pNext = NULL;
				wds.dstSet = descriptorSet[desCount];
				wds.dstBinding = 2;
				wds.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				wds.descriptorCount = 1;
				wds.dstArrayElement = 0;
				wds.pImageInfo = NULL;
				wds.pBufferInfo = &tex->uboDescriptor[j].m_descriptor;
				wds.pTexelBufferView = NULL;
				vkUpdateDescriptorSets(m_viewDevice, 1, &wds, 0, NULL);

				vkCmdBindDescriptorSets(mipCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet[desCount], 0, 0);

				float numdis = (float)glm::ceil((glm::max(tex->width, tex->height) * glm::pow(0.5f, tex->ubo[j].srcMipLevel+1)) / 8);
				vkCmdDispatch(mipCommandBuffer, (uint32_t)numdis, (uint32_t)numdis, 1);

				// Set for the next loop
				mipRemainder -= 4;
				glm::abs(mipRemainder);
				desCount++;
			}
		}
		vkEndCommandBuffer(mipCommandBuffer);

		// Submit it
		VkSubmitInfo submit;
		VkPipelineStageFlags flag = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		submit = VKTools::Initializers::SubmitInfo();
		submit.pCommandBuffers = &mipCommandBuffer;
		submit.commandBufferCount = 1;

		VK_CHECK_RESULT(vkQueueSubmit(m_deviceQueues.compute, 1, &submit, VK_NULL_HANDLE));
		vkDeviceWaitIdle(m_viewDevice);

		return 0;
	}

	void Prepare()
	{
		// glfw settings
		glfwSetFramebufferSizeCallback(m_glfwWindow, resize);

		// Initialize vulkan specific features
		VulkanCore::Prepare();

		// Initialize the camera
		m_camera = new Camera(m_screenResolution.x, m_screenResolution.y);
		// Create the static descriptorSets
		CreateUniformBuffers();
		CreateStaticDescriptorSetLayout();
		CreateStaticDescriptorPool();
		CreateStaticDescriptorSet();
		//setup the upload buffer
		CreateCommandBuffer();
		//create image samplers
		CreateSamplers();
		//load all the data
		SetScene(GetAssetStaticManager(SPONZAPATH));	// Create the scene
		CreateScene();									// Create the scene meshes
		LoadTextures();									// Loads all the scene textures
		
		// Create the Anisotropic voxel texture
		CreateAnisotropicVoxelTexture(&m_avt, m_cvctSettings.gridSize, m_cvctSettings.gridSize, m_cvctSettings.gridSize, VK_FORMAT_R8G8B8A8_UNORM, GRIDMIPMAP, m_cvctSettings.cascadeCount,m_physicalGPU, m_viewDevice, this);
		//upload all the data
		UploadData();		
		BuildCommandMip();

		// Initialize the Render Pipeline States
		// ImGUI pipeline state
		CreateImgGUIPipelineState(
			ImGUIState,
			m_swapChain.m_imageCount,
			(VulkanCore*)this,
			GetGraphicsCommandPool(),
			&m_swapChain);
		// Forward renderer pipeline state
		CreateForwardRenderState(
			ForwardRendererState,
			m_frameBuffers.data(),
			m_swapChain.m_imageCount,
			(VulkanCore*)this,
			m_devicePools.graphics,
			&m_swapChain,
			m_staticDescriptorSet,
			&m_vertices,
			m_meshes,
			m_meshCount,
			m_renderPass,
			m_staticDescriptorSetLayout);
		// Voxelizer pipeline state
		CreateVoxelizerState(
			VoxelizerState,
			(VulkanCore*)this,
			m_devicePools.graphics,
			&m_swapChain,
			m_staticDescriptorSet,
			&m_vertices,
			m_meshes,
			m_meshCount,
			m_staticDescriptorSetLayout,
			m_camera,
			&m_avt);
		// Post voxelizer state
		CreatePostVoxelizerState(
			PostVoxelizerState,
			(VulkanCore*)this,
			GetComputeCommandPool(),
			m_viewDevice,
			&m_swapChain,
			&m_avt);
		// Voxelizer mipmapper
		CreateMipMapperState(
			VoxelMipMapperState,
			(VulkanCore*)this,
			GetComputeCommandPool(),
			m_viewDevice,
			&m_swapChain,
			&m_avt);
		// Voxel renderer debug pipeline state
		CreateVoxelRenderDebugState(
			VoxelDebugState,
			m_frameBuffers.data(),
			m_swapChain.m_imageCount,
			(VulkanCore*)this,
			m_devicePools.graphics,
			&m_swapChain,
			m_staticDescriptorSet,
			m_staticDescriptorSetLayout,
			m_camera,
			&m_avt);
		// Cone tracer pipeline state
		CreateConeTraceState(
			ConeTraceState,
			m_swapChain.m_imageCount,
			(VulkanCore*)this,
			m_devicePools.compute,
			&m_swapChain,
			&m_avt);
		// Forward main renderer pipeline state
		CreateForwardMainRendererState(
			ForwardMainRenderState,
			m_frameBuffers.data(),
			m_swapChain.m_imageCount,
			(VulkanCore*)this,
			m_devicePools.graphics,
			m_viewDevice,
			&m_swapChain,
			m_staticDescriptorSet,
			&m_vertices,
			m_meshes,
			m_meshCount,
			m_renderPass,
			m_staticDescriptorSetLayout,
			&m_avt);
		// Deferred main renderer pipeline state
		CreateDeferredMainRenderState(
			DeferredMainRenderState,
			m_swapChain.m_imageCount,
			(VulkanCore*)this,
			GetGraphicsCommandPool(),
			&m_swapChain,
			m_staticDescriptorSet,
			&m_vertices,
			m_meshes,
			m_meshCount,
			&m_avt,
			m_staticDescriptorSetLayout,
			m_cvctSettings.deferredScale);

		m_prepared = true;
	}
};

// todo: clean this
void resize(GLFWwindow* window, int w, int h)
{
	m_cvct->WindowResize(window, w, h);
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow)
{
	// Setup window
	glfwSetErrorCallback(error_callback);
	if (!glfwInit())
		return 1;

	m_cvct = new CVCT("CVCT V0.01","CVCT Console");
	// Initialize window
	m_cvct->InitializeGLFWWindow(false);
	// Load model here
	std::string path = SPONZAPATH;
	m_assetManager.InitAssetManager();
	m_assetManager.LoadAsset(path.c_str(), (uint32_t)path.length());
	// Initialize vulkan
	m_cvct->InitializeSwapchain();
	m_cvct->CVCT::Prepare();
	// Renderloop
	m_cvct->RenderLoop();

	// Flush all assets
	m_assetManager.FlushAssets();
	// Cleanup
	// todo: do cleanup

	return 0;
}