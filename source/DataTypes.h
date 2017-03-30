#ifndef DATATYPES_H
#define DATATYPES_H

#include <glm/glm.hpp>
#include <vulkan.h>
#include <vector>

// Texture & Mesh defines
#define MAXTEXTURES 128			// Number of max textures
#define MAXMESHES 512			// Number of max meshes
#define MAXSCENES 12			// Number of max loaded opengex models
#define MAXGRIDMIP 16
#define ASSETPATH "Assets/"

struct TextureMipMapperUBOComp;
struct BufferObject;
struct AnisotropicVoxelTexture;
struct IsotropicVoxelTexture;
class Camera;
struct CVCTSettings;
class SwapChain;

enum VoxelDirections { POSX = 0, NEGX, POSY, NEGY, POSZ, NEGZ, NUM_DIRECTIONS };
enum VoxelAxis{ AXIS_YZ = 0, AXIS_XZ, AXIS_XY };
const uint32_t INVALID_TEXTURE = 0xFFFFFF;

struct Vertices
{
	VkBuffer buf;			
	VkDeviceMemory mem;	
	VkPipelineVertexInputStateCreateInfo inputState;
	VkVertexInputBindingDescription* bindingDescriptions;
	uint32_t bindingDescriptionCount;
	VkVertexInputAttributeDescription* attributeDescriptions;
	uint32_t attributeDescriptionCount;
};

struct Indices
{
	VkBuffer buf;
	VkDeviceMemory mem;
};

struct Mesh
{
	uint32_t primitiveType;
	uint32_t vertexBufferStartIndex;
	uint32_t indexBufferStartIndex;
	uint32_t vertexBufferCount;
	uint32_t indexBufferCount;
	uint64_t vertexCount;
};

struct Modelref
{
	uint64_t nameOffset;
	uint32_t modelIndex;
	uint32_t* materialIndices;
	uint32_t materialIndexCount;
	glm::mat4 transform;
};

struct Model
{
	uint32_t meshStartIndex;
	uint32_t meshCount;
};

struct Material
{
	uint64_t nameStringOffset;
	uint32_t textureReferenceStart;
	uint32_t textureReferenceCount;
};

struct Texture
{
	uint64_t pathOffset;
};

struct Textureref
{
	uint64_t attribOffset;
	uint32_t textureIndex;
};

struct Pointlight
{
	uint64_t nameOffset;
	glm::mat4 transform;
	glm::vec3 color;
	float constantAttenuation, linearAttenuation, quadraticAttenuation, attenuationScale, attenuationOffset;
	uint32_t flags;
};

struct Spotlight
{
	uint64_t nameOffset;
	glm::mat4 transform;
	glm::vec3 color;
	float constantAttenuation, linearAttenuation, quadraticAttenuation, attenuationScale, attenuationOffset;
	float outerAngle, innerAngle;
	uint32_t flags;
};

struct Directionallight
{
	uint64_t nameOffset;
	glm::vec3 direction;
	glm::vec3 color;
	uint32_t flags;
};

struct Vertexbuffer
{
	uint32_t elementType;
	uint32_t elementCount;
	uint32_t vertexCount;
	uint64_t attribStringOffset;
	uint64_t vertexOffset;
	uint64_t totalSize;
};

struct Indexbuffer
{
	uint32_t indexByteSize;
	uint32_t materialSlotIndex;
	uint32_t indexCount;
	uint64_t indexOffset;
	uint64_t totalSize;
};

struct Scene
{
	Model* models;
	Mesh* meshes;
	Material* materials;
	Vertexbuffer* vertexBuffers;
	Indexbuffer* indexBuffers;
	Modelref* modelRefs;
	Texture* textures;
	Textureref* textureRefs;
	Spotlight* spotLights;
	Pointlight* pointLights;
	Directionallight* directionalLights;

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

struct Asset
{
	uint64_t type;
	uint64_t size;
	uint8_t* data;
};

struct AssetDescriptor
{
	char name[512];
	Asset asset;
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
	Asset asset;
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
struct MipDesc
{
	uint32_t width, height;
	uint32_t offset;
};

#pragma warning(disable: 4200)
struct ImageDesc
{
	uint32_t width, height, mipCount;
	MipDesc* mips;
};

struct vk_ib_s
{
	VkBuffer buffer;
	VkIndexType format;
	uint64_t offset, count;
};

struct BufferObject
{
	VkBuffer				buffer;
	VkDeviceMemory			memory;
	VkDescriptorBufferInfo	descriptor;
	uint32_t				allocSize;
	BYTE*					mapped;
};

enum TextureIndex
{
	DIFFUSE_TEXTURE,
	NORMAL_TEXTURE,
	OPACITY_TEXTURE,
	EMISSION_TEXTURE,

	TEXTURE_COUNT
};

struct VKSubMesh
{
	vk_ib_s ibv;
	uint32_t indexCount;
	uint32_t textureIndex[TextureIndex::TEXTURE_COUNT];
};

struct VKMesh
{
	VkBuffer vertexResources[8];
	uint64_t vertexStrides[8];
	uint64_t vertexOffsets[8];
	VKSubMesh* submeshes;
	uint32_t vbvCount, submeshCount;
	uint64_t vertexCount;
};

struct VKTexture
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
	BufferObject*			boDescriptor;
};

struct VKScene
{
	Scene* scene;
	VkBuffer scenebuffer;
	VkDeviceMemory scenememory;
	Vertices vertices;
	Indices indices;
	uint32_t textureOffset;
	glm::mat4 modelMatrix;
	VKMesh* vkmeshes;
	uint32_t vkmeshCount;
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

#define PREVOXELGRID_COUNT 16 //4*4

enum CascadeAttributes
{
	ALBEDO_OPACITY_GRID = 0,
	NORMAL_GRID = 1,
	EMISSION_GRID = 2,
	DIRECT_LIGHT_GRID = 3,
	BOUNCE_LIGHT_GRID = 4,

	CASCADE_ATTRIBUTE_COUNT,
};

struct CascadeAttribute
{
	struct PreVoxelGrid
	{
		AnisotropicVoxelTexture* preGrid;

	}preVoxelGrid[16 * 3];
	AnisotropicVoxelTexture* texture;
};

struct Cascade
{
	CascadeAttribute attributes[CASCADE_ATTRIBUTE_COUNT];
};

struct InstanceDataAABB
{
	glm::vec3 position;	// Middle position of the AABB
	float padding0;
	glm::vec3 size;		// Size of the AABB
	float padding1;
};

// Indirect draw statistics (updated via compute)
struct InstanceStatistic
{
	uint32_t drawCount;						// Total number of indirect draw counts to be issued
};

struct PerSubMeshUBO
{
	uint32_t idxDiffuse;
	uint32_t idxNormal;
	uint32_t idxOpacity;
	uint32_t idxEmission;
	BYTE padding0[240];
};

struct PerSceneUBO
{
	glm::mat4 modelMatrix;
	BYTE padding[192];
};

struct CameraUBO
{
	glm::mat4 projectionMatrix;
	glm::mat4 viewMatrix;
	glm::vec3 cameraPosition;	float padding0;
	glm::vec3 forwardVector;	float padding1;
	glm::vec3 upVector;			float padding2;
	glm::vec2 offset0;			glm::vec2 padding3;
};

struct IsotropicVoxelTexture
{
	uint32_t width, height, depth, mipcount, cascadecount;
	VkSampler sampler;
	VkSampler conetraceSampler;
	VkImage image;
	VkImageLayout imagelayout;
	VkImageView view[MAXGRIDMIP];
	VkDescriptorImageInfo descriptor[MAXGRIDMIP];
	VkFormat format;
	VkDeviceMemory deviceMemory;
};

struct AnisotropicVoxelTexture
{
	uint32_t				width, height, depth, mipcount, cascadecount;
	VkSampler				sampler;
	VkSampler				conetraceSampler;
	VkImage					image;
	VkImageLayout			imageLayout;
	VkImageView				view[MAXGRIDMIP];	// ordered as dir1mip0,dir1mip1,dir1mip2, dir2mip0,dir2mip1
	VkDescriptorImageInfo	descriptor[MAXGRIDMIP];
	VkFormat				format;
	VkDeviceMemory			deviceMemory;
	VkDeviceMemory			alphaDeviceMemory;
};

// Used for voxelization, and AA
struct VoxelizerGrid
{
	IsotropicVoxelTexture albedoOpacity;// RGBA8
	IsotropicVoxelTexture normal;		// RGBA8
	IsotropicVoxelTexture emission;		// RGBA8
};
// Used to sample indirect illumination
struct VoxelGrid
{
	AnisotropicVoxelTexture albedoOpacity;	// RGBA8
	AnisotropicVoxelTexture normal;			// RGBA8
	AnisotropicVoxelTexture emission;		// RGBA8
	AnisotropicVoxelTexture bufferPosition;	// RGBA8
	BufferObject surfacelist;				// cascade, axis, z,y,x 
};

struct DebugBox
{
	glm::vec3 position;	float padding0;	// Center position
	glm::vec3 size;		float padding1;	// Diagonal size
	glm::vec3 color;	float padding2;	// Color of outline
};

#endif	//DATATYPES_H
