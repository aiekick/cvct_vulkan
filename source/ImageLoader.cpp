#include "ImageLoader.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image/stb_image.h>
#include "MemoryAllocator.h"

uint32_t ConvertAsset_Image
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
	//TODO LOAD MIPMAPS
	int width = 0, height = 0, comp = 0;
	stbi_set_flip_vertically_on_load(1);
	stbi_ldr_to_hdr_scale(1.0f);
	stbi_ldr_to_hdr_gamma(1.0f);
	float* pixels = stbi_loadf_from_memory((stbi_uc*)data, (int)dataSizeInBytes, &width, &height, &comp, 4);

	if (!pixels)
	{
		printf("Coudln't load image: \n");
		return -1;
	}

	uint32_t totalPixelCount = width * height;
	uint32_t mips = 1;
	uint32_t totalByteSize = sizeof(ImageDesc) + totalPixelCount * sizeof(uint32_t) + mips * sizeof(MipDesc);
	uint8_t* assetData = (uint8_t*)AllocateVirtualMemory(ALLOCATOR_IDX_ASSET_DATA, allocator, totalByteSize);

	//assign the first image description(main image)
	ImageDesc* imgDesc = (ImageDesc*)assetData;
	MipDesc* mip = (MipDesc*)(assetData + sizeof(ImageDesc));
	ImageDesc image;
	image.height = height;
	image.width = width;
	image.mipCount = mips;
	image.mips = mip;
	*imgDesc = image;

	uint32_t* outPixels = (uint32_t*)(assetData + sizeof(ImageDesc) + mips * sizeof(MipDesc));
	uint32_t* mipPixels = outPixels;
	
	MipDesc mip0;
	mip0.width = width;
	mip0.height = height;
	mip0.offset = 0;
	*mip = mip0;

	for (int32_t y = 0; y < height; y++)
	{
		for (int32_t x = 0; x < width; x++)
		{
			float* src = pixels + (4 * (y * width + x));
			mipPixels[y * width + x] =
				(((uint32_t)(src[0] * 255.0f)) << 0)
				| (((uint32_t)(src[1] * 255.0f)) << 8)
				| (((uint32_t)(src[2] * 255.0f)) << 16)
				| (((uint32_t)(src[3] * 255.0f)) << 24);
		}
	}

	outAsset->type = 'IMG';
	outAsset->size = sizeof(ImageDesc) + mips * sizeof(MipDesc) + totalPixelCount * 4;
	outAsset->data = assetData;

	stbi_image_free(pixels);
	
	return 0;
}

ImageLoader::ImageLoader()
{
}

ImageLoader::~ImageLoader()
{
}
