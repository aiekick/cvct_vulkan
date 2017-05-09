#ifndef MEMORYALLOCATOR_H
#define MEMORYALLOCATOR_H

#include <Windows.h>
#include <assert.h>

struct Memory_Linear_Allocator
{
	uint8_t* startPtr;			//starting pointer of allocated buffer
	uint64_t sizeInBytes;		//totall size of the allocated buffer
	uint64_t allocatedBytes;	//current allocated size
};

inline Memory_Linear_Allocator* CreateVirtualMemoryAllocator(uint32_t rendererIdx, uint64_t sizeInBytes)
{
	void* memptr = (void*)((rendererIdx + 1) * 0x100000000ULL);
	memptr = VirtualAlloc(memptr, sizeInBytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

	if (memptr == NULL)
		return NULL;

	Memory_Linear_Allocator* allocator = (Memory_Linear_Allocator*)malloc(sizeof(Memory_Linear_Allocator));
	(*allocator).startPtr = (uint8_t*)memptr;
	(*allocator).sizeInBytes = sizeInBytes;
	(*allocator).allocatedBytes = 0;

	return allocator;
}

inline uint32_t DestroyVirtualMemoryAllocator(uint32_t rendererIdx, Memory_Linear_Allocator* allocator)
{
	if (VirtualFree(allocator->startPtr, 0, MEM_RELEASE) == FALSE)
		return -1;

	free(allocator);

	return 0;
}

inline void* AllocateVirtualMemory(uint32_t rendererIdx, Memory_Linear_Allocator* allocator, uint64_t sizeInBytes)
{
	if (allocator->allocatedBytes + sizeInBytes > allocator->sizeInBytes)
		return NULL;

	uint8_t* startPtr = allocator->startPtr + allocator->allocatedBytes;
	allocator->allocatedBytes += sizeInBytes;

	return (void*)startPtr;
}

inline void ResetVirtualMemory(uint32_t rendererIdx, Memory_Linear_Allocator* allocator)
{
	void* memptr = allocator->startPtr;
	memptr = VirtualAlloc(memptr, allocator->sizeInBytes, MEM_RESET, PAGE_READWRITE);
	assert(memptr && memptr == allocator->startPtr);
	allocator->allocatedBytes = 0;
}

inline uint64_t GetVirtualMemoryAllocatedByteCount(uint32_t rendererIdx, Memory_Linear_Allocator* allocator)
{
	return allocator->allocatedBytes;
}

inline void* GetVirtualMemoryStart(uint32_t rendererIdx, Memory_Linear_Allocator* allocator)
{
	return (void*)allocator->startPtr;
}

#endif	//MEMORYALLOCATOR_H
