#ifndef VCTPIPELINEDEFINES_H
#define VCTPIPELINEDEFINES_H

#include "VKTools.h"
#include "Defines.h"
#include <glm/glm.hpp>

class VulkanCore;
class SwapChain;
struct Vertices;
struct Indices;
struct vk_mesh_s;

#define DYNAMIC_DESCRIPTOR_SET_COUNT 8192

////////////////////////////////////////////////////////////////////////////////
// Descriptorset Layouts
////////////////////////////////////////////////////////////////////////////////
enum StaticDescriptorLayout
{
	STATIC_DESCRIPTOR_BUFFER = 0,
	STATIC_DESCRIPTOR_SAMPLER,
	STATIC_DESCRIPTOR_IMAGE,
	STATIC_DESCRIPTOR_COUNT,
};
enum ForwardRendererDescriptorLayout
{
	FORWARDRENDER_DESCRIPTOR_IMAGE_DIFFUSE,
	FORWARDRENDER_DESCRIPTOR_IMAGE_NORMAL,
	FORWARDRENDER_DESCRIPTOR_IMAGE_OPACITY,

	FORWARDRENDER_DESCRIPTOR_COUNT,
};
enum VoxelizerDescriptorLayout
{
	VOXELIZER_DESCRIPTOR_IMAGE_DIFFUSE = 0,
	VOXELIZER_DESCRIPTOR_IMAGE_NORMAL,
	VOXELIZER_DESCRIPTOR_IMAGE_OPACITY,
	VOXELIZER_MULTIPLE_DESCRIPTOR_COUNT,
	// Texture 3D
	VOXELIZER_DESCRIPTOR_IMAGE_VOXELGRID = 0,
	// Geometry UBO
	VOXELIZER_DESCRIPTOR_BUFFER_GEOM,
	// Fragment UBO
	VOXELIZER_DESCRIPTOR_BUFFER_FRAG,
	VOXELIZER_DESCRIPTOR_IMAGE_ALPHAVOXELGRID,
	VOXELIZER_SINGLE_DESCRIPTOR_COUNT,

	VOXELIZER_DESCRIPTOR_COUNT = VOXELIZER_SINGLE_DESCRIPTOR_COUNT + VOXELIZER_MULTIPLE_DESCRIPTOR_COUNT
};
enum VoxelizerDebugDescriptorLayout
{
	// Geometry UBO
	VOXELIZERDEBUG_DESCRIPTOR_VOXELGRID = 0,
	VOXELIZERDEBUG_DESCRIPTOR_BUFFER_GEOM,

	VOXELIZERDEBUG_DESCRIPTOR_COUNT,
};
enum MipMapperDescriptorLayout
{
	// Compute UBO
	MIPMAPPER_DESCRIPTOR_VOXELGRID = 0,
	MIPMAPPER_DESCRIPTOR_IMAGE_VOXELGRID,
	MIPMAPPER_DESCRIPTOR_BUFFER_COMP,

	MIPMAPPER_DESCRIPTOR_COUNT,
};
enum ConeTraceDescriptorLayout
{
	CONETRACER_DESCRIPTOR_VOXELGRID = 0,
	CONETRACER_DESCRIPTOR_BUFFER_COMP,
	CONETRACER_DESCRIPTOR_FRAMEBUFFER,

	CONETRACER_DESCRIPTOR_COUNT
};
enum PostVoxelizerDescriptorLayout
{
	POSTVOXELIZER_DESCRIPTOR_VOXELGRID_DIFFUSE = 0,
	POSTVOXELIZER_DESCRIPTOR_VOXELGRID_ALPHA,

	POSTVOXELIZERDESCRIPTOR_COUNT
};
enum ForwardMainRendererDescriptorLayout
{
	// Fragment multiple 
	FORWARD_MAIN_DESCRIPTOR_IMAGE_DIFFUSE = 0,
	FORWARD_MAIN_DESCRIPTOR_IMAGE_NORMAL,
	FORWARD_MAIN_DESCRIPTOR_IMAGE_OPACITY,
	
	FORWARD_MAIN_DESCRIPTOR_MULTIPLE_COUNT,

	// Fragment single
	FORWARD_MAIN_DESCRIPTOR_BUFFER_FRAG = 0,
	FORWARD_MAIN_DESCRIPTOR_VOXELGRID,

	FORWARD_MAIN_DESCRIPTOR_SINGLE_COUNT,

	FORWARD_MAIN_DESCRIPTOR_COUNT = FORWARD_MAIN_DESCRIPTOR_MULTIPLE_COUNT + FORWARD_MAIN_DESCRIPTOR_SINGLE_COUNT
};
enum DeferredMainRendererDescriptorLayout
{
	// Fragment multiple: first and third pass
	DEFERRED_MAIN_DESCRIPTOR_IMAGE_DIFFUSE = 0,
	DEFERRED_MAIN_DESCRIPTOR_IMAGE_NORMAL,
	DEFERRED_MAIN_DESCRIPTOR_IMAGE_OPACITY,
	DEFERRED_MAIN_DESCRIPTOR_MULTIPLE_COUNT,

	// Fragment: second pass fourth
	DEFERRED_MAIN_DESCRIPTOR_BUFFER_FRAG = 0,
	DEFERRED_MAIN_DESCRIPTOR_VOXELGRID,
	DEFERRED_MAIN_DESCRIPTOR_OUTPUT,
	DEFERRED_MAIN_DESCRIPTOR_INPUT_POSITION,
	DEFERRED_MAIN_DESCRIPTOR_INPUT_NORMAL,
	DEFERRED_MAIN_DESCRIPTOR_INPUT_ALBEDO,
	DEFERRED_MAIN_DESCRIPTOR_INPUT_TANGENT,
	DEFERRED_MAIN_DESCRIPTOR_PASS_COUNT,

	DEFERRED_MAIN_DESCRIPTOR_INPUT  = 0,

	DEFERRED_MAIN_DESCRIPTOR_COUNT = DEFERRED_MAIN_DESCRIPTOR_MULTIPLE_COUNT + DEFERRED_MAIN_DESCRIPTOR_PASS_COUNT + 1
};

////////////////////////////////////////////////////////////////////////////////
// Uniform buffer structures
////////////////////////////////////////////////////////////////////////////////
// Static UBO
struct StaticUBO
{
	glm::mat4 modelMatrix;
	glm::mat4 viewMatrix;
	glm::mat4 projectionMatrix;
};
// Forward renderer uniform buffer structures
struct ForwardRenderUBO
{
	glm::mat4 movel;
	glm::mat4 view;
	glm::mat4 projection;
};
// Voxelizer uniform buffer structures
struct VoxelizerUBOGeom
{
	glm::mat4 viewProjectionXY;
	glm::mat4 viewProjectionXZ;
	glm::mat4 viewProjectionYZ;
};
struct VoxelizerUBOFrag
{
	glm::vec4 voxelRegionWorld;	// Region of the world
	uint32_t voxelResolution;	// Resolution of the voxel grid
	uint32_t cascadeCount;		// Cascade count
	glm::vec2 padding;
};
// Voxelizer debug uniform buffer structures
struct VoxelizerDebugUBOGeom
{
	uint32_t gridResolution;
	float voxelSize;
	uint32_t mipmap;
	uint32_t side;
	glm::vec4 voxelRegionWorld;
};
// Voxel mip mapper uniform buffer structures
struct VoxelMipMapperUBOComp
{
	uint32_t srcMipLevel;
	uint32_t numMipLevels;
	uint32_t cascadeCount;
	float padding0;
	glm::vec3 texelSize;
	uint32_t gridSize;
};
// Texzture mip mapper uniform buffer structures
struct TextureMipMapperUBOComp
{
	uint32_t srcMipLevel;
	uint32_t numMipLevels;
	glm::vec2 padding1;
	glm::vec2 texelSize;
	glm::vec2 padding2;
};
// Cone Tracer uniform buffer structure
struct ConeTracerUBOComp
{
	glm::vec4 voxelRegionWorld[10];		// Base Voxel region world
	glm::vec3 cameraPosition;			// Camera world position
	float fovy;							// vertical field of view in radius
	glm::vec3 cameraLookAt;				// Camera forward vector
	float aspect;						// aspect ratio (screen width / screen height)
	glm::vec3 cameraUp;					// Camera up vector
	uint32_t cascadeCount;				// Number of cascades
	glm::vec2 screenres;				// Screen resolution in pixels
	glm::vec2 padding1;
	glm::vec3 voxelGridResolution;		// voxel grid resolution
	float padding2;
};
// Main renderer UBO
struct ForwardMainRendererUBOFrag
{
	glm::vec4 voxelRegionWorld[10];	// list of voxel region worlds
	glm::vec3 cameraPosition;		// Positon of the camera
	uint32_t voxelGridResolution;	// Resolution of the voxel grid
	uint32_t cascadeCount;			// Number of cascades
	glm::vec3 padding0;
};

struct DeferredMainRendererUBOFrag
{
	glm::vec4 voxelRegionWorld[10];	// list of voxel region worlds
	glm::vec3 cameraPosition;		// Positon of the camera
	uint32_t voxelGridResolution;	// Resolution of the voxel grid
	uint32_t cascadeCount;			// Number of cascades
	float scaledWidth;				// scaled framebuffer size width
	float scaledHeight;				// scaled framebuffer size height
	uint32_t conecount;				// Cone count
	uint32_t deferredRenderer;		// deferred renderer
	glm::vec3 padding0;
};

struct UniformData
{
	VkBuffer				m_buffer;
	VkDeviceMemory			m_memory;
	VkDescriptorBufferInfo	m_descriptor;
	uint32_t				m_allocSize;
	BYTE*					m_mapped;
};

struct RenderState
{
	VkPipelineLayout		m_pipelineLayout;
	VkRenderPass			m_renderpass;
	VkDescriptorSetLayout*	m_descriptorLayouts;
	uint32_t				m_descriptorLayoutCount;
	VkDescriptorSet*		m_descriptorSets;
	uint32_t				m_descriptorSetCount;
	VkPipeline*				m_pipelines;
	uint32_t				m_pipelineCount;
	VkPipelineCache			m_pipelineCache;
	VkCommandBuffer*		m_commandBuffers;
	uint32_t				m_commandBufferCount;
	VkSemaphore*			m_semaphores;
	uint32_t				m_semaphoreCount;
	UniformData*			m_uniformData;
	uint32_t				m_uniformDataCount;
	VkDescriptorPool		m_descriptorPool;
	VkFramebuffer*			m_framebuffers;
	uint32_t				m_framebufferCount;
	FrameBufferAttachment*	m_framebufferAttatchments;
	uint32_t				m_framebufferAttatchmentCount;
	VkQueryPool				m_queryPool;
	uint32_t				m_queryCount;
	uint64_t*				m_queryResults;

	// Funciton pointer
	void(*m_CreateCommandBufferFunc)(RenderState*, VkCommandPool, VulkanCore*, uint32_t, VkFramebuffer*, BYTE*);
	BYTE* m_cmdBufferParameters;
};

#endif	//CVCTPIPELINES_H