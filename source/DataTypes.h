#ifndef DATATYPES_H
#define DATATYPES_H

#include <glm/glm.hpp>
#include <vulkan.h>

struct TextureMipMapperUBOComp;
struct UniformData;
struct AnisotropicVoxelTexture;
class Camera;
struct CVCTSettings;
class SwapChain;

enum VoxelDirections { POSX, NEGX, POSY, NEGY, POSZ, NEGZ, NUM_DIRECTIONS };

struct mesh_s
{
	uint32_t primitiveType;
	uint32_t vertexBufferStartIndex;
	uint32_t indexBufferStartIndex;
	uint32_t vertexBufferCount;
	uint32_t indexBufferCount;
	uint64_t vertexCount;
};

struct model_ref_s
{
	uint64_t nameOffset;
	uint32_t modelIndex;
	uint32_t* materialIndices;
	uint32_t materialIndexCount;
	glm::mat4 transform;
};

struct model_s
{
	uint32_t meshStartIndex;
	uint32_t meshCount;
};

struct material_s
{
	uint64_t nameStringOffset;
	uint32_t textureReferenceStart;
	uint32_t textureReferenceCount;
};

struct texture_s
{
	uint64_t pathOffset;
};

struct texture_ref_s
{
	uint64_t attribOffset;
	uint32_t textureIndex;
};

struct point_light_s
{
	uint64_t nameOffset;
	glm::mat4 transform;
	float color[3];
	float constantAttenuation, linearAttenuation, quadraticAttenuation, attenuationScale, attenuationOffset;
	uint32_t flags;
};

struct spot_light_s
{
	uint64_t nameOffset;
	glm::mat4 transform;
	float color[3];
	float constantAttenuation, linearAttenuation, quadraticAttenuation, attenuationScale, attenuationOffset;
	float outerAngle, innerAngle;
	uint32_t flags;
};

struct directional_light_s
{
	uint64_t nameOffset;
	float direction[3];
	float color[3];
	uint32_t flags;
};

struct vertex_buffer_s
{
	uint32_t elementType;
	uint32_t elementCount;
	uint32_t vertexCount;
	uint64_t attribStringOffset;
	uint64_t vertexOffset;
	uint64_t totalSize;
};

struct index_buffer_s
{
	uint32_t indexByteSize;
	uint32_t materialSlotIndex;
	uint32_t indexCount;
	uint64_t indexOffset;
	uint64_t totalSize;
};

struct scene_s
{
	model_s* models;
	mesh_s* meshes;
	material_s* materials;
	vertex_buffer_s* vertexBuffers;
	index_buffer_s* indexBuffers;
	model_ref_s* modelRefs;
	texture_s* textures;
	texture_ref_s* textureRefs;
	spot_light_s* spotLights;
	point_light_s* pointLights;
	directional_light_s* directionalLights;

	uint32_t* materialIndices;
	//uint32_t* meshIndices;
	//uint64_t* materialStringOffsets;

	uint8_t* vertexData;
	uint8_t* indexData;
	const char* stringData;

	uint64_t vertexDataSizeInBytes;
	uint64_t indexDataSizeInBytes;
	uint64_t stringDataSizeInBytes;

	uint32_t materialIndexCount;
	//uint32_t meshIndexCount;
	//uint32_t materialStringOffsetCount;

	uint32_t modelCount;
	uint32_t meshCount;
	uint32_t materialCount;
	uint32_t vertexBufferCount;
	uint32_t indexBufferCount;
	uint32_t modelReferenceCount;
	uint32_t textureCount;
	uint32_t textureReferenceCount;
	uint32_t spotLightCount;
	uint32_t pointLightCount;
	uint32_t directionalLightCount;
};

struct asset_s
{
	uint64_t type;
	uint64_t size;
	uint8_t* data;
};

struct AssetDescriptor
{
	char name[512];
	asset_s asset;
};

struct AssetCacheHeader
{
	uint32_t magicNumber;
	uint64_t entryCount;
	uint64_t assetBlobSize;
	uint64_t dependencyBlobSize;
};

struct CacheEntry
{
	char name[265];
	uint64_t timestamp;
	uint64_t contentHash;
	uint64_t contentLength;
	const char* dependenciesStart;
	uint32_t dependencyCount;
	asset_s asset;
};

enum
{
	ALLOCATOR_IDX_ASSET_DATA = 10,
	ALLOCATOR_IDX_ASSET_DESC = 11,
	ALLOCATOR_IDX_CACHE_ENTRY = 12,
	ALLOCATOR_IDX_DEPENDENCIES = 13,

	ALLOCATOR_IDX_TEMP = 20,
};

//image data structures
struct mip_desc_s
{
	uint32_t width, height;
	uint32_t offset;
};

#pragma warning(disable: 4200)
struct image_desc_s
{
	uint32_t width, height, mipCount;
	mip_desc_s* mips;
};

struct vk_ib_s
{
	VkBuffer buffer;
	VkIndexType format;
	uint64_t offset, count;
};

enum TextureIndex
{
	DIFFUSE_TEXTURE = 0,
	NORMAL_TEXTURE = 1,
	OPACITY_TEXTURE = 2,

	TEXTURE_NUM,
};

struct vk_mesh_s
{
	VkBuffer vertexResources[8];
	uint64_t vertexStrides[8];
	uint64_t vertexOffsets[8];
	struct
	{
		vk_ib_s ibv;
		uint32_t indexCount;
		uint32_t textureIndex[TextureIndex::TEXTURE_NUM];

	} submeshes[4];
	uint32_t vbvCount, submeshCount;
	uint64_t vertexCount;
};

struct vk_texture_s
{
	uint32_t				width, height, mipCount, descriptorSetCount;
	VkSampler				sampler;
	VkImage					image;
	VkImageLayout			imageLayout;
	VkImageView*			view;
	VkDescriptorImageInfo*	descriptor;
	//VkFormat				format;
	VkDeviceMemory			deviceMemory;
	TextureMipMapperUBOComp*	ubo;
	UniformData*			uboDescriptor;
};

enum VertexOffset
{
	ATTRIBUTE_POSITION,
	ATTRIBUTE_TEXCOORD,
	ATTRIBUTE_NORMAL,
	ATTRIBUTE_TANGENT,
	ATTRIBUTE_BITANGENT,

	ATTRIBUTE_COUNT
};

struct FrameBufferAttachment
{
	VkImage image;
	VkDeviceMemory mem;
	VkImageView view;
	VkFormat format;
};

enum RenderFlags
{
	// Rendering options
	RENDER_FORWARD = 1 << 0,
	RENDER_VOXELDEBUG = 1 << 1,
	RENDER_CONETRACE = 1 << 2,
	RENDER_FORWARDMAIN = 1 << 3,
	RENDER_VOXELIZE = 1 << 4,

	// Mipmaps
	RENDER_MIPMAP0 = 1 << 5,
	RENDER_MIPMAP1 = 1 << 6,
	RENDER_MIPMAP2 = 1 << 7,
	RENDER_MIPMAP3 = 1 << 8,
	RENDER_MIPMAP4 = 1 << 9,
	RENDER_MIPMAP5 = 1 << 10,
	RENDER_MIPMAP6 = 1 << 11,
	// Cascades
	RENDER_CASCADE1 = 1 << 12,
	RENDER_CASCADE2 = 1 << 13,
	RENDER_CASCADE3 = 1 << 14,
	RENDER_CASCADE4 = 1 << 15,
	RENDER_CASCADE5 = 1 << 16,
	RENDER_CASCADE6 = 1 << 17,
	RENDER_CASCADE7 = 1 << 18,

	// additional
	RENDER_VOXELCASCADE = 1 << 19,
	RENDER_DEFERREDMAIN = 1 << 20,
};

struct CVCTSettings
{
	uint32_t cascadeCount;		// Number of cascades
	float gridRegion;			// Size of the voxel base region
	uint32_t gridSize;			// Grid size
	float deferredScale;		// Scale of the deferred rendering

	uint32_t cascadeNum;		// Current processed cascade
	uint32_t currentMipMap = 0;
	uint32_t currentSide = VoxelDirections::POSX;
	uint32_t conecount;
	uint32_t deferredRender = 0;
};

struct RenderStatesTimeStamps
{
	float voxelizerTimestamp;
	float postVoxelizerTimestamp;
	float mipMapperTimestamp;
	float forwardRendererTimestamp;
	float conetracerTimestamp;
	float forwardMainRendererTimestamp;
	float deferredMainRendererTimestamp;
};

struct ImGUIParameters
{
	Camera* camera;
	uint32_t currentBuffer;
	uint32_t* renderflags;
	AnisotropicVoxelTexture* avt;
	float appversion;
	SwapChain* swapchain;
	glm::uvec2* screenres;
	float dt;
	RenderStatesTimeStamps* timeStamps;
	CVCTSettings* settings;
	uint32_t* conecount;
};


#endif	//DATATYPES_H
