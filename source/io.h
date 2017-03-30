#ifndef IO_H
#define IO_H

#include <Windows.h>
#include <stdint.h>

struct MappedFile
{
	void* file;
	void* mapping;
	void* dataPtr;
	uint64_t size;
	uint64_t changeTimestamp;
};

inline uint32_t readonly_mapped_file_open(MappedFile* outFile, const char* filePath)
{
	MappedFile temp;
	LARGE_INTEGER size;

	// Opening the file
	temp.file = CreateFile(filePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (temp.file == INVALID_HANDLE_VALUE)
		return -2;	// Could not open the file. It is really quite likely that the file just doesn't exist.

					// Getting the size
	if (!GetFileSizeEx(temp.file, &size))
		return CloseHandle(temp.file),	// For some reason we failed to get the file size. Huh.
		-3;

	temp.size = size.QuadPart;

	// Get the last change size
	FILE_BASIC_INFO basicInfo;
	if (!GetFileInformationByHandleEx(temp.file, FileBasicInfo, &basicInfo, sizeof(basicInfo)))
		return CloseHandle(temp.file),	// For some reason we failed to get the file info. Huh.
		-4;

	temp.changeTimestamp = basicInfo.LastWriteTime.QuadPart;

	// Creating the mapping
	temp.mapping = CreateFileMapping(temp.file, NULL, PAGE_READONLY, 0, 0, NULL);
	if (temp.mapping == 0)
		return CloseHandle(temp.file),		// We were unable to map the file
		-5;

	// Mapping the view
	temp.dataPtr = (uint8_t*)MapViewOfFile(temp.mapping, FILE_MAP_READ, 0, 0, 0);
	if (temp.dataPtr == 0)
		return CloseHandle(temp.mapping),	// We were unable to map the file to an actual pointer in memory
		CloseHandle(temp.file),
		-6;

	*outFile = temp;

	//
	return 0;
}

inline uint32_t readonly_mapped_file_close(MappedFile* file)
{
	UnmapViewOfFile(file->dataPtr);
	CloseHandle(file->mapping);
	CloseHandle(file->file);

	return 0;
}

inline uint32_t readonly_mapped_file_get_data(MappedFile* file, void** outData, uint64_t* outSizeInBytes)
{
	*outData = (void*)file->dataPtr;
	*outSizeInBytes = file->size;
	return 0;
}

inline uint32_t readonly_mapped_file_get_change_timestamp(MappedFile* file, uint64_t* outChangeTime)
{
	*outChangeTime = file->changeTimestamp;
	return 0;
}

#endif	//IO_H