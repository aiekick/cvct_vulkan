#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#define COLOR_IMAGE_POSX_3D_BINDING 0
#define COLOR_IMAGE_NEGX_3D_BINDING 1
#define COLOR_IMAGE_POSY_3D_BINDING 2
#define COLOR_IMAGE_NEGY_3D_BINDING 3
#define COLOR_IMAGE_POSZ_3D_BINDING 4
#define COLOR_IMAGE_NEGZ_3D_BINDING 5
#define COLOR_IMAGE_COUNT 6
#define OUTPUTVERTICES 16
#define INDICES 18

layout(points) in;                                                                  
layout(triangle_strip,max_vertices = OUTPUTVERTICES) out;          

// Input
layout(location = 0) in uint inVertexID[];

// Output locations
layout(location = 0) out vec4 outPosition;		// Clip space position
layout(location = 1) out vec4 outColor;			// Voxel color

// Voxel grid
layout(set = 1, binding = 0) uniform sampler3D voxelColor;

// Uniform buffers
layout (set = 0, binding = 0) uniform UBO 
{
	mat4 modelMatrix;
	mat4 viewMatrix;
	mat4 projectionMatrix;
} ubo;

layout(set = 1, binding = 1) uniform UBO2
{
	int gridResolution;			// base grid resolution per side and cascade
	float voxelSize;			// base voxel size per side and cascade
	uint mipmap;				// mipmap
	uint side;					// the side. 0 - 5
	vec4 voxelRegionWorld;		// Base(0) origin(.xyz) and size of the grid region(.w) ( campos - (voxelbaseregion / 2) )
}ubo2;

layout(push_constant) uniform PushConsts
{
	layout(offset = 0)uint cascadeNum;		// The current cascade
} pc;

// Global members
float gvoxelSize = 0;
uint ggridres = 0;
uint gvoxelside = 0;

ivec3 GetVoxelFromIndex(int index)
{
	ivec3 res;
	int size = int(ggridres);
	res.z = index / (size * size);
	res.y = (index - (res.z * (size*size)) ) / size;
	res.x = index % size;
	return res;
}

vec3 GetWorldPositionFromGrid(vec3 gridPosition)
{
	vec3 res;
	vec3 region = ubo2.voxelRegionWorld.xyz;
	vec3 gridres = vec3(ggridres,ggridres,ggridres);
	vec3 worldSize = vec3(ubo2.voxelRegionWorld.w,ubo2.voxelRegionWorld.w,ubo2.voxelRegionWorld.w);
	res = ((gridPosition / gridres) * worldSize) + region;
	return res;
}

void main()
{
	gvoxelSize = ubo2.voxelSize*pow(2,ubo2.mipmap);
	ggridres = uint(float(ubo2.gridResolution)*pow(0.5f,ubo2.mipmap));
	gvoxelside = max(0,min(ubo2.side, 5));

	float voxelHalfSize = gvoxelSize/2;
	vec3 min = vec3(-voxelHalfSize,-voxelHalfSize,-voxelHalfSize);
	vec3 max = vec3(voxelHalfSize,voxelHalfSize,voxelHalfSize);

	// AABB vertices
    const vec3 Pos[8] = {
        vec3( min.x, min.y, min.z ),    // 0
        vec3( min.x, min.y, max.z ),    // 1
        vec3( min.x, max.y, min.z ),    // 2
        vec3( min.x, max.y, max.z ),    // 3
        vec3( max.x, min.y, min.z ),    // 4
        vec3( max.x, min.y, max.z ),    // 5
        vec3( max.x, max.y, min.z ),    // 6
        vec3( max.x, max.y, max.z )     // 7
    };

	const int Index[INDICES] = 
	{
        0, 1, 2,
        3, 6, 7,
        4, 5, 99,
        2, 6, 0,
        4, 1, 5,
        3, 7, 99
    };

	// Grid position
	uint maxIndexCount = ggridres*ggridres*ggridres;
	// Check if emit primitive
	if(inVertexID[0] < maxIndexCount)
	{
		vec3 voxelRes = vec3(ggridres,ggridres,ggridres);
		ivec3 gridPos = GetVoxelFromIndex(int(inVertexID[0]));	// grid position relative to side and cascade
		int sideoffset = int(ggridres * ubo2.side);
		int cascadeoffset = int(ggridres * pc.cascadeNum);

		ivec3 coord = ivec3(gridPos.x + sideoffset, gridPos.y + cascadeoffset, gridPos.z);
		//coord = ivec3(vec3(coord)*pow(0.5f,ubo2.mipmap));
		
		vec4 color = texelFetch(voxelColor, coord, int(ubo2.mipmap)).rgba;
		if(color.r > 0.0 || color.g > 0.0 || color.b > 0.0 || color.a > 0.0)
		{
			// World position
			vec3 worldPos = GetWorldPositionFromGrid(vec3(gridPos));	
			// Clip position
			mat4 vp = ubo.projectionMatrix * ubo.viewMatrix;
			for(uint i = 0; i < INDICES; i++)
			{
				if(Index[i] == 99)
				{
					EndPrimitive();
				}
				else
				{
					vec4 position =	vp * (vec4(worldPos + Pos[Index[i]],1));
					outPosition = position;
					outColor = color;
					//outColor = vec4(1,0,0,1);
					gl_Position = position;
					EmitVertex();
				}
			}
		}
	}
	
}