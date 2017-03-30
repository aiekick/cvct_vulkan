#include "SceneTools.h"
#include "VKTools.h"
#include <glm\glm.hpp>
#include <float.h>
#include "VulkanCore.h"
#include "DataTypes.h"
#include "Shader.h"
#include "AnisotropicVoxelTexture.h"
#include "Defines.h"

void LoadAssetStaticManager(AssetManager& assetmanager, char* path, uint32_t pathLenght)
{
	assetmanager.LoadAsset(path, pathLenght);
}

Asset* GetAssetStaticManager(AssetManager& manager, char* path)
{
	Asset* asset = NULL;
	manager.GetAsset(path, &asset);
	if (!asset)
		printf("asset not loaded");
	return asset;
}

uint32_t CreateVulkanScenes(
	VulkanCore* core,
	VkCommandBuffer commandbuffer,
	VKScene* scenes, 
	uint32_t scenecount,
	PerSceneUBO* sceneubo)
{
	VkResult result;

	for (uint32_t i = 0; i < scenecount; i++)
	{
		VKScene& vkscene = scenes[i];
		Scene* scene = vkscene.scene;
		VkDevice device = core->GetViewDevice();

		// Set the per scene ubo
		sceneubo[i].modelMatrix = vkscene.modelMatrix;

		uint64_t totalBufferSize = scene->vertexDataSizeInBytes + scene->indexDataSizeInBytes;
		///////////////////////////////////////////////////////
		/////Create Vulkan specific buffers
		/////////////////////////////////////////////////////// 
		VkCommandBufferBeginInfo cmdBufInfo = {};
		cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		VK_CHECK_RESULT(vkBeginCommandBuffer(commandbuffer, &cmdBufInfo));

		// Create the device specific and staging buffer
		VkBufferCreateInfo bufferInfo = {};
		VkBuffer sceneStageBuffer;
		VkMemoryRequirements bufferMemoryRequirements, stagingMemoryRequirements;

		// Create the stagingd
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.pNext = NULL;
		bufferInfo.size = totalBufferSize;
		bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		bufferInfo.flags = 0;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		result = vkCreateBuffer(device, &bufferInfo, NULL, &sceneStageBuffer);

		// Create the device buffer
		bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
		result = vkCreateBuffer(device, &bufferInfo, NULL, &vkscene.scenebuffer);

		// Set memory requirements
		vkGetBufferMemoryRequirements(device, vkscene.scenebuffer, &bufferMemoryRequirements);
		vkGetBufferMemoryRequirements(device, sceneStageBuffer, &stagingMemoryRequirements);
		uint32_t bufferTypeIndex = VKTools::GetMemoryType(core, bufferMemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		uint32_t stagingTypeIndex = VKTools::GetMemoryType(core, stagingMemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

		if (!bufferTypeIndex || !stagingTypeIndex)
			RETURN_ERROR(-1, "No compatible memory type");

		// Creating the memory
		VkDeviceMemory stagingDeviceMemory;
		VkMemoryAllocateInfo allocInfo;

		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.pNext = NULL;
		allocInfo.allocationSize = bufferMemoryRequirements.size;
		allocInfo.memoryTypeIndex = bufferTypeIndex;
		result = vkAllocateMemory(device, &allocInfo, NULL, &vkscene.scenememory);
		if (result != VkResult::VK_SUCCESS)
			RETURN_ERROR(-1, "not able to allocate buffer device memory");

		allocInfo.allocationSize = stagingMemoryRequirements.size;
		allocInfo.memoryTypeIndex = stagingTypeIndex;
		result = vkAllocateMemory(device, &allocInfo, NULL, &stagingDeviceMemory);
		if (result != VkResult::VK_SUCCESS)
			RETURN_ERROR(-1, "not able to allocate staging device memory");

		// Bind all the memory
		result = vkBindBufferMemory(device, vkscene.scenebuffer, vkscene.scenememory, 0);
		if (result != VkResult::VK_SUCCESS)
			RETURN_ERROR(-1, "Not able to bind buffer with device memory");

		result = vkBindBufferMemory(device, sceneStageBuffer, stagingDeviceMemory, 0);
		if (result != VkResult::VK_SUCCESS)
			RETURN_ERROR(-1, "Not able to bind staging with device memory");

		// Map memory dest to pointer
		uint8_t* dst = NULL;
		result = vkMapMemory(device, stagingDeviceMemory, 0, totalBufferSize, 0, (void**)&dst);

		if (result != VK_SUCCESS)
			RETURN_ERROR(-1, "unable to map destination pointer");

		//copy the vertex and indices data to the destination
		memcpy(dst, scene->vertexData, scene->vertexDataSizeInBytes);
		memcpy(dst + scene->vertexDataSizeInBytes, scene->indexData, scene->indexDataSizeInBytes);
		vkUnmapMemory(device, stagingDeviceMemory);

		///////////////////////////////////////////////////////
		/////stage buffer to the GPU
		/////////////////////////////////////////////////////// 
		// Vertex buffer+Indices buffer
		VkBufferCopy bufferCopy = { 0,0, totalBufferSize };
		vkCmdCopyBuffer(commandbuffer, sceneStageBuffer, vkscene.scenebuffer, 1, &bufferCopy);

		VKTools::FlushCommandBuffer(commandbuffer, core->GetGraphicsQueue(), device, core->GetGraphicsCommandPool(), false);
	
		// Destroy staging buffers
		vkDestroyBuffer(device, sceneStageBuffer, NULL);
		vkFreeMemory(device, stagingDeviceMemory, NULL);

		///////////////////////////////////////////////////////
		/////Set bindings
		/////////////////////////////////////////////////////// 
		// todo: free this
		vkscene.vertices.bindingDescriptionCount = 5;
		vkscene.vertices.attributeDescriptionCount = 5;
		vkscene.vertices.bindingDescriptions = (VkVertexInputBindingDescription*)malloc(sizeof(VkVertexInputBindingDescription) * vkscene.vertices.bindingDescriptionCount);
		vkscene.vertices.attributeDescriptions = (VkVertexInputAttributeDescription*)malloc(sizeof(VkVertexInputAttributeDescription) * vkscene.vertices.attributeDescriptionCount);
		// Set binding description
		// Location 0 : Position
		vkscene.vertices.bindingDescriptions[0].binding = (uint32_t)ATTRIBUTE_POSITION;
		vkscene.vertices.bindingDescriptions[0].stride = 3 * sizeof(float);
		vkscene.vertices.bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		// Location 1 : Position
		vkscene.vertices.bindingDescriptions[1].binding = (uint32_t)ATTRIBUTE_TEXCOORD;
		vkscene.vertices.bindingDescriptions[1].stride = 2 * sizeof(float);
		vkscene.vertices.bindingDescriptions[1].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		// Location 2 : Position
		vkscene.vertices.bindingDescriptions[2].binding = (uint32_t)ATTRIBUTE_NORMAL;
		vkscene.vertices.bindingDescriptions[2].stride = 3 * sizeof(float);
		vkscene.vertices.bindingDescriptions[2].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		// Location 3 : Position
		vkscene.vertices.bindingDescriptions[3].binding = (uint32_t)ATTRIBUTE_TANGENT;
		vkscene.vertices.bindingDescriptions[3].stride = 3 * sizeof(float);
		vkscene.vertices.bindingDescriptions[3].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		// Location 4 : Position
		vkscene.vertices.bindingDescriptions[4].binding = (uint32_t)ATTRIBUTE_BITANGENT;
		vkscene.vertices.bindingDescriptions[4].stride = 3 * sizeof(float);
		vkscene.vertices.bindingDescriptions[4].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		// Attribute descriptions
		// Describes memory layout and shader attribute locations
		// Location 0 : Position
		vkscene.vertices.attributeDescriptions[0].binding = (uint32_t)ATTRIBUTE_POSITION;
		vkscene.vertices.attributeDescriptions[0].location = ATTRIBUTE_POSITION;
		vkscene.vertices.attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		vkscene.vertices.attributeDescriptions[0].offset = 0;
		// Location 1 : Texcoord
		vkscene.vertices.attributeDescriptions[1].binding = (uint32_t)ATTRIBUTE_TEXCOORD;
		vkscene.vertices.attributeDescriptions[1].location = ATTRIBUTE_TEXCOORD;
		vkscene.vertices.attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
		vkscene.vertices.attributeDescriptions[1].offset = 0;
		//location 2 : Normal
		vkscene.vertices.attributeDescriptions[2].binding = (uint32_t)ATTRIBUTE_NORMAL;
		vkscene.vertices.attributeDescriptions[2].location = ATTRIBUTE_NORMAL;
		vkscene.vertices.attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
		vkscene.vertices.attributeDescriptions[2].offset = 0;
		//location 3 : Tangent
		vkscene.vertices.attributeDescriptions[3].binding = (uint32_t)ATTRIBUTE_TANGENT;
		vkscene.vertices.attributeDescriptions[3].location = ATTRIBUTE_TANGENT;
		vkscene.vertices.attributeDescriptions[3].format = VK_FORMAT_R32G32B32_SFLOAT;
		vkscene.vertices.attributeDescriptions[3].offset = 0;
		//location 4 : Bitangent
		vkscene.vertices.attributeDescriptions[4].binding = (uint32_t)ATTRIBUTE_BITANGENT;
		vkscene.vertices.attributeDescriptions[4].location = ATTRIBUTE_BITANGENT;
		vkscene.vertices.attributeDescriptions[4].format = VK_FORMAT_R32G32B32_SFLOAT;
		vkscene.vertices.attributeDescriptions[4].offset = 0;

		// Assign to vertex input state
		vkscene.vertices.inputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vkscene.vertices.inputState.pNext = NULL;
		vkscene.vertices.inputState.flags = VK_FLAGS_NONE;
		vkscene.vertices.inputState.vertexBindingDescriptionCount = 5;
		vkscene.vertices.inputState.pVertexBindingDescriptions = vkscene.vertices.bindingDescriptions;
		vkscene.vertices.inputState.vertexAttributeDescriptionCount = 5;
		vkscene.vertices.inputState.pVertexAttributeDescriptions = vkscene.vertices.attributeDescriptions;

		vkscene.vertices.buf = vkscene.scenebuffer;
		vkscene.vertices.mem = vkscene.scenememory;
	}

	return 0;
}

uint32_t CreateIndirectDrawBuffer(
	VulkanCore* core,
	VkDrawIndexedIndirectCommand* indirectCommands,
	uint32_t buffersize,
	VKSubMesh* submeshes,
	uint32_t submeshcount)
{
	if(submeshcount > buffersize)
		RETURN_ERROR(-1, "buffer size too small");

	for (uint32_t i = 0; i < submeshcount; i++)
	{
		indirectCommands[i].indexCount = (uint32_t)submeshes[i].ibv.count;
	}

	return 0;
}

uint32_t CreateVulkanMeshes(
	VulkanCore* core,
	VkCommandBuffer commandbuffer,
	VKScene* scenes,
	uint32_t scenecount,
	VKMesh* meshes,
	uint32_t& meshcount,
	VKSubMesh* submeshes,
	uint32_t& submeshcount,
	PerSubMeshUBO* submeshubo)
{
	//VkResult result;
	uint32_t textureoffset = 0;
	uint32_t meshoffset = 0;
	uint32_t meshNum = 0;
	uint32_t submeshoffset = 0;
	PerSubMeshUBO* submesh = submeshubo;
	VKSubMesh* vksubmesh = submeshes;

	for (uint32_t i = 0; i < scenecount; i++)
	{
		VKScene& vkscene = scenes[i];
		Scene* scene = vkscene.scene;
		VkDevice device = core->GetViewDevice();

		vkscene.vkmeshCount = scene->meshCount;
		vkscene.vkmeshes = meshes + meshoffset;
		meshoffset += vkscene.vkmeshCount;

		///////////////////////////////////////////////////////
		/////create the vulkan meshes
		/////////////////////////////////////////////////////// 
		//set up all the meshes
		//vk_mesh_s* mesh = m_meshes;
		
		meshcount += scene->meshCount;
		for (uint32_t r = 0; r < scene->modelReferenceCount; r++)
		{
			Modelref* modelRef = &scene->modelRefs[r];
			Model* model = &scene->models[modelRef->modelIndex];
			for (uint32_t m = 0; m < model->meshCount; m++)
			{
				VKMesh meshData = {};
				Mesh* mesh = &scene->meshes[model->meshStartIndex + m];

				// The number of materialindexcount is the same as the number of submeshes
				for (uint32_t k = 0; k < modelRef->materialIndexCount; k++)
				{
					Material* material = &scene->materials[modelRef->materialIndices[k]];
					VKSubMesh vksubmeshData = {};
					for (uint32_t j = 0; j < TextureIndex::TEXTURE_COUNT; j++)
						vksubmeshData.textureIndex[j] = INVALID_TEXTURE;

					for (uint32_t t = 0; t < material->textureReferenceCount; t++)
					{
						uint32_t reffer = material->textureReferenceStart + t;
						if (reffer > 35)
							int asda = 0;
						Textureref* textureRef = &scene->textureRefs[material->textureReferenceStart + t];
						const char* attrib = scene->stringData + textureRef->attribOffset;

						uint32_t flag = 0;
						if (strcmp(attrib, "diffuse") == 0)
							flag |= DIFFUSE_TEXTURE, vksubmeshData.textureIndex[DIFFUSE_TEXTURE] = textureoffset + material->textureReferenceStart + t;
						else if (strcmp(attrib, "normal") == 0)
							flag |= NORMAL_TEXTURE, vksubmeshData.textureIndex[NORMAL_TEXTURE] = textureoffset + material->textureReferenceStart + t;
						else if (strcmp(attrib, "opacity") == 0)
							flag |= OPACITY_TEXTURE, vksubmeshData.textureIndex[OPACITY_TEXTURE] = textureoffset + material->textureReferenceStart + t;
						else if (strcmp(attrib, "emission") == 0)
							flag |= EMISSION_TEXTURE, vksubmeshData.textureIndex[EMISSION_TEXTURE] = textureoffset + material->textureReferenceStart + t;
					}

					PerSubMeshUBO submeshubo = {
						vksubmeshData.textureIndex[DIFFUSE_TEXTURE],
						vksubmeshData.textureIndex[NORMAL_TEXTURE],
						vksubmeshData.textureIndex[OPACITY_TEXTURE],
						vksubmeshData.textureIndex[EMISSION_TEXTURE],
						0
					};

					// Increment the submesh ubo
					*submesh = submeshubo;
					submesh++;

					// Increment the submesh
					*vksubmesh = vksubmeshData;
					vksubmesh++;
				}

				//assign vertex id to meshes
				if (mesh->vertexBufferCount > 8)
					RETURN_ERROR(-1, "Number of vertex buffers is too high");
				if (mesh->indexBufferCount > 4)
					RETURN_ERROR(-1, "Numer of submeshes is too high");

				meshData.vbvCount = mesh->vertexBufferCount;
				meshData.submeshCount = mesh->indexBufferCount;
				meshData.vertexCount = mesh->vertexCount;
				meshData.submeshes = submeshes + submeshoffset;

				for (uint32_t j = 0; j < mesh->vertexBufferCount; j++)
				{
					Vertexbuffer* vb = &scene->vertexBuffers[mesh->vertexBufferStartIndex + j];
					uint32_t idx = 0xFFFFFFF;
					const char* attrib = scene->stringData + vb->attribStringOffset;

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
						meshData.vertexResources[idx] = vkscene.scenebuffer;
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
					Indexbuffer* ib = &scene->indexBuffers[mesh->indexBufferStartIndex + j];

					vk_ib_s vkib;
					vkib.buffer = vkscene.scenebuffer;
					vkib.format = (ib->indexByteSize == 4) ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16;
					vkib.offset = scene->vertexDataSizeInBytes + ib->indexOffset;
					vkib.count = ib->indexCount;

					meshData.submeshes[j].ibv = vkib;
					meshData.submeshes[j].indexCount = ib->indexCount;

					submeshoffset++;
				}
				submeshcount += mesh->indexBufferCount;
				meshes[meshNum++] = meshData;
			}
		}
		textureoffset += scene->textureCount;
	}
	return 0;	//everything is uploaded
}

uint32_t UpdateDescriptorSets(
	VulkanCore* core,
	VKScene* scenes,
	uint32_t scenecount,
	VKMesh* meshes,
	uint32_t& meshcount,
	VkDescriptorSet staticdescriptorset,
	VkDescriptorSet* texturedescriptorset)
{
	//VkResult result;
	uint32_t textureoffset = 0;
	uint32_t meshoffset = 0;
	uint32_t submeshNum = 0;

	for (uint32_t i = 0; i < scenecount; i++)
	{
		VKScene& vkscene = scenes[i];
		Scene* scene = vkscene.scene;
		VkDevice device = core->GetViewDevice();

		vkscene.vkmeshCount = scene->meshCount;
		vkscene.vkmeshes = meshes + meshoffset;
		meshoffset += vkscene.vkmeshCount;

		///////////////////////////////////////////////////////
		/////create the vulkan meshes
		/////////////////////////////////////////////////////// 
		//set up all the meshes
		//vk_mesh_s* mesh = m_meshes;

		for (uint32_t r = 0; r < scene->modelReferenceCount; r++)
		{
			Modelref* modelRef = &scene->modelRefs[r];
			Model* model = &scene->models[modelRef->modelIndex];
			for (uint32_t m = 0; m < model->meshCount; m++)
			{
				Mesh* mesh = &scene->meshes[model->meshStartIndex + m];

				// The number of materialindexcount is the same as the number of submeshes
				for (uint32_t k = 0; k < modelRef->materialIndexCount; k++)
				{
					Material* material = &scene->materials[modelRef->materialIndices[k]];

					for (uint32_t t = 0; t < material->textureReferenceCount; t++)
					{
						Textureref* textureRef = &scene->textureRefs[material->textureReferenceStart + t];
						const char* attrib = scene->stringData + textureRef->attribOffset;
						uint32_t binding = INVALID_TEXTURE;

						if		(strcmp(attrib, "diffuse") == 0)	binding = 0;
						else if (strcmp(attrib, "normal") == 0)		binding = 1;
						else if (strcmp(attrib, "opacity") == 0)	binding = 2;
						else if (strcmp(attrib, "emission") == 0)	binding = 3;

						uint32_t element = vkscene.textureOffset + material->textureReferenceStart + t;

						if (submeshNum > 256)
							int asda = 0;

						if (binding != INVALID_TEXTURE)
						{
							VkCopyDescriptorSet cds = {};
							cds.sType = VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET;
							cds.srcSet = staticdescriptorset;
							cds.srcBinding = STATIC_DESCRIPTOR_IMAGE;
							cds.srcArrayElement = vkscene.textureOffset + material->textureReferenceStart + t;
							cds.dstSet = texturedescriptorset[submeshNum];
							cds.dstBinding = binding;
							cds.dstArrayElement = 0;
							cds.descriptorCount = 1;
							vkUpdateDescriptorSets(device, 0, NULL, 1, &cds);
						}
					}
					submeshNum++;
				}
			}
		}
		textureoffset += scene->textureCount;
	}
	return 0;
}

uint32_t CreateVulkanTextures(
	VulkanCore* core,
	VkCommandBuffer commandbuffer,
	AssetManager& assetmanager,
	VKScene* scenes,
	uint32_t scenecount,
	VKTexture* textures,
	uint32_t& texturecount,
	VkSampler sampler,
	VkDescriptorSet staticdescriptorset)
{
	if (!scenecount)	RETURN_ERROR(-1, "Textures trying to load before scene is assigned");
	VkDevice device = core->GetViewDevice();

	VkCommandBufferBeginInfo cmdBufInfo = {};
	cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	VK_CHECK_RESULT(vkBeginCommandBuffer(commandbuffer, &cmdBufInfo));

	for (uint32_t i = 0; i < scenecount; i++)
	{
		VKScene& vkscene = scenes[i];
		scenes->textureOffset = 0;
		texturecount += scenes[i].scene->textureCount;
		Scene* scene = scenes[i].scene;
		int32_t high = -1;		//TODO: UGLY, FIX LATER
								// Calculate/assign texture ID to meshes
		for (uint32_t r = 0; r < scene->modelReferenceCount; r++)
		{
			Modelref* modelRef = &scene->modelRefs[r];
			for (uint32_t k = 0; k < modelRef->materialIndexCount; k++)
			{
				Material* material = &scene->materials[modelRef->materialIndices[k]];
				for (uint32_t t = 0; t < material->textureReferenceCount; t++)
				{
					Textureref* textureRef = &scene->textureRefs[material->textureReferenceStart + t];
					Texture* texture = &scene->textures[textureRef->textureIndex];

					char totallPath[512];
					const char* path = scene->stringData + texture->pathOffset;
					strncpy(totallPath, ASSETPATH, sizeof(totallPath));
					strncat(totallPath, path, sizeof(totallPath));

					if (int32_t(material->textureReferenceStart + t) > high)
					{
						high = material->textureReferenceStart + t;
						CreateTexture(core, assetmanager,commandbuffer, staticdescriptorset, sampler, textures, texturecount, vkscene.textureOffset + material->textureReferenceStart + t, totallPath);
					}
				}
			}
		}
	}
	VKTools::FlushCommandBuffer(commandbuffer, core->GetGraphicsQueue(), core->GetViewDevice(), core->GetGraphicsCommandPool(), false);

	return 0;
}

uint32_t CreateTexture(
	VulkanCore* core,
	AssetManager& assetmanager,
	VkCommandBuffer commandbuffer,
	VkDescriptorSet descriptorset,
	VkSampler sampler,
	VKTexture* textures,
	uint32_t texturecount,
	uint32_t index,
	const char* path)
{
	if (index >= MAXTEXTURES)
		RETURN_ERROR(-1, "Number of textures exceed limit");

	VkResult result;
	Asset* image;
	image = GetAssetStaticManager(assetmanager,(char*)path);
	VkDevice device = core->GetViewDevice();

	ImageDesc* imageDesc = (ImageDesc*)image->data;
	if (!imageDesc)
		RETURN_ERROR(-1, "Image could not find between the descriptors");

	uint8_t* pixelData = (uint8_t*)(imageDesc->mips + imageDesc->mipCount);

	uint64_t pixelSize = 0;
	for (uint32_t i = 0; i < imageDesc->mipCount; i++)
		pixelSize += imageDesc->mips[i].width * imageDesc->mips[i].height * sizeof(uint32_t);	//size of one pixel is 4 bytes

	//////////////////////////////////////////////////
	//// Set the VK Texture
	///////////////////////////////////////////////// 
	VKTexture& vktexture = textures[index];
	vktexture = {};
	vktexture.width = imageDesc->width;
	vktexture.height = imageDesc->height;
	vktexture.mipCount = (uint32_t)floor(log2(glm::max(imageDesc->width, imageDesc->height))) + 1;
	vktexture.descriptorSetCount = (uint32_t)ceil((float)(vktexture.mipCount - 1) / 4);
	vktexture.sampler = sampler;
	// TODO: THIS MIGHT ERROR
	vktexture.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	vktexture.view = (VkImageView*)malloc(sizeof(VkImageView) * vktexture.mipCount);
	vktexture.descriptor = (VkDescriptorImageInfo*)malloc(sizeof(VkDescriptorImageInfo) * vktexture.mipCount);

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
	result = vkCreateBuffer(device, &bufferInfo, NULL, &stagingBuffer);

	VkImageCreateInfo imageInfo = {};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.pNext = NULL;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	//imageInfo.mipLevels = imageDesc->mipCount;
	imageInfo.mipLevels = vktexture.mipCount;
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
	result = vkCreateImage(device, &imageInfo, NULL, &vktexture.image);
	if (result != VkResult::VK_SUCCESS)
		RETURN_ERROR(-1, "vkCreateImage failed (0x%08X)", (uint32_t)result);

	VkMemoryRequirements imageMemoryRequirements, stagingMemoryRequirements;
	vkGetImageMemoryRequirements(device, vktexture.image, &imageMemoryRequirements);
	vkGetBufferMemoryRequirements(device, stagingBuffer, &stagingMemoryRequirements);
	uint32_t imageTypeIndex = VKTools::GetMemoryType(core, imageMemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	uint32_t stagingTypeIndex = VKTools::GetMemoryType(core, stagingMemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

	if (!imageTypeIndex || !stagingTypeIndex)
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
	result = vkAllocateMemory(device, &allocateInfo, NULL, &imageDeviceMemory);
	if (result != VK_SUCCESS)
		RETURN_ERROR(-1, "vkAllocateMemory failed (0x%08X)", (uint32_t)result);

	allocateInfo.allocationSize = stagingMemoryRequirements.size;
	allocateInfo.memoryTypeIndex = stagingTypeIndex;
	result = vkAllocateMemory(device, &allocateInfo, NULL, &stagingDeviceMemory);
	if (result != VK_SUCCESS)
		RETURN_ERROR(-1, "vkAllocateMemory failed (0x%08X)", (uint32_t)result);

	//////////////////////////////////////////////////
	//// Bind memory
	/////////////////////////////////////////////////
	result = vkBindImageMemory(device, vktexture.image, imageDeviceMemory, 0);
	if (result != VK_SUCCESS)	RETURN_ERROR(-1, "vkBindMemory failed(0x%08X)", (uint32_t)result);

	result = vkBindBufferMemory(device, stagingBuffer, stagingDeviceMemory, 0);
	if (result != VK_SUCCESS)	RETURN_ERROR(-1, "vkBindMemory failed(0x%08X)", (uint32_t)result);

	//////////////////////////////////////////////////
	//// Map memory/copy memory
	/////////////////////////////////////////////////
	uint8_t* dst = NULL;
	result = vkMapMemory(device, stagingDeviceMemory, 0, pixelSize, 0, (void**)&dst);
	memcpy(dst, pixelData, pixelSize);
	vkUnmapMemory(device, stagingDeviceMemory);

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
		commandbuffer,
		vktexture.image,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		srRange);

	vkCmdCopyBufferToImage(commandbuffer, stagingBuffer, vktexture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, imageDesc->mipCount, mipCopies);

	// Change texture image layout to shader read after all mip levels have been copied
	VKTools::SetImageLayout(
		commandbuffer,
		vktexture.image,
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
	viewCreateInfo.image = vktexture.image;
	viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewCreateInfo.subresourceRange.baseArrayLayer = 0;
	viewCreateInfo.subresourceRange.layerCount = 1;
	// create the imageview
	for (uint32_t i = 0; i < vktexture.mipCount; i++)
	{
		viewCreateInfo.subresourceRange.baseMipLevel = i;
		viewCreateInfo.subresourceRange.levelCount = vktexture.mipCount - i;
		result = vkCreateImageView(device, &viewCreateInfo, NULL, &vktexture.view[i]);
		if (result != VK_SUCCESS)
			RETURN_ERROR(-1, "vkcreateImageView Failed (0x%08X)", (uint32_t)result);

		//////////////////////////////////////////////////
		//// Update the descriptorset. bind image
		/////////////////////////////////////////////////
		vktexture.descriptor[i].sampler = vktexture.sampler;
		vktexture.descriptor[i].imageView = vktexture.view[i];
		vktexture.descriptor[i].imageLayout = vktexture.imageLayout;
	}

	// Create the uniform buffers per descriptorsetCount
	vktexture.ubo = (TextureMipMapperUBOComp*)malloc(sizeof(TextureMipMapperUBOComp) * vktexture.descriptorSetCount);
	vktexture.boDescriptor = (BufferObject*)malloc(sizeof(BufferObject) * vktexture.descriptorSetCount);
	uint32_t mipcount = vktexture.mipCount - 1;
	int32_t mipRemainder = mipcount;
	for (uint32_t i = 0; i < vktexture.descriptorSetCount; i++)
	{
		// Create the uniform data
		uint32_t base = mipcount - mipRemainder;
		TextureMipMapperUBOComp ubo;
		ubo.srcMipLevel = base;
		ubo.numMipLevels = glm::min(4, mipRemainder);
		float msize = (float)(glm::max(vktexture.width, vktexture.height) * glm::pow(0.5f, base + 1));
		ubo.texelSize = glm::vec2(1.0f / msize);
		vktexture.ubo[i] = ubo;

		mipRemainder -= 4;
		glm::abs(mipRemainder);

		// Create the descriptor data
		VKTools::CreateBuffer(core, device,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			sizeof(TextureMipMapperUBOComp),
			NULL,
			&vktexture.boDescriptor[i].buffer,
			&vktexture.boDescriptor[i].memory,
			&vktexture.boDescriptor[i].descriptor);

		// Mapping Forward renderer
		uint8_t *pData;
		VK_CHECK_RESULT(vkMapMemory(device, vktexture.boDescriptor[i].memory, 0, sizeof(TextureMipMapperUBOComp), 0, (void**)&pData));
		memcpy(pData, &ubo, sizeof(TextureMipMapperUBOComp));
		vkUnmapMemory(device, vktexture.boDescriptor[i].memory);
	}

	VkWriteDescriptorSet wds = VKTools::Initializers::WriteDescriptorSet(
		descriptorset,
		VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
		STATIC_DESCRIPTOR_IMAGE,
		&vktexture.descriptor[0]);
	wds.dstArrayElement = index;

	vkUpdateDescriptorSets(device, 1, &wds, 0, NULL);

	return 0;
}

uint32_t CreateSamplers(VkDevice device, VkDescriptorSet descriptorset, VkSampler& sampler)
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
	result = vkCreateSampler(device, &sci, NULL, &sampler);
	if (result != VK_SUCCESS)
		RETURN_ERROR(-1, "Failed to create sampler");

	// Create the texture sampler
	VkDescriptorImageInfo dii = {};
	dii.sampler = sampler;
	VkWriteDescriptorSet wds = VKTools::Initializers::WriteDescriptorSet(
		descriptorset,
		VK_DESCRIPTOR_TYPE_SAMPLER,
		STATIC_DESCRIPTOR_SAMPLER,
		(VkDescriptorBufferInfo*)NULL);
	wds.pImageInfo = &dii;
	vkUpdateDescriptorSets(device, 1, &wds, 0, NULL);

	return 0;
}

void LoadScene(AssetManager& assetmanager, VKScene* scenes, uint32_t& scenecount, const char* path, uint32_t pathLength, glm::mat4& model)
{
	if (assetmanager.LoadAsset(path, pathLength) == 0)
	{
		scenes[scenecount].scene = (Scene*)GetAssetStaticManager(assetmanager, (char*)path)->data;
		scenes[scenecount].modelMatrix = model;
		scenecount++;
	}
}

uint32_t BuildCommandMip(
	VulkanCore* core,
	VKTexture* textures,
	uint32_t texturecount)
{
	VkDevice device = core->GetViewDevice();

	uint32_t totalDescriptorSetCount = 0;
	for (uint32_t i = 0; i < texturecount; i++)
		totalDescriptorSetCount += textures[i].descriptorSetCount;

	////////////////////////////////////////////////////////////////////////////////
	// Create commandbuffers and semaphores
	////////////////////////////////////////////////////////////////////////////////
	// Create the semaphore
	VkSemaphore mipSemaphore;
	VkSemaphoreCreateInfo semInfo = VKTools::Initializers::SemaphoreCreateInfo();
	vkCreateSemaphore(device, &semInfo, NULL, &mipSemaphore);
	// Create the command buffer
	VkCommandBuffer mipCommandBuffer = VKTools::Initializers::CreateCommandBuffer(core->GetComputeCommandPool(), device, VK_COMMAND_BUFFER_LEVEL_PRIMARY, false);
	// Uniform data
	BufferObject uniformData;
	VKTools::CreateBuffer(core, device,
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		sizeof(TextureMipMapperUBOComp),
		NULL,
		&uniformData.buffer,
		&uniformData.memory,
		&uniformData.descriptor);

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
	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayoutCreateInfo, NULL, &layout));

	////////////////////////////////////////////////////////////////////////////////
	// Create pipeline layout
	////////////////////////////////////////////////////////////////////////////////
	VkPipelineLayout pipelineLayout;
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = VKTools::Initializers::PipelineLayoutCreateInfo(0, 1, &layout);
	VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));

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
	VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, NULL, &descriptorPool));

	////////////////////////////////////////////////////////////////////////////////
	// Create the descriptor set
	////////////////////////////////////////////////////////////////////////////////
	VkDescriptorSet descriptorSet[MAXTEXTURES * 10];
	VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = VKTools::Initializers::DescriptorSetAllocateInfo(descriptorPool, 1, &layout);
	for (uint32_t i = 0; i < totalDescriptorSetCount; i++)
	{
		//allocate the descriptorset with the pool
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &descriptorSet[i]));
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
	shaderStage = VKTools::LoadShader("shaders/texturemipmapper.comp.spv", "main", device, VK_SHADER_STAGE_COMPUTE_BIT);
	computePipelineCreateInfo.stage = shaderStage.m_shaderStage;

	VK_CHECK_RESULT(vkCreateComputePipelines(device, core->GetPipelineCache(), 1, &computePipelineCreateInfo, nullptr, &mipPipeline));

	////////////////////////////////////////////////////////////////////////////////
	// Build command buffers
	////////////////////////////////////////////////////////////////////////////////
	VkCommandBufferBeginInfo cmdBufInfo = {};
	cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBufInfo.pNext = NULL;

	VK_CHECK_RESULT(vkBeginCommandBuffer(mipCommandBuffer, &cmdBufInfo));
	vkCmdBindPipeline(mipCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, mipPipeline);

	uint32_t desCount = 0;
	for (uint32_t i = 0; i < texturecount; i++)
	{
		VKTexture* tex = &textures[i];
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
			vkUpdateDescriptorSets(device, 1, &wds, 0, NULL);
			// Bind the mipmaps
			wds.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			wds.pNext = NULL;
			wds.dstSet = descriptorSet[desCount];
			wds.dstBinding = 1;
			wds.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			wds.descriptorCount = 4;//glm::min(4, mipRemainder);
			wds.dstArrayElement = 0;
			wds.pImageInfo = ii;
			vkUpdateDescriptorSets(device, 1, &wds, 0, NULL);
			// Bind the mipmaps
			wds.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			wds.pNext = NULL;
			wds.dstSet = descriptorSet[desCount];
			wds.dstBinding = 2;
			wds.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			wds.descriptorCount = 1;
			wds.dstArrayElement = 0;
			wds.pImageInfo = NULL;
			wds.pBufferInfo = &tex->boDescriptor[j].descriptor;
			wds.pTexelBufferView = NULL;
			vkUpdateDescriptorSets(device, 1, &wds, 0, NULL);

			vkCmdBindDescriptorSets(mipCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet[desCount], 0, 0);

			float numdis = (float)glm::ceil((glm::max(tex->width, tex->height) * glm::pow(0.5f, tex->ubo[j].srcMipLevel + 1)) / 8);
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

	VK_CHECK_RESULT(vkQueueSubmit(core->GetComputeQueue(), 1, &submit, VK_NULL_HANDLE));
	vkDeviceWaitIdle(device);

	return 0;
}

uint32_t CreateAABB(
	VKScene* scenes,
	uint32_t scenecount,
	VKMesh* meshes,
	uint32_t meshcount,
	InstanceDataAABB* aabb)
{
	uint32_t offset = 0;
	InstanceDataAABB* caabb = aabb;
	for (uint32_t i = 0; i < scenecount; i++)
	{
		VKScene* vkscene = &scenes[i];
		glm::mat4 model = vkscene->modelMatrix;
		uint8_t* startdata = vkscene->scene->vertexData;
		uint8_t* indexdata = vkscene->scene->vertexData;	// Offset is stored from the start of the data
		
		for (uint32_t m = offset; m < vkscene->scene->meshCount + offset; m++)
		{
			VKMesh* mesh = &meshes[m];		

			for (uint32_t sm = 0; sm < mesh->submeshCount; sm++)
			{
				glm::vec3 min = glm::vec3(FLT_MAX);
				glm::vec3 max = glm::vec3(-FLT_MAX);
				VKSubMesh* submesh = &mesh->submeshes[sm];
				uint8_t* ind = indexdata + submesh->ibv.offset;
				uint8_t* vertexStartData = startdata + mesh->vertexOffsets[ATTRIBUTE_POSITION];
				for (uint32_t index = 0; index < submesh->ibv.count; index++)
				{
					uint32_t formatsize = (submesh->ibv.format == VK_INDEX_TYPE_UINT32) ? 4 : 2;

					uint32_t idx = {};
					glm::vec3 vertexPos = {};
					memcpy(&idx, ind + (index * formatsize), formatsize);
					memcpy(&vertexPos, vertexStartData + (sizeof(float) * 3 * idx), sizeof(float) * 3);

					max = glm::max(max, vertexPos);
					min = glm::min(min, vertexPos);
				}
				min = glm::vec3((model * glm::vec4(min, 1)));
				max = glm::vec3((model * glm::vec4(max, 1)));
				glm::vec3 diff = max - min;

				InstanceDataAABB taabb;
				taabb.position = min + (diff * 0.5f);
				taabb.size = diff;
				*caabb = taabb;

				caabb++;
			}
		}
		offset += vkscene->scene->meshCount;
	}
	return 0;
}

void CreateVulkanBuffers(
	VulkanCore* core, 
	BufferObject& persceneBO, 
	BufferObject& persubmeshBO,
	BufferObject& cameraBO,
	VKScene* scene, 
	uint32_t sceneCount)
{
	VkDevice device = core->GetViewDevice();

	// Create the static buffer
	VKTools::CreateBuffer(core, device,
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		sizeof(PerSceneUBO) * sceneCount,
		NULL,
		&persceneBO.buffer,
		&persceneBO.memory,
		&persceneBO.descriptor);
	persceneBO.descriptor.range = VK_WHOLE_SIZE;

	// Create the dynamic buffer
	uint32_t submeshcount = {};
	for (uint32_t i = 0; i < sceneCount; i++)
	{
		for (uint32_t j = 0; j < scene[i].scene->meshCount; j++)
		{
			submeshcount += scene[i].scene->meshes[j].indexBufferCount;	
		}
	}	

	VKTools::CreateBuffer(core, device,
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		sizeof(PerSubMeshUBO) * submeshcount,
		NULL,
		&persubmeshBO.buffer,
		&persubmeshBO.memory,
		&persubmeshBO.descriptor);
	persubmeshBO.descriptor.range = VK_WHOLE_SIZE;

	// Create the camera uniform buffer
	VKTools::CreateBuffer(core, device,
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		sizeof(CameraUBO),
		NULL,
		&cameraBO.buffer,
		&cameraBO.memory,
		&cameraBO.descriptor);
}

void CreateDescriptor(
	VulkanCore* core,
	uint32_t textureDescriptorsetCount,
	VkDescriptorSetLayout& staticdescriptorlayout,
	VkDescriptorSetLayout& texturedescriptorlayout,
	VkDescriptorPool& staticpool,
	VkDescriptorPool& texturepool,
	VkDescriptorSet& staticdescriptorset,
	VkDescriptorSet* texturedescriptorset,
	BufferObject& persceneBO,
	BufferObject& permeshBO,
	BufferObject& cameraBO)
{
	VkDevice device = core->GetViewDevice();

	///////////////////////////////////////////////////////
	/////static and dynamic descriptorlayout
	/////////////////////////////////////////////////////// 
	// Static descriptorset
	VkDescriptorSetLayoutBinding staticLayout[STATIC_DESCRIPTOR_COUNT];
	// Binding 0 : Uniform buffer (Vertex shader) ( MVP matrix uniforms ) Per scene
	staticLayout[STATIC_DESCRIPTOR_PERSCENE_BUFFER] =
	{ STATIC_DESCRIPTOR_PERSCENE_BUFFER, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT, NULL };
	// Binding 1 : image samplers
	staticLayout[STATIC_DESCRIPTOR_SAMPLER] =
	{ STATIC_DESCRIPTOR_SAMPLER, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL };
	// Binding 2: image descriptor sampler
	staticLayout[STATIC_DESCRIPTOR_IMAGE] =
	{ STATIC_DESCRIPTOR_IMAGE, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, MAXTEXTURES, VK_SHADER_STAGE_FRAGMENT_BIT, NULL };
	// Binding 3 : Uniform buffer (Vertex shader) ( MVP matrix uniforms )
	staticLayout[STATIC_DESCRIPTOR_PERMESH_BUFFER] =
	{ STATIC_DESCRIPTOR_PERMESH_BUFFER, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL };
	// Binding 4 : Uniform buffer (Vertex shader) ( MVP matrix uniforms )
	staticLayout[STATIC_DESCRIPTOR_CAMERA_BUFFER] =
	{ STATIC_DESCRIPTOR_CAMERA_BUFFER, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT, NULL };
	VkDescriptorSetLayoutCreateInfo staticDescriptorLayout = VKTools::Initializers::DescriptorSetLayoutCreateInfo(0, STATIC_DESCRIPTOR_COUNT, staticLayout);
	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &staticDescriptorLayout, NULL, &staticdescriptorlayout));

	///////////////////////////////////////////////////////
	/////static and dynamic descriptorlayout
	/////////////////////////////////////////////////////// 
	// Static descriptorset
	VkDescriptorSetLayoutBinding texturelayout[MESHTEXTURE_DESCRIPTOR_COUNT];
	// Binding 0 : diffuse
	texturelayout[MESHTEXTURE_DESCRIPTOR_DIFFUSE] =
	{ MESHTEXTURE_DESCRIPTOR_DIFFUSE, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL };
	// Binding 1 : normal
	texturelayout[MESHTEXTURE_DESCRIPTOR_NORMAL] =
	{ MESHTEXTURE_DESCRIPTOR_NORMAL, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL };
	// Binding 2: opacity
	texturelayout[MESHTEXTURE_DESCRIPTOR_OPACITY] =
	{ MESHTEXTURE_DESCRIPTOR_OPACITY, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL };
	// Binding 3 : emission
	texturelayout[MESHTEXTURE_DESCRIPTOR_EMISSION] =
	{ MESHTEXTURE_DESCRIPTOR_EMISSION, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL };
	VkDescriptorSetLayoutCreateInfo textureDescriptorLayout = VKTools::Initializers::DescriptorSetLayoutCreateInfo(0, MESHTEXTURE_DESCRIPTOR_COUNT, texturelayout);
	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &textureDescriptorLayout, NULL, &texturedescriptorlayout));

	///////////////////////////////////////////////////////
	///// Create Descriptor Pool
	/////////////////////////////////////////////////////// 
	// Static Descriptor Pool
	VkDescriptorPoolSize staticPoolSize[STATIC_DESCRIPTOR_COUNT];
	staticPoolSize[STATIC_DESCRIPTOR_PERSCENE_BUFFER] = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC , 1 };
	staticPoolSize[STATIC_DESCRIPTOR_SAMPLER] = { VK_DESCRIPTOR_TYPE_SAMPLER , 1 };
	staticPoolSize[STATIC_DESCRIPTOR_IMAGE] = { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE , MAXTEXTURES };
	staticPoolSize[STATIC_DESCRIPTOR_PERMESH_BUFFER] = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC , 1 };
	staticPoolSize[STATIC_DESCRIPTOR_CAMERA_BUFFER] = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER , 1 };
	VkDescriptorPoolCreateInfo staticDescriptorCreateInfo = VKTools::Initializers::DescriptorPoolCreateInfo(
		0, 1, STATIC_DESCRIPTOR_COUNT, staticPoolSize);
	VK_CHECK_RESULT(vkCreateDescriptorPool(device, &staticDescriptorCreateInfo, nullptr, &staticpool));

	// Texture Descriptor Pool
	VkDescriptorPoolSize texturePoolSize[1];
	texturePoolSize[0] = { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE , textureDescriptorsetCount * MESHTEXTURE_DESCRIPTOR_COUNT};
	VkDescriptorPoolCreateInfo textureDescriptorCreateInfo = VKTools::Initializers::DescriptorPoolCreateInfo(
		0, textureDescriptorsetCount, 1, texturePoolSize);
	VK_CHECK_RESULT(vkCreateDescriptorPool(device, &textureDescriptorCreateInfo, nullptr, &texturepool));
	///////////////////////////////////////////////////////
	///// Create descriptorset
	/////////////////////////////////////////////////////// 
	// Create the static descriptorset
	VkDescriptorSetAllocateInfo staticDescriptorAllocInfo = VKTools::Initializers::DescriptorSetAllocateInfo(staticpool, 1, &staticdescriptorlayout);
	VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &staticDescriptorAllocInfo, &staticdescriptorset));
	for (uint32_t i = 0; i < textureDescriptorsetCount; i++)
	{
		// Create the texture descriptorset
		VkDescriptorSetAllocateInfo textureDescriptorAllocInfo = VKTools::Initializers::DescriptorSetAllocateInfo(texturepool, 1, &texturedescriptorlayout);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &textureDescriptorAllocInfo, &texturedescriptorset[i]));
	}
	///////////////////////////////////////////////////////
	///// Set the vulkan buffers
	/////////////////////////////////////////////////////// 
	VkWriteDescriptorSet wds[3];
	wds[0] = VKTools::Initializers::WriteDescriptorSet(
		staticdescriptorset,
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
		STATIC_DESCRIPTOR_PERSCENE_BUFFER,
		&persceneBO.descriptor);
	wds[1] = VKTools::Initializers::WriteDescriptorSet(
		staticdescriptorset,
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
		STATIC_DESCRIPTOR_PERMESH_BUFFER,
		&permeshBO.descriptor);
	wds[2] = VKTools::Initializers::WriteDescriptorSet(
		staticdescriptorset,
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		STATIC_DESCRIPTOR_CAMERA_BUFFER,
		&cameraBO.descriptor);
	vkUpdateDescriptorSets(device, 3, wds, 0, NULL);
}

uint32_t UploadVulkanBuffers(
	VulkanCore* core,
	BufferObject& persceneBO,
	PerSceneUBO* perscene,
	uint32_t scenecount,
	BufferObject& persubmeshBO,
	PerSubMeshUBO* persubmesh,
	uint32_t submeshcount
)
{
	VkDevice device = core->GetViewDevice();

	void *mapped;
	persceneBO.allocSize = sizeof(PerSceneUBO) * scenecount;
	VK_CHECK_RESULT(vkMapMemory(device, persceneBO.memory, 0, persceneBO.allocSize, 0, &mapped));
	memcpy(mapped, perscene, persceneBO.allocSize);
	vkUnmapMemory(device, persceneBO.memory);

	persubmeshBO.allocSize = sizeof(PerSubMeshUBO) * submeshcount;
	VK_CHECK_RESULT(vkMapMemory(device, persubmeshBO.memory, 0, persubmeshBO.allocSize, 0, &mapped));
	memcpy(mapped, persubmesh, persubmeshBO.allocSize);
	vkUnmapMemory(device, persubmeshBO.memory);

	return 0;
}

int32_t CreateVoxelSamplers(
	VulkanCore& core,
	VkSampler& linear,
	VkSampler& point)
{
	CreateLinearSampler(core, linear);
	CreatePointSampler(core, point);
	return 0;
}

int32_t CreateVoxelTextures(
	VulkanCore& core,
	uint32_t gridresolution,
	uint32_t axisscalar,
	VoxelizerGrid* voxelizergrid,
	uint32_t voxelizergridcount,
	VoxelGrid& voxelgrid,
	uint32_t mipcount,
	uint32_t cascadecount)
{
	VkDevice device = core.GetViewDevice();
	glm::uvec3 axisresolution[3] = {
		glm::uvec3(gridresolution, gridresolution * axisscalar, gridresolution * axisscalar),	// YZ
		glm::uvec3(gridresolution * axisscalar, gridresolution, gridresolution * axisscalar),	// XZ
		glm::uvec3(gridresolution * axisscalar, gridresolution * axisscalar, gridresolution)	// XY
	};

	////////////////////////////////////////////////////////////////////////////////
	// Create the voxelizer textures
	////////////////////////////////////////////////////////////////////////////////
	for (uint32_t i = 0; i < AXISCOUNT; i++)
	{
		CreateIsotropicVoxelTexture(core, voxelizergrid[i].albedoOpacity, axisresolution[i].x, axisresolution[i].y, axisresolution[i].z, 1, VK_FORMAT_R8G8B8A8_UNORM, 1);
		CreateIsotropicVoxelTexture(core, voxelizergrid[i].normal, axisresolution[i].x, axisresolution[i].y, axisresolution[i].z, 1, VK_FORMAT_R8G8B8A8_UNORM, 1);
		CreateIsotropicVoxelTexture(core, voxelizergrid[i].emission, axisresolution[i].x, axisresolution[i].y, axisresolution[i].z, 1, VK_FORMAT_R8G8B8A8_UNORM, 1);
	}

	////////////////////////////////////////////////////////////////////////////////
	// Create the voxelizer textures
	////////////////////////////////////////////////////////////////////////////////
	CreateAnisotropicVoxelTexture(core, voxelgrid.albedoOpacity, gridresolution, gridresolution, gridresolution, VK_FORMAT_R8G8B8A8_UNORM, mipcount, cascadecount);
	CreateAnisotropicVoxelTexture(core, voxelgrid.normal, gridresolution, gridresolution, gridresolution, VK_FORMAT_R8G8B8A8_UNORM, mipcount, cascadecount);
	CreateAnisotropicVoxelTexture(core, voxelgrid.emission, gridresolution, gridresolution, gridresolution, VK_FORMAT_R8G8B8A8_UNORM, mipcount, cascadecount);
	CreateAnisotropicVoxelTexture(core, voxelgrid.bufferPosition, gridresolution, gridresolution, gridresolution, VK_FORMAT_R8G8B8A8_UNORM, 1, 1);
	
	uint32_t size = gridresolution*gridresolution*gridresolution * sizeof(uint32_t)*NUM_DIRECTIONS;
	VKTools::CreateBuffer(&core, device,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		size,
		NULL,
		&voxelgrid.surfacelist.buffer,
		&voxelgrid.surfacelist.memory,
		&voxelgrid.surfacelist.descriptor);

	return 0;
}