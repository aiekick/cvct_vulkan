#include "AssetManager.h"
#include <stdio.h>
#include "io.h"
#include "MemoryAllocator.h"

AssetManager::AssetManager()
{
	//assign the convertermap funcitons
	ConverterMap cm[] = 
	{
		{ ".png",  ConvertAsset_Image },
		{ ".tga",  ConvertAsset_Image },
		{ ".tif",  ConvertAsset_Image },
		{ ".spv",  ConvertAsset_SPIRV },
		{ ".ogex", ConvertAsset_OpenGEX}
	};
	memset(m_conversionStack, 0, sizeof(m_conversionStack));			//set the conversion stack to 0
	memcpy(m_converterMap, cm, sizeof(ConverterMap) * CONVERTERNUM);	//assign the conversionmap
	m_descriptorCount = 0;
	m_conversionDepth = 0;
	m_cacheEntryCount = 0;
	m_modifcationCount = 0;
}

AssetManager::~AssetManager()
{
}

int32_t AssetManager::InitAssetManager()
{
	//create all the allocators
	m_assetAllocator = CreateVirtualMemoryAllocator(ALLOCATOR_IDX_ASSET_DATA, 0xFFFFFFFF);
	m_assetDescriptorAllocator = CreateVirtualMemoryAllocator(ALLOCATOR_IDX_ASSET_DESC, 0xFFFFFFFF);
	m_cacheEntryAllocator = CreateVirtualMemoryAllocator(ALLOCATOR_IDX_CACHE_ENTRY, 0xFFFFFFFF);
	m_dependencyAllocator = CreateVirtualMemoryAllocator(ALLOCATOR_IDX_DEPENDENCIES, 0xFFFFFFFF);

	//allocate memory
	m_cacheEntries = (CacheEntry*)GetVirtualMemoryStart(ALLOCATOR_IDX_CACHE_ENTRY,m_cacheEntryAllocator);
	//m_cacheEntries = (CacheEntry*)AllocateVirtualMemory(ALLOCATOR_IDX_CACHE_ENTRY, m_cacheEntryAllocator, 0xFFFFFFFF);
	m_assetDescriptors = (AssetDescriptor*)AllocateVirtualMemory(ALLOCATOR_IDX_ASSET_DESC, m_assetDescriptorAllocator, 0xFFFFFFFF);

	//load asset cache here
#define LOADCACHE
#ifdef LOADCACHE
	MappedFile file;
	if(readonly_mapped_file_open(&file,"assets/assets.cache") == 0)		//loads the file, and puts it in memory
	{
		printf("Loading asset cache: \n");
		LARGE_INTEGER start, end;
		double tickToMiliseconds;
		QueryPerformanceFrequency(&start);
		tickToMiliseconds = 1000.0f / start.QuadPart;
		QueryPerformanceCounter(&start);

		AssetCacheHeader* header = nullptr;
		uint64_t fileSize;
		uint32_t result = readonly_mapped_file_get_data(&file, (void**)&header, &fileSize);
		if (result == 0 && header->magicNumber ==  'RAC0')
		{
			assert(fileSize == (sizeof(*header) + (header->entryCount * sizeof(CacheEntry) + header->assetBlobSize + header->dependencyBlobSize)));
			void* assetData = AllocateVirtualMemory(ALLOCATOR_IDX_ASSET_DATA, m_assetAllocator, header->assetBlobSize);
			char* dependencyData = (char*)AllocateVirtualMemory(ALLOCATOR_IDX_DEPENDENCIES, m_dependencyAllocator, header->dependencyBlobSize);
			void* m_cacheEntries = AllocateVirtualMemory(ALLOCATOR_IDX_CACHE_ENTRY, m_cacheEntryAllocator, header->entryCount * sizeof(CacheEntry));
			m_cacheEntryCount = header->entryCount;

			const CacheEntry* srcEntries = (const CacheEntry*)(header + 1);
			const char* srcData = (const char*)(srcEntries + header->entryCount);	//offset depending on the number of entrycounts
			const char* scrDeps = (const char*)(srcData + header->assetBlobSize);	//offset depending on the assetblob size in bytes

			memcpy(m_cacheEntries, srcEntries, header->entryCount * sizeof(CacheEntry));
			memcpy(assetData, srcData, header->assetBlobSize);
			memcpy(dependencyData, scrDeps, header->dependencyBlobSize);

			QueryPerformanceCounter(&end);
			printf("Loading asset cache took %.02f ms\n\n", tickToMiliseconds * (end.QuadPart - start.QuadPart));
		}

		readonly_mapped_file_close(&file);
	}
	else
	{
		printf("No cache available, magic number not consistant \n");
	}
#endif
	
	return 0;
}

int32_t AssetManager::LoadAsset(const char* path, uint32_t pathLength)
{
	//TODO: this early out wont work
	for (uint32_t i = 0; i < m_descriptorCount; i++)
	{
		if(strcmp((const char*)m_assetDescriptors[i].name,path) == 0)
		return -1;	//already loaded from cache or file
	}

	//save it 
	char* buffer = (char*)_alloca(pathLength + 1);
	strncpy(buffer, path, pathLength);
	buffer[pathLength] = '\0';

	//set the dependency of the file
	uint32_t curDepth = m_conversionDepth++;
	assert(curDepth < CONVERSIONDEPTH);
	if (curDepth > 0)
	{
		char* depStr = (char*)AllocateVirtualMemory(ALLOCATOR_IDX_DEPENDENCIES, m_dependencyAllocator, pathLength + 1);
		strcpy_s(depStr, pathLength + 1, buffer);

		for (uint32_t i = curDepth; i > 0; i--)
		{
			m_conversionStack[i - 1].dependencyCount++;
			if (m_conversionStack[i - 1].dependencyCount == 1)
				m_conversionStack[i - 1].dependencyStart = depStr;
		}
	}

	//open file
	MappedFile assetFile;
	uint32_t ret = readonly_mapped_file_open(&assetFile, buffer);
	if (ret != 0)
	{
		m_conversionDepth--;
		return -1;	// Could not open file
	}

	//get the data, and timestamp
	const void* dataFile;
	uint64_t fileSize, timestamp;
	readonly_mapped_file_get_data(&assetFile, (void**)&dataFile, &fileSize);	//get data
	readonly_mapped_file_get_change_timestamp(&assetFile, &timestamp);	//for comparing the change in time

	//load from the loaded cache, if it already is loaded once
	uint32_t matched = 0;
	for (uint32_t i = 0; i < m_cacheEntryCount; i++)
	{
		//file is stored in cache
		if (strcmp((const char*)m_cacheEntries[i].name,path) == 0 &&
			m_cacheEntries[i].contentLength == fileSize &&
			m_cacheEntries[i].timestamp == timestamp)
		{
			AssetDescriptor* desc = m_assetDescriptors + (m_descriptorCount++);	//offset depending

			strcpy_s((char*)desc->name,pathLength+1,path);
			desc->asset = m_cacheEntries[i].asset;

			const char* dependencyStr = m_cacheEntries[i].dependenciesStart;
			for (uint32_t j = 0; j < m_cacheEntries[i].dependencyCount; j++)
			{
				size_t len = strlen(dependencyStr);
				LoadAsset(dependencyStr, (uint32_t)len);
				dependencyStr += (len + 1);
			}

			readonly_mapped_file_close(&assetFile);
			m_conversionDepth--;
			return 0;	// Loaded from cache
		}
	}
	//if it doesn't load the asset from cache
	//find the extension
	const char* ext = buffer;
	for (int32_t j = pathLength - 1; j >= 0; j--)
	{
		if (buffer[j] == '.' || buffer[j] == '/' || buffer[j] == '\\')
		{
			ext = buffer + j;
			break;
		}
	}

	//find converter based on the extension
	sig_ConvertAsset* converterFunc;
	for (uint32_t i = 0; i, CONVERTERNUM; i++)
	{
		if (strcmp(ext, m_converterMap[i].type) != 0)		//no hit
			continue;
		else												//hit
		{	
			converterFunc = &m_converterMap[i].func;
			break;
		}
	}

	//get the base path length
	uint32_t basePathLength = 0;
	for (uint32_t i = pathLength; i > 0; i--)
	{
		if (buffer[i - 1] == '/' || buffer[i - 1] == '\\')
		{
			basePathLength = i;
			break;
		}
	}

	//load asset normally
	printf(" - Loading asset %-32s\r", buffer);
	LARGE_INTEGER start, end;
	double tickToMiliseconds;
	QueryPerformanceFrequency(&start);
	tickToMiliseconds = 1000.0 / start.QuadPart;
	QueryPerformanceCounter(&start);

	AssetDescriptor descriptor;
	ret = (*converterFunc)(&descriptor.asset, dataFile, fileSize, buffer, basePathLength, m_assetAllocator, ALLOCATOR_IDX_ASSET_DATA, *this);

	QueryPerformanceCounter(&end);				//end timing

	readonly_mapped_file_close(&assetFile);		//close file
	m_conversionDepth--;

	if (ret != 0)
	{
		printf(" - [%8.02f ms] Loading asset %-32s failed with code %u\n", tickToMiliseconds * (end.QuadPart - start.QuadPart), buffer, ret);
		return -7;	// Conversion failed
	}

	printf(" - [%8.02f ms] Loaded asset %-32s\n", tickToMiliseconds * (end.QuadPart - start.QuadPart), buffer);

	//offset the pointer in memory
	m_cacheEntries;
	
	CacheEntry* cacheEntry = (CacheEntry*)AllocateVirtualMemory(ALLOCATOR_IDX_CACHE_ENTRY, m_cacheEntryAllocator,sizeof(CacheEntry));
	m_cacheEntryCount++;
	//CacheEntry* cacheEntry = m_cacheEntries + (m_cacheEntryCount++);
	AssetDescriptor* descriptorEntry = m_assetDescriptors + (m_descriptorCount++);

	CacheEntry ce;
	//ce.name = path;
	strcpy_s((char*)ce.name, pathLength+1, path);
	ce.timestamp = timestamp;
	ce.contentLength = fileSize;
	ce.dependenciesStart = m_conversionStack[curDepth].dependencyStart;
	ce.dependencyCount = m_conversionStack[curDepth].dependencyCount;
	ce.asset = descriptor.asset;

	strcpy_s(descriptor.name, pathLength+1,path);

	*cacheEntry = ce;
	*descriptorEntry = descriptor;

	m_modifcationCount++;

	return 0;
}

int32_t AssetManager::FlushAssets()
{
	/*store in this format:
		- header
		- cacheentries
		- data
		- dependencies
	*/

	if (m_modifcationCount == 0)		//check for changes in the cache entry
		return 0;

	FILE* cacheFile = fopen("assets/assets.cache", "wb"); //write in binary
	if (!cacheFile)
		printf("no available cache file to flush to \n");

	AssetCacheHeader header;
	header.magicNumber = 'RAC0';
	header.entryCount = m_cacheEntryCount;
	header.assetBlobSize = m_assetAllocator->allocatedBytes;
	header.dependencyBlobSize = m_dependencyAllocator->allocatedBytes;

	uint32_t cacheSize = sizeof(CacheEntry);

	// write header
	fwrite(&header, sizeof(header), 1, cacheFile);
	// write cache entires ( descriptors )
	fwrite(m_cacheEntryAllocator->startPtr, 1, m_cacheEntryAllocator->allocatedBytes, cacheFile);
	// write raw data
	fwrite(m_assetAllocator->startPtr, 1, m_assetAllocator->allocatedBytes, cacheFile);
	// write dependencies
	fwrite(m_dependencyAllocator->startPtr, 1, m_dependencyAllocator->allocatedBytes, cacheFile);
	fclose(cacheFile);

	return 0;
}

int32_t AssetManager::GetAsset(const char* path, Asset** outAsset)
{
	Asset* out = NULL;
	int32_t ret = 0;

	for (uint32_t i = 0; i < m_descriptorCount; i++)
	{
		if (strcmp(path, (const char*)m_assetDescriptors[i].name) == 0)//hit
		{
			(*outAsset) = &m_assetDescriptors[i].asset;
			return ret;
		}
	}
	ret = -1;
	return ret;
}