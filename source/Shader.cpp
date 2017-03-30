#include "Shader.h"
#include "MemoryAllocator.h"
#include "VKTools.h"
#include <stdio.h>
#include <assert.h>
#include "DataTypes.h"
#include "AssetManager.h"

//loading shader binary
uint32_t ConvertAsset_HLSL_Bytecode
(
	Asset* outAsset,
	const void* data,
	uint64_t dataSizeInBytes,
	const char* basePath,
	uint32_t basePathLength,
	Memory_Linear_Allocator* allocator,
	uint32_t allocatorIdx,
	AssetManager& assetmanager)
{
	return 0;
}

uint32_t ConvertAsset_SPIRV
(
	Asset* outAsset,
	const void* data,
	uint64_t dataSizeInBytes,
	const char* basePath,
	uint32_t basePathLength,
	Memory_Linear_Allocator* allocator,
	uint32_t allocatorIdx,
	AssetManager& assetmanager)
{
	return 0;
}

///////////////////////////////////////////
////Shader
///////////////////////////////////////////
Shader::Shader()
{

}

Shader::Shader(VkPipelineShaderStageCreateInfo shaderStage)
{
	m_shaderStage = shaderStage;
}


Shader::~Shader()
{
}

namespace VKTools
{
	///////////////////////////////////////////
	////ShaderLoader
	///////////////////////////////////////////
	Shader LoadShader(const char* fileName, const char* entryPoint, VkDevice device, VkShaderStageFlagBits stage, const char* globalFile)
	{
		VkPipelineShaderStageCreateInfo shaderStage = {};
		shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStage.stage = stage;

		// Read the shader
		size_t size;
		FILE *fp = fopen(fileName, "rb");
		assert(fp);

		fseek(fp, 0L, SEEK_END);
		size = ftell(fp);
		fseek(fp, 0L, SEEK_SET);

		char* shaderCode = new char[size];
		size_t retval = fread(shaderCode, size, 1, fp);
		assert(retval == 1);
		assert(size > 0);
		fclose(fp);

		// Read the global
		size_t gsize = 0;
		char* globalCode = NULL;
		if (globalFile)
		{
			FILE* gfp = fopen(globalFile, "rb");
			assert(gfp);

			//get the size
			fseek(gfp, 0L, SEEK_END);
			gsize = ftell(gfp);
			fseek(gfp, 0L, SEEK_SET);

			globalCode = new char[gsize];
			size_t retval = fread(globalCode, gsize, 1, gfp);
			assert(retval == 1);
			assert(size > 0);
			fclose(gfp);
		}

		char* comb = new char[size + gsize];
		memcpy(comb, shaderCode, size);
		memcpy(comb + size, globalCode, gsize);

		VkShaderModule shaderModule;
		VkShaderModuleCreateInfo moduleCreateInfo;
		moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		moduleCreateInfo.pNext = NULL;
		moduleCreateInfo.codeSize = size;
		moduleCreateInfo.pCode = (uint32_t*)comb;
		moduleCreateInfo.flags = 0;

		VK_CHECK_RESULT(vkCreateShaderModule(device, &moduleCreateInfo, NULL, &shaderModule));

		delete[] shaderCode;
		delete[] comb;
		if (globalCode) delete[] globalCode;

		shaderStage.module = shaderModule;
		shaderStage.pName = entryPoint; // todo : make param
		assert(shaderStage.module != NULL);
		//create the shader class
		Shader shader(shaderStage);
		//return the shader
		return shader;
	}
};


