#ifndef VCTPIPELINEDEFINES_H
#define VCTPIPELINEDEFINES_H

#include "VKTools.h"
#include "Defines.h"
#include <glm/glm.hpp>
#include "MemoryAllocator.h"

class VulkanCore;
class SwapChain;
struct Vertices;
struct Indices;
struct VKMesh;

////////////////////////////////////////////////////////////////////////////////
// Descriptorset Layouts
////////////////////////////////////////////////////////////////////////////////
enum StaticDescriptorLayout
{
	STATIC_DESCRIPTOR_PERSCENE_BUFFER = 0,
	STATIC_DESCRIPTOR_SAMPLER,
	STATIC_DESCRIPTOR_IMAGE,
	STATIC_DESCRIPTOR_PERMESH_BUFFER,
	STATIC_DESCRIPTOR_CAMERA_BUFFER,
	STATIC_DESCRIPTOR_COUNT,
};
enum MeshTextureDescriptorLayout
{
	MESHTEXTURE_DESCRIPTOR_DIFFUSE = 0,
	MESHTEXTURE_DESCRIPTOR_NORMAL,
	MESHTEXTURE_DESCRIPTOR_OPACITY,
	MESHTEXTURE_DESCRIPTOR_EMISSION,
	MESHTEXTURE_DESCRIPTOR_COUNT,
};
enum VoxelizerDescriptorLayout
{
	VOXELIZER_DESCRIPTOR_ALBEDOOPACITY = 0,
	VOXELIZER_DESCRIPTOR_NORMAL,
	VOXELIZER_DESCRIPTOR_EMISSION,
	VOXELIZER_DESCRIPTOR_UBO_GEOM,
	VOXELIZER_DESCRIPTOR_UBO_FRAG,
	VOXELIZER_DESCRIPTOR_COUNT,
};
enum PostVoxelizerDescriptorLayout
{
	POSTVOXELIZER_DESCRIPTOR_VOXELGRID_DIFFUSE = 0,
	POSTVOXELIZER_DESCRIPTOR_VOXELGRID_NORMAL,
	POSTVOXELIZER_DESCRIPTOR_VOXELGRID_EMISSION,
	POSTVOXELIZER_DESCRIPTOR_VOXELGRID_BUFFERPOSITION,
	POSTVOXELIZER_DESCRIPTOR_BUFFER_SURFACEPOSITION,
	POSTVOXELIZER_DESCRIPTOR_BUFFER_SURFACECOUNT,
	POSTVOXELIZER_DESCRIPTOR_BUFFER_DISPATCHINDIRECT,
	POSTVOXELIZER_DESCRIPTOR_TEXTURE_DIFFUSE,
	POSTVOXELIZER_DESCRIPTOR_TEXTURE_NORMAL,
	POSTVOXELIZER_DESCRIPTOR_TEXTURE_EMISSION,
	POSTVOXELIZER_DESCRIPTOR_TEXTURE_SAMPLER,
	POSTVOXELIZERDESCRIPTOR_COUNT
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
enum GridCullingDescriptorLayout
{
	GRIDCULLING_DESCRIPTOR_INSTANCE_DATA = 0,
	GRIDCULLING_DESCRIPTOR_INSTANCE_DRAW,
	GRIDCULLING_DESCRIPTOR_STATISTICS,
	GRIDCULLING_DESCRIPTOR_INSTANCE_DRAW2,
	GRIDCULLING_DESCRIPTOR_DEBUG_DATA,

	GRIDCULLING_DESCRIPTOR_COUNT,
};

enum DebugDrawRendererDescriptorLayout
{
	DEBUGRENDERER_DESCRIPTOR_DEBUGBUFFER = 0,

	DEBUGRENDERER_DESCRIPTOR_COUNT,
};

////////////////////////////////////////////////////////////////////////////////
// Uniform buffer structures
////////////////////////////////////////////////////////////////////////////////
// Static UBO

// Forward renderer uniform buffer structures
struct ForwardRenderUBO
{
	glm::mat4 movel;
	glm::mat4 view;
	glm::mat4 projection;
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
struct VoxelizerUBOFrag
{

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
	BufferObject*			m_bufferData;
	uint32_t				m_bufferDataCount;
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