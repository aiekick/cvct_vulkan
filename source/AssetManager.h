#ifndef ASSETMANAGER_H
#define ASSETMANAGER_H

#include <stdint.h>
#include "Defines.h"
#include "DataTypes.h"
#include "OpenGEX.h"


#define CONVERTERNUM 5
#define CONVERSIONDEPTH 4

typedef uint32_t(*sig_ConvertAsset) (asset_s* outAsset, const void* data, uint64_t dataSizeInBytes, const char* basePath, uint32_t basePathLength, Memory_Linear_Allocator* allocator, uint32_t allocatorIdx);
uint32_t ConvertAsset_Image(asset_s* outAsset, const void* data, uint64_t dataSizeInBytes, const char* basePath, uint32_t basePathLength, Memory_Linear_Allocator* allocator, uint32_t allocatorIdx);
uint32_t ConvertAsset_OpenGEX(asset_s* outAsset, const void* data, uint64_t dataSizeInBytes, const char* basePath, uint32_t basePathLength, Memory_Linear_Allocator* allocator, uint32_t allocatorIdx);
uint32_t ConvertAsset_HLSL_Bytecode(asset_s* outAsset, const void* data, uint64_t dataSizeInBytes, const char* basePath, uint32_t basePathLength, Memory_Linear_Allocator* allocator, uint32_t allocatorIdx);
uint32_t ConvertAsset_SPIRV(asset_s* outAsset, const void* data, uint64_t dataSizeInBytes, const char* basePath, uint32_t basePathLength, Memory_Linear_Allocator* allocator, uint32_t allocatorIdx);

struct ConverterMap
{
	const char* type;
	sig_ConvertAsset func;
};

struct ConversionStack
{
	const char* dependencyStart;
	uint32_t dependencyCount;
};


class AssetManager
{
public:
	AssetManager();
	~AssetManager();

	int32_t InitAssetManager();// initializes the assetmanager
	int32_t LoadAsset(const char* path, uint32_t pathLength); 	//loads all assets, will branch depending on the type of file
	int32_t FlushAssets();
	int32_t GetAsset(const char* path, asset_s** outAsset);
	///////////////////////////////////////////////////////
	//convertermap
	ConverterMap		m_converterMap[CONVERTERNUM];
	//counters
	uint32_t m_descriptorCount;
	uint64_t m_cacheEntryCount;
	uint32_t m_modifcationCount;
	//allocators
	Memory_Linear_Allocator* m_assetAllocator;				//linear asset allocator for easy caching of data
	Memory_Linear_Allocator* m_assetDescriptorAllocator;	//linear descriptor allocator
	Memory_Linear_Allocator* m_cacheEntryAllocator;			//linear cache entry allocator
	Memory_Linear_Allocator* m_dependencyAllocator;			//linear dependency allocator
	//memory pointer data
	CacheEntry* m_cacheEntries;								//pointer to start of the cache entries
	AssetDescriptor* m_assetDescriptors;					//pointer to start of the allocated descriptors
	uint32_t m_conversionDepth;
	ConversionStack m_conversionStack[CONVERSIONDEPTH];

};

#endif	//ASSETMANAGER_H
