// http://www.seas.upenn.edu/~pcozzi/OpenGLInsights/OpenGLInsights-SparseVoxelization.pdf
#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#define COLOR_IMAGE_VOXEL 0
#define ALPHA_IMAGE_VOXEL 3
#define COLOR_IMAGE_COUNT 6

#define COLOR_IMAGE_POSX_3D_BINDING 0
#define COLOR_IMAGE_NEGX_3D_BINDING 1
#define COLOR_IMAGE_POSY_3D_BINDING 2
#define COLOR_IMAGE_NEGY_3D_BINDING 3
#define COLOR_IMAGE_POSZ_3D_BINDING 4
#define COLOR_IMAGE_NEGZ_3D_BINDING 5

#define TEXTURE_DIFFUSE 0
#define TEXTURE_NORMAL 1
#define TEXTURE_MASK 2
#define EPS 0000.1f

// Input
layout(location = 0) in vec2 inTex;
layout(location = 1) in vec3 inWPos;
layout(location = 2) in vec3 inWNormal;
layout(location = 3) in vec3 inWTan;
layout(location = 4) in vec3 inWBitan;

// Set binding 0
layout(set = 0, binding = 1) uniform sampler textureSampler;
// Set binding 1
layout(set = 1, binding = 0) uniform texture2D diffuseTexture;
layout(set = 1, binding = 1) uniform texture2D normalTexture;
layout(set = 1, binding = 2) uniform texture2D maskTexture;
// Uniform buffers
layout (set = 2, binding = 2) uniform UBO 
{
	vec4 voxelRegionWorld;	// Base(0) origin(.xyz) and size of the grid region(.w) ( campos - (voxelbaseregion / 2) )
	uint voxelResolution;	// Resolution of the voxel grid
	uint cascadeCount;		// The number of totall cascades
	vec2 padding0;
} ubo;
// Voxel textures
layout(set = 2, binding = COLOR_IMAGE_VOXEL, r32ui) uniform uimage3D tVoxColor;
// Alpha textures
layout(set = 2, binding = ALPHA_IMAGE_VOXEL, r32ui) uniform uimage3D tVoxAlpha;
// Current processed cascade
layout(push_constant) uniform PushConsts
{
	layout(offset = 0)uint cascadeNum;		// The current cascade
} pc;

// Globals
float gcascadeNum = 0;
float gcascadeCount = 0;

// Gets the visibliy of the current texel by comparing it with the depth
float getVisibility()
{
    //float fragLightDepth = vertexData.shadowMapPos.z;
    //float shadowMapDepth = texture(shadowMap, vertexData.shadowMapPos.xy).r;
    //if(fragLightDepth <= shadowMapDepth)
      //  return 1.0;
    // Less darknessFactor means lighter shadows
    //float darknessFactor = 20.0;
    //return clamp(exp(darknessFactor * (shadowMapDepth - fragLightDepth)), 0.0, 1.0);

	return 1.0;
}

// Because imageatomic only supports ui image formats, bitshift RGB to an unsigned int format.
uint packColor(vec4 color)
{
    uvec4 cb = uvec4(color*255.0);
    return (cb.a << 24U) | (cb.b << 16U) | (cb.g << 8U) | cb.r;
}
// Convert UINT(ABGR) to vec4(RGBA)
vec4 ConvRGBA8ToVec4(uint val)
{
	return vec4(float((val&0x000000FF)), float((val&0x0000FF00)>>8U), float((val&0x00FF0000)>>16U), 
				float((val&0xFF000000)>>24U) );
}
// Convert vec4(RGBA) to UINT(ABGR)
uint ConvVec4ToRGBA8(vec4 val)
{
	uvec4 cb = uvec4(val);
	return (cb.w << 24U) | (cb.z << 16U) | (cb.y << 8U) | cb.x;
}

void ImageAtomicRGBA8Avg(uint side, ivec3 coords, vec3 val, float alphaVal)
{
	vec4 diffuse = vec4(val.rgb*255.0f,1.0);
	uint newDiffuse = ConvVec4ToRGBA8(diffuse);
	uint prevStoredDiffuse = 0; uint curStoredDiffuse; 
	// offset the s for the sides
	uint sideoffset = side * ubo.voxelResolution;		
	coords.x += int(sideoffset);
	while( (curStoredDiffuse = imageAtomicCompSwap(tVoxColor,coords,prevStoredDiffuse,newDiffuse)) != prevStoredDiffuse)
	{
		// Calculate the average diffuse
		prevStoredDiffuse = curStoredDiffuse;
		vec4 rval = ConvRGBA8ToVec4(curStoredDiffuse);	// Convert back to rgba
		rval.xyz = (rval.xyz * rval.w);					// Denormalize
		vec4 curDiffuseF = rval+diffuse;				// add current iteration
		curDiffuseF.xyz /= (curDiffuseF.w);				// Normalize
		newDiffuse = ConvVec4ToRGBA8(curDiffuseF);
	}

	// atomic add the alpha
	uint alpha = uint(alphaVal * 255.0f);
	imageAtomicAdd(tVoxAlpha,coords,alpha);
}

void main() 
{
	// Set the globals
	gcascadeNum = float(pc.cascadeNum);
	gcascadeCount = float(ubo.cascadeCount);

	float xoffset = 0.166666;		// 1 / anisotropic
	uint cascadeoffset =  ubo.voxelResolution * uint(gcascadeNum);

	// Get the visibility from the depth texture, to see if the texel is lit by the light
    float visibility = getVisibility();
	// Get the diffuse color of the texel
	vec4 diffuse = texture(sampler2D(diffuseTexture, textureSampler), inTex);
	// Get the alpha from the diffuse texture
	float alpha = diffuse.a;
    alpha = 1.0;
	// Normalize the normal in fragment space
    vec3 normal = normalize(inWNormal);
	// Calculate the scalar depending on the light direction vector and normal
    //float LdotN = max(dot(-uLightDir, normal), 0.0);
	float LdotN = max(1.0, 0.0);
	// Calculate the outcolor
   // vec3 outColor = diffuse.rgb*visibility*LdotN;

	vec3 outColor = diffuse.rgb;
	float alphaColor = diffuse.a;
	
	// Calculate the vertex world position, and convert it to region space
	vec4 voxelRegionWorld = ubo.voxelRegionWorld;
	uint voxelResolution = ubo.voxelResolution;
    vec3 voxelPosTextureSpace = (inWPos-voxelRegionWorld.xyz)/voxelRegionWorld.w;
	// Get from region space scalar to voxel space
	ivec3 voxelPosImageCoord = ivec3(voxelPosTextureSpace * voxelResolution);
	//offset the y for the cascades
	voxelPosImageCoord.y += int(cascadeoffset);			

	// write to the anisotropic voxel textures
	// use atomic functions so other shaders can't write to it at the same time
	// weigh every color with the normals ( anisotropic )
	// Slow way, but most accurate (blending)
	ImageAtomicRGBA8Avg(COLOR_IMAGE_POSX_3D_BINDING,voxelPosImageCoord,vec3(outColor*max(normal.x,	EPS)), alphaColor);
	ImageAtomicRGBA8Avg(COLOR_IMAGE_NEGX_3D_BINDING,voxelPosImageCoord,vec3(outColor*max(-normal.x,	EPS)), alphaColor);
	ImageAtomicRGBA8Avg(COLOR_IMAGE_POSY_3D_BINDING,voxelPosImageCoord,vec3(outColor*max(normal.y,	EPS)), alphaColor);
	ImageAtomicRGBA8Avg(COLOR_IMAGE_NEGY_3D_BINDING,voxelPosImageCoord,vec3(outColor*max(-normal.y,	EPS)), alphaColor);
	ImageAtomicRGBA8Avg(COLOR_IMAGE_POSZ_3D_BINDING,voxelPosImageCoord,vec3(outColor*max(normal.z,	EPS)), alphaColor);
	ImageAtomicRGBA8Avg(COLOR_IMAGE_NEGZ_3D_BINDING,voxelPosImageCoord,vec3(outColor*max(-normal.z,	EPS)), alphaColor);

	// Store the RGB. A consists of a 8 bit counter
	//imageAtomicAdd(tVoxColor[COLOR_IMAGE_POSX_3D_BINDING], voxelPosImageCoord, packColor(vec4(outColor*max(normal.x,	EPS), 1.0)));
	//imageAtomicAdd(tVoxColor[COLOR_IMAGE_NEGX_3D_BINDING], voxelPosImageCoord, packColor(vec4(outColor*max(-normal.x,	EPS), 1.0)));
	//imageAtomicAdd(tVoxColor[COLOR_IMAGE_POSY_3D_BINDING], voxelPosImageCoord, packColor(vec4(outColor*max(normal.y,	EPS), 1.0)));
	//imageAtomicAdd(tVoxColor[COLOR_IMAGE_NEGY_3D_BINDING], voxelPosImageCoord, packColor(vec4(outColor*max(-normal.y,	EPS), 1.0)));
	//imageAtomicAdd(tVoxColor[COLOR_IMAGE_POSZ_3D_BINDING], voxelPosImageCoord, packColor(vec4(outColor*max(normal.z,	EPS), 1.0)));
	//imageAtomicAdd(tVoxColor[COLOR_IMAGE_NEGZ_3D_BINDING], voxelPosImageCoord, packColor(vec4(outColor*max(-normal.z,	EPS), 1.0)));

	// Old
	//imageAtomicMax(tVoxColor[COLOR_IMAGE_POSX_3D_BINDING], voxelPosImageCoord, packColor(vec4(outColor*max(normal.x,	EPS), alpha)));
	//imageAtomicMax(tVoxColor[COLOR_IMAGE_NEGX_3D_BINDING], voxelPosImageCoord, packColor(vec4(outColor*max(-normal.x,	EPS), alpha)));
	//imageAtomicMax(tVoxColor[COLOR_IMAGE_POSY_3D_BINDING], voxelPosImageCoord, packColor(vec4(outColor*max(normal.y,	EPS), alpha)));
	//imageAtomicMax(tVoxColor[COLOR_IMAGE_NEGY_3D_BINDING], voxelPosImageCoord, packColor(vec4(outColor*max(-normal.y,	EPS), alpha)));
	//imageAtomicMax(tVoxColor[COLOR_IMAGE_POSZ_3D_BINDING], voxelPosImageCoord, packColor(vec4(outColor*max(normal.z,	EPS), alpha)));
	//imageAtomicMax(tVoxColor[COLOR_IMAGE_NEGZ_3D_BINDING], voxelPosImageCoord, packColor(vec4(outColor*max(-normal.z,	EPS), alpha)));
}