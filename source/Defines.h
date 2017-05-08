#ifndef DEFINES_H
#define DEFINES_H

#include <assert.h>
#include <Windows.h>

#define _STR(x) #x
#define STR(x) _STR(x)
#define LOG(severity,format,...) printf(STR(__FILE__) ": " STR(__LINE__) "][" severity"] " format "\n",__VA_ARGS__)
#define RETURN_ERROR(errcode,format,...) return LOG("CRITICAL", format, __VA_ARGS__),assert(0),errcode
#define ERROR_VOID(format,...) return LOG("CRITICAL", format, __VA_ARGS__),assert(0)

// in seconds
inline float Ctime()
{
	static __int64 start = 0;
	static __int64 frequency = 0;

	if (start == 0)
	{
		QueryPerformanceCounter((LARGE_INTEGER*)&start);
		QueryPerformanceFrequency((LARGE_INTEGER*)&frequency);
		return 0.0f;
	}

	__int64 counter = 0;
	QueryPerformanceCounter((LARGE_INTEGER*)&counter);
	return (float)((counter - start) / double(frequency));
}


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// Memory system
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

#endif	//DEFINES_H