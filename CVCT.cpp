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
#include "SceneTools.h"

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
#define LIGHTPATH "Assets/cube.ogex"

// Cascade voxel grid defines
#define GRIDSIZE 64				// number of voxels per cascade
#define GRIDMIPMAP 3			// Number of mipmap per cascade
#define GRIDREGION 6.0f			// Base grid reigon
#define CASCADECOUNT 3			// Number of sascades
#define CONECOUNT 15			// Default number of cones
#define DEFERRED_DEFAULT 0		// Default deferred renderer
#define DEFERRED_SCALE 0.5f		//
#define TEXTURE_DESC_COUNT	2048//
#define DEBUG_BOX_COUNT 2048
#define AXISCOUNT 3

// Forward declare
class CVCT;

///////////////////////////////////////////////////////
// Global Members
///////////////////////////////////////////////////////
// Members
CVCT* m_cvct;
AnisotropicVoxelTexture			m_avt;						// Grid instance containing the voxels in 3D texture
uint32_t						m_renderFlags = 0;			// Render flags
CVCTSettings					m_cvctSettings = {};		// Cascade settings
RenderStatesTimeStamps			m_timeStamps = {};			// state timestamps
VkDrawIndexedIndirectCommand	indirectCommands[MAXMESHES*4];// indirectcommands
InstanceStatistic				indirectStats;				// Indirect draw statistics (updated via compute)
PerSceneUBO						perSceneUBO[MAXSCENES];		// Per Scene Uniform Buffer
PerSubMeshUBO					persubmeshUBO[MAXMESHES*4];	// Per (sub)Meshh Uniform Buffer
CameraUBO						cameraUBO;					// Camera Uniform Buffer
AssetManager					assetManager;				// Asset manager
VKScene							scenes[MAXSCENES];			// loaded models
VKMesh							meshes[MAXMESHES];			// Vulkan mesh list
VKSubMesh						submeshes[MAXMESHES * 4];	// Vulkan submesh list
VKTexture						textures[MAXTEXTURES];		// Vulkan texture list
uint32_t						sceneCount = 0;				// Loaded scenes counter
uint32_t						meshCount = 0;				// Mesh Counter
uint32_t						submeshCount = 0;			// Sub Mesh Counter
uint32_t						textureCount = 0;			// Texture Counter
InstanceDataAABB				instanceAABB[MAXMESHES*4];	// Instance AABB
VoxelizerGrid					voxelizergrid[AXISCOUNT];		// voxelzer grid, used for voxelization
VoxelGrid						voxelgrid;		// voxel grid, used to store voxelization data
Camera* camera;

// Renderstates
RenderState ImGUIState = {};				// ImGUI state
RenderState GridCullingState = {};			// Grid Culling state
RenderState ForwardRendererState = {};		// Forward Renderer state
RenderState DebugBoxRenderState = {};		// Debug box renderer
RenderState VoxelizerState = {};			// Voxelization state
RenderState PostVoxelizerState = {};		// Post voxelization state
VkCommandBuffer m_uploadCommandBuffer = {};
VkCommandBuffer m_clearCommandBuffer = {};

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
	// Descriptorset Layout
	VkDescriptorSetLayout m_staticDescriptorSetLayout;
	VkDescriptorSetLayout m_textureDescriptorSetLayout;
	// DescriptorPools
	VkDescriptorPool m_staticDescriptorPool;
	VkDescriptorPool m_textureDescriptorPool;
	VkDescriptorPool m_imguiDescriptorPool;				// Descriptorpool specifically for imgui
	// Descriptorsets
	VkDescriptorSet m_staticDescriptorSet;
	VkDescriptorSet m_textureDescriptorSet[TEXTURE_DESC_COUNT];
	// Uniform buffer object Vertex shader
	BufferObject m_boPerSceneData;
	BufferObject m_boPerSubMeshData;
	BufferObject m_boCameraData;
	VkSampler m_sampler;
	VkSampler m_voxelLinearSampler;
	VkSampler m_voxelPointSampler;
	bool m_prepared = {};
	float m_deltaTime = 0;		// in seconds
	bool m_paused = {};
	glm::dvec2 m_mousePosition;
	glm::vec2 m_prevMousePosition;
	uint32_t m_currentBuffer = 0;

public:
	// Cascade helper funcitons
	void VoxelizeCascade(uint32_t cascade)
	{
		m_cvctSettings.cascadeNum = cascade;
		UpdateUniformBuffers();
	}

	// Change voxelgrid size
	/*void ChangeVoxelGridSizeAndCascade(uint32_t size,uint32_t cascade)
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
	}*/

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
	/*void WindowResize(GLFWwindow* window, int width, int height)
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
	}*/

	void DrawImGUI()
	{
		ImGUIParameters* par = (ImGUIParameters*)ImGUIState.m_cmdBufferParameters;
		par->camera = camera;
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

	bool exec = false;
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
		
	//	// Clear the swapchain images textures
	//	VK_CHECK_RESULT(vkBeginCommandBuffer(m_clearCommandBuffer, &cmdBufInfo));
	//	m_swapChain.ClearImages(m_clearCommandBuffer, m_currentBuffer);
	//	VKTools::FlushCommandBuffer(m_clearCommandBuffer, m_deviceQueues.graphics, m_viewDevice, m_devicePools.graphics, false);
	//	// Clear the anisotroipc voxel texture
	//	VK_CHECK_RESULT(vkBeginCommandBuffer(m_clearCommandBuffer, &cmdBufInfo));
	//	ClearAnisotropicVoxelTexture(&m_avt, m_clearCommandBuffer);
	//	VKTools::FlushCommandBuffer(m_clearCommandBuffer, m_deviceQueues.graphics, m_viewDevice, m_devicePools.graphics, false);

		// Grid culling
		DestroyCommandBuffer(
			GridCullingState,
			this,
			m_devicePools.compute);
		CommandGridCulling(
			GridCullingState,
			this,
			submeshCount,
			glm::vec3(0,0,0),
			glm::vec3(10, 10, 10));
		m_submitInfo.pSignalSemaphores = &GridCullingState.m_semaphores[0];
		m_submitInfo.pCommandBuffers = &GridCullingState.m_commandBuffers[0];
		VK_CHECK_RESULT(vkQueueSubmit(m_deviceQueues.compute, 1, &m_submitInfo, VK_NULL_HANDLE));
		m_submitInfo.pWaitSemaphores = &GridCullingState.m_semaphores[0];

		m_submitInfo.pSignalSemaphores = &ForwardRendererState.m_semaphores[0];
		m_submitInfo.pCommandBuffers = &ForwardRendererState.m_commandBuffers[m_currentBuffer];
		VK_CHECK_RESULT(vkQueueSubmit(m_deviceQueues.graphics, 1, &m_submitInfo, VK_NULL_HANDLE));
		m_submitInfo.pWaitSemaphores = &ForwardRendererState.m_semaphores[0];

		m_submitInfo.pSignalSemaphores = &DebugBoxRenderState.m_semaphores[0];
		m_submitInfo.pCommandBuffers = &DebugBoxRenderState.m_commandBuffers[m_currentBuffer];
		VK_CHECK_RESULT(vkQueueSubmit(m_deviceQueues.graphics, 1, &m_submitInfo, VK_NULL_HANDLE));
		m_submitInfo.pWaitSemaphores = &DebugBoxRenderState.m_semaphores[0];

		UpdateDUBOVoxelizer(
			VoxelizerState,
			*this,
			glm::vec4(0, 0, 0, 5),
			glm::ivec3(64, 64, 64));

		m_submitInfo.pSignalSemaphores = &VoxelizerState.m_semaphores[0];
		m_submitInfo.pCommandBuffers = &VoxelizerState.m_commandBuffers[0];
		VK_CHECK_RESULT(vkQueueSubmit(m_deviceQueues.graphics, 1, &m_submitInfo, VK_NULL_HANDLE));
		m_submitInfo.pWaitSemaphores = &VoxelizerState.m_semaphores[0];

		m_submitInfo.pSignalSemaphores = &VoxelizerState.m_semaphores[1];
		m_submitInfo.pCommandBuffers = &VoxelizerState.m_commandBuffers[1];
		VK_CHECK_RESULT(vkQueueSubmit(m_deviceQueues.graphics, 1, &m_submitInfo, VK_NULL_HANDLE));
		m_submitInfo.pWaitSemaphores = &VoxelizerState.m_semaphores[1];

		m_submitInfo.pSignalSemaphores = &VoxelizerState.m_semaphores[2];
		m_submitInfo.pCommandBuffers = &VoxelizerState.m_commandBuffers[2];
		VK_CHECK_RESULT(vkQueueSubmit(m_deviceQueues.graphics, 1, &m_submitInfo, VK_NULL_HANDLE));
		m_submitInfo.pWaitSemaphores = &VoxelizerState.m_semaphores[2];

		if (!exec)
		{
			DestroyCommandBuffer(
				PostVoxelizerState,
				this,
				m_devicePools.compute);
			CommandIndirectPostVoxelizer(
				PostVoxelizerState,
				this,
				&voxelgrid,
				glm::ivec3(GRIDSIZE),
				0);

			m_submitInfo.pSignalSemaphores = &PostVoxelizerState.m_semaphores[0];
			m_submitInfo.pCommandBuffers = &PostVoxelizerState.m_commandBuffers[0];
			VK_CHECK_RESULT(vkQueueSubmit(m_deviceQueues.compute, 1, &m_submitInfo, VK_NULL_HANDLE));
			m_submitInfo.pWaitSemaphores = &PostVoxelizerState.m_semaphores[0];

			exec = true;
		}
		


		// Voxel building pass
		float accVoxelizer = 0;
		float accPostVoxelizer = 0;
		float accMipmapper = 0;
		if ((m_renderFlags & RenderFlags::RENDER_VOXELIZE))
		{
			for (uint32_t i = 0; i < m_cvctSettings.cascadeCount; i++)
			{
		//		VoxelizeCascade(i);
		//
		//		// Voxelizer rendering
		//		m_submitInfo.pSignalSemaphores = &VoxelizerState.m_semaphores[i];
		//		m_submitInfo.pCommandBuffers = &VoxelizerState.m_commandBuffers[i];
		//		VK_CHECK_RESULT(vkQueueSubmit(m_deviceQueues.graphics, 1, &m_submitInfo, VK_NULL_HANDLE));
		//		m_submitInfo.pWaitSemaphores = &VoxelizerState.m_semaphores[i];
		//
		//		// Post voxelizer
		//		m_submitInfo.pSignalSemaphores = &PostVoxelizerState.m_semaphores[i];
		//		m_submitInfo.pCommandBuffers = &PostVoxelizerState.m_commandBuffers[i];
		//		VK_CHECK_RESULT(vkQueueSubmit(m_deviceQueues.compute, 1, &m_submitInfo, VK_NULL_HANDLE));
		//		m_submitInfo.pWaitSemaphores = &PostVoxelizerState.m_semaphores[i];
		//
		//		// Voxel mipmapping
		//		m_submitInfo.pSignalSemaphores = &VoxelMipMapperState.m_semaphores[i];
		//		m_submitInfo.pCommandBuffers = &VoxelMipMapperState.m_commandBuffers[i];
		//		VK_CHECK_RESULT(vkQueueSubmit(m_deviceQueues.compute, 1, &m_submitInfo, VK_NULL_HANDLE));
		//		m_submitInfo.pWaitSemaphores = &VoxelMipMapperState.m_semaphores[i];
		//
		//		vkDeviceWaitIdle(m_viewDevice);
		//
		//		accVoxelizer += GetTimeStamp(VoxelizerState, 0, 2, 0, 1);
		//		accPostVoxelizer += GetTimeStamp(PostVoxelizerState, 0, 2, 0, 1);
		//		accMipmapper += GetTimeStamp(VoxelMipMapperState, 0, 2, 0, 1);
			}
		}

//		// Pick one of the renderers
//		if((m_renderFlags & RenderFlags::RENDER_FORWARD))
//		{
//			//Scene rendering
//			m_submitInfo.pSignalSemaphores = &ForwardRendererState.m_semaphores[0];
//			m_submitInfo.pCommandBuffers = &ForwardRendererState.m_commandBuffers[m_currentBuffer];
//			VK_CHECK_RESULT(vkQueueSubmit(m_deviceQueues.graphics, 1, &m_submitInfo, VK_NULL_HANDLE));
//			m_submitInfo.pWaitSemaphores = &ForwardRendererState.m_semaphores[0];
//		}
//		else if ((m_renderFlags & RenderFlags::RENDER_CONETRACE))
//		{
//			// Voxelizer Debug rendering
//			m_submitInfo.pSignalSemaphores = &ConeTraceState.m_semaphores[0];
//			m_submitInfo.pCommandBuffers = &ConeTraceState.m_commandBuffers[m_currentBuffer];
//			VK_CHECK_RESULT(vkQueueSubmit(m_deviceQueues.compute, 1, &m_submitInfo, VK_NULL_HANDLE));
//			m_submitInfo.pWaitSemaphores = &ConeTraceState.m_semaphores[0];
//		}
//		else if ((m_renderFlags & RenderFlags::RENDER_FORWARDMAIN))
//		{
//			// Scene rendering
//			m_submitInfo.pSignalSemaphores = &ForwardMainRenderState.m_semaphores[0];
//			m_submitInfo.pCommandBuffers = &ForwardMainRenderState.m_commandBuffers[m_currentBuffer];
//			VK_CHECK_RESULT(vkQueueSubmit(m_deviceQueues.graphics, 1, &m_submitInfo, VK_NULL_HANDLE));
//			m_submitInfo.pWaitSemaphores = &ForwardMainRenderState.m_semaphores[0];
//		}
//		else if ((m_renderFlags & RenderFlags::RENDER_DEFERREDMAIN))
//		{
//			// Scene rendering
//			m_submitInfo.pSignalSemaphores = &DeferredMainRenderState.m_semaphores[0];
//			m_submitInfo.pCommandBuffers = &DeferredMainRenderState.m_commandBuffers[m_currentBuffer];
//			VK_CHECK_RESULT(vkQueueSubmit(m_deviceQueues.graphics, 1, &m_submitInfo, VK_NULL_HANDLE));
//			m_submitInfo.pWaitSemaphores = &DeferredMainRenderState.m_semaphores[0];
//		}
//		// nothing writing tot he framebuffer at all
//		else
//		{
//			VkImageSubresourceRange srRange = {};
//			srRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//			srRange.baseMipLevel = 0;
//			srRange.levelCount = 1;
//			srRange.baseArrayLayer = 0;
//			srRange.layerCount = 1;
//			//Set image layout
//			VK_CHECK_RESULT(vkBeginCommandBuffer(m_clearCommandBuffer, &cmdBufInfo));
//			VKTools::SetImageLayout(
//				m_clearCommandBuffer,
//				m_swapChain.m_images[m_currentBuffer],
//				VK_IMAGE_LAYOUT_UNDEFINED,
//				VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
//				srRange);
//			VKTools::FlushCommandBuffer(m_clearCommandBuffer, m_deviceQueues.graphics, m_viewDevice, m_devicePools.graphics, false);
//		}
//	
//		if ((m_renderFlags & RenderFlags::RENDER_VOXELDEBUG) && (m_renderFlags & RenderFlags::RENDER_FORWARD))
//		{
//			// Voxelizer Debug rendering
//			m_submitInfo.pSignalSemaphores = &VoxelDebugState.m_semaphores[0];
//			// offsets in commandbuffer. order: fb1[cc1,cc2,cc3],fb2[cc1,cc2,cc3] etc etc..
//			m_submitInfo.pCommandBuffers = &VoxelDebugState.m_commandBuffers[m_currentBuffer * m_avt.m_cascadeCount];
//			VK_CHECK_RESULT(vkQueueSubmit(m_deviceQueues.graphics, 1, &m_submitInfo, VK_NULL_HANDLE));
//			m_submitInfo.pWaitSemaphores = &VoxelDebugState.m_semaphores[0];
//		}
//
//		// Imgui pass
//		if(!hideGUi)
//			DrawImGUI();
//
//		//Query here in milliseconds
//		float forwardTimestamp = 0;
//		float conetracerTimestamp = 0;
//		float forwardMainTimestamp = 0;
//		float deferredMainTimestamp = 0;
//
//		if (m_renderFlags & RenderFlags::RENDER_FORWARD) 
//			forwardTimestamp = GetTimeStamp(ForwardRendererState, 0, 2, 0, 1);
//		if (m_renderFlags & RenderFlags::RENDER_CONETRACE) 
//			conetracerTimestamp = GetTimeStamp(ConeTraceState, 0, 2, 0, 1);
//		if (m_renderFlags & RenderFlags::RENDER_FORWARDMAIN)
//			forwardMainTimestamp  = GetTimeStamp(ForwardMainRenderState, 0, 2, 0, 1);
//		if (m_renderFlags & RenderFlags::RENDER_DEFERREDMAIN)
//			deferredMainTimestamp = GetTimeStamp(DeferredMainRenderState, 0, 2, 0, 1);
//
//		m_timeStamps = RenderStatesTimeStamps{ accVoxelizer, accPostVoxelizer, accMipmapper, forwardTimestamp, conetracerTimestamp, forwardMainTimestamp, deferredMainTimestamp };
		
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
	//	if (gridChange != m_cvctSettings.gridSize)
	//	{
	//		gridChange = m_cvctSettings.gridSize;
	//		//ChangeVoxelGridSizeAndCascade(m_cvctSettings.gridSize, m_cvctSettings.cascadeCount);
	//	}
	//	if (cascadeChange != m_cvctSettings.cascadeCount)
	//	{
	//		cascadeChange = m_cvctSettings.cascadeCount;
	//		//ChangeVoxelGridSizeAndCascade(m_cvctSettings.gridSize, m_cvctSettings.cascadeCount);
	//	}

		UpdateUniformBuffers();
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
		}
	}

	ConeTracerUBOComp coneTracerUBO;
	ForwardMainRendererUBOFrag forwardMainrendererUBO;
	DeferredMainRendererUBOFrag deferredMainRendererUBO;
	void UpdateUniformBuffers()
	{
		// Correction matrix for perspective matrix for models being upside down
		glm::mat4 correction;
		correction[1][1] = -1;
		correction[2][2] = 0.5;
		correction[3][2] = 0.5;

		// Update camera uniform buffer
		cameraUBO.projectionMatrix = correction * glm::perspective(45.0f, (float)m_screenResolution.x / (float)m_screenResolution.y, 0.1f, 100.0f);
		cameraUBO.viewMatrix = camera->GetViewMatrix();
		cameraUBO.cameraPosition = camera->GetPosition();
		cameraUBO.forwardVector = camera->GetForwardVector();
		cameraUBO.upVector = camera->GetUpVector();

		uint8_t *pData;
		VK_CHECK_RESULT(vkMapMemory(GetViewDevice(), m_boCameraData.memory, 0, sizeof(CameraUBO), 0, (void**)&pData));
		memcpy(pData, &cameraUBO, sizeof(CameraUBO));
		vkUnmapMemory(m_viewDevice, m_boCameraData.memory);

		/*
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
		*/
	}

	void HandleMessages()
	{
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
		camera->Translate(cameraVelocity);
		// Camera rotation
		if (glfwGetMouseButton(m_glfwWindow, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS && !ImGui::IsMouseHoveringAnyWindow())
		{
			glm::dvec2 mousePos = {};
			glfwGetCursorPos(m_glfwWindow, &mousePos.x, &mousePos.y);
			glm::dvec2 mouseDelta = mousePos - m_mousePosition;
			glm::quat rotX = glm::angleAxis<double>(glm::radians(mouseDelta.y) * 0.5f, glm::dvec3(-1, 0, 0));
			glm::quat rotY = glm::angleAxis<double>(glm::radians(mouseDelta.x) * 0.5f, glm::dvec3(0, -1, 0));
			glm::quat rotation = camera->GetRotation() * rotX;
			rotation = rotY*rotation;
			camera->SetRotation(rotation);
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

	uint32_t CreateCommandBuffer()
	{
		m_uploadCommandBuffer = VKTools::Initializers::CreateCommandBuffer(m_devicePools.graphics, m_viewDevice, VK_COMMAND_BUFFER_LEVEL_PRIMARY, false);
		m_clearCommandBuffer = VKTools::Initializers::CreateCommandBuffer(m_devicePools.graphics, m_viewDevice, VK_COMMAND_BUFFER_LEVEL_PRIMARY, false);
		return 0;
	}

	void Prepare()
	{
		// Initialize vulkan specific features
		VulkanCore::Prepare();

		// Create command buffers
		CreateCommandBuffer();
		// Create the vulkan buffers
		CreateVulkanBuffers(this, m_boPerSceneData, m_boPerSubMeshData, m_boCameraData, scenes, sceneCount);
		// Create the static descriptorSets
		CreateDescriptor(this, TEXTURE_DESC_COUNT, m_staticDescriptorSetLayout, m_textureDescriptorSetLayout, m_staticDescriptorPool, m_textureDescriptorPool, m_staticDescriptorSet, m_textureDescriptorSet, m_boPerSceneData, m_boPerSubMeshData, m_boCameraData);
		// create image samplers
		CreateSamplers(GetViewDevice(), m_staticDescriptorSet, m_sampler);
		CreateVoxelSamplers(*this, m_voxelLinearSampler, m_voxelPointSampler);
		// Create the vulkan data
		CreateVulkanScenes(this, m_uploadCommandBuffer, scenes, sceneCount, perSceneUBO);					// Create the vulkan scenes
		CreateVulkanMeshes(this,m_uploadCommandBuffer, scenes, sceneCount, meshes, meshCount, submeshes, submeshCount, persubmeshUBO);									// Create the vulkan meshes
		CreateVulkanTextures(this, m_uploadCommandBuffer, assetManager, scenes, sceneCount, textures, textureCount, m_sampler, m_staticDescriptorSet);		// Create the vulkan tetures
		// Create indirectdraw buffer
		CreateIndirectDrawBuffer(this, indirectCommands, MAXMESHES*4, submeshes, submeshCount);
		// Update the vulkan buffers
		UploadVulkanBuffers(this, m_boPerSceneData, perSceneUBO, sceneCount, m_boPerSubMeshData, persubmeshUBO, submeshCount);
		// Update the texture descriptorsets
		UpdateDescriptorSets(this, scenes, sceneCount, meshes, meshCount, m_staticDescriptorSet, m_textureDescriptorSet);
		// Create the mipmap for all the textures
		BuildCommandMip(this,textures,textureCount);
		// Create the colliders
		CreateAABB(scenes,sceneCount,meshes,meshCount, instanceAABB);
		// Create the voxel textures
		uint32_t mip = (uint32_t)floor(log2(glm::max(GRIDSIZE, GRIDSIZE))) + 1;
		CreateVoxelTextures(*this, GRIDSIZE, 2, voxelizergrid, 3, voxelgrid, mip, 4);

		// initialize the render states

		//	// Initialize the Render Pipeline States
		//	// ImGUI pipeline state
		//	CreateImgGUIPipelineState(
		//		ImGUIState,
		//		m_swapChain.m_imageCount,InstanceStatistic
		//		(VulkanCore*)this,
		//		GetGraphicsCommandPool(),
		//		&m_swapChain);

		// Initialize the grid culling state
		StateGridCulling(
			GridCullingState,
			this,
			instanceAABB,
			submeshCount,
			indirectStats,
			indirectCommands,
			DEBUG_BOX_COUNT);
		// Initialize the forward renderer state
		StateForwardRenderer(
			ForwardRendererState,
			this,
			m_staticDescriptorSet,
			m_staticDescriptorSetLayout,
			m_textureDescriptorSetLayout,
			scenes,
			sceneCount);

		// Forward render
	//	DestroyCommandBuffer(
	//		ForwardRendererState,
	//		this,
	//		m_devicePools.graphics);
	//	CommandForwardRenderer(
	//		ForwardRendererState,
	//		this,
	//		scenes,
	//		sceneCount,
	//		meshes,
	//		meshCount,
	//		m_staticDescriptorSet,
	//		m_textureDescriptorSet);
		CommandIndirectForwardRender(
			ForwardRendererState,
			this,
			scenes,
			sceneCount,
			meshes,
			meshCount,
			m_staticDescriptorSet,
			m_textureDescriptorSet,
			GridCullingState.m_bufferData[1].buffer);

		// AABB debug renderer
		StateDebugRenderer(
			DebugBoxRenderState,
			this,
			m_staticDescriptorSetLayout,
			GridCullingState.m_bufferData[4]);
		CommandIndirectDebugRenderer(
			DebugBoxRenderState,
			this,
			m_staticDescriptorSet,
			GridCullingState.m_bufferData[3].buffer);

		// Create voxelizer staet
		StateVoxelizer(
			VoxelizerState,
			this,
			m_staticDescriptorSet,
			m_staticDescriptorSetLayout,
			m_textureDescriptorSetLayout,
			scenes,
			sceneCount,
			GRIDSIZE,
			voxelizergrid);
		// Create the command
		CommandIndirectVoxelizer(
			VoxelizerState,
			this,
			scenes,
			sceneCount,
			meshes,
			meshCount,
			m_staticDescriptorSet,
			m_textureDescriptorSet,
			GridCullingState.m_bufferData[1].buffer);

		// Create post voxeilzer state
		StatePostVoxelizer(
			PostVoxelizerState,
			this,
			GRIDSIZE,
			voxelizergrid,
			AXISCOUNT,
			&voxelgrid,
			m_voxelPointSampler);

		m_prepared = true;
	}
};

// todo: clean this
void resize(GLFWwindow* window, int w, int h)
{
	//m_cvct->WindowResize(window, w, h);
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow)
{
	// Setup window
	glfwSetErrorCallback(error_callback);
	if (!glfwInit())
		return 1;

	// glfw settings	todo: clean
	//glfwSetFramebufferSizeCallback(m_glfwWindow, resize);

	m_cvct = new CVCT("CVCT V0.01","CVCT Console");

	// Initialize window
	m_cvct->InitializeGLFWWindow(false);

	// Load model here
	glm::vec3 scale = { 0.01f,0.01f,0.01f };
	glm::mat4 spnozaModel = glm::scale(scale);
	std::string sponzapath = SPONZAPATH;
	std::string lightpath = LIGHTPATH;
	assetManager.InitAssetManager();
	LoadScene(assetManager, scenes, sceneCount, sponzapath.c_str(), (uint32_t)sponzapath.length(), spnozaModel);		//load sponza model
	LoadScene(assetManager, scenes, sceneCount, lightpath.c_str(), (uint32_t)lightpath.length(), spnozaModel);			//load light model

	// Initialize vulkan
	m_cvct->InitializeSwapchain();
	m_cvct->CVCT::Prepare();
	// Initialize the camera
	glm::uvec2 screenres = m_cvct->GetScreenResolution();
	camera = new Camera(screenres.x, screenres.y);

	// Renderloop
	m_cvct->RenderLoop();

	// Flush all assets
	assetManager.FlushAssets();

	// Cleanup
	// todo: do cleanup

	return 0;
}