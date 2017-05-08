#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#define MAXCASCADE 10
#define SHOW_ALL 0
#define SHOW_AO 1
#define SHOW_IL 2

layout (location = 0) in vec2 inUV;

layout (input_attachment_index = 2, set = 2, binding = 3) uniform subpassInput samplerposition;
layout (input_attachment_index = 3, set = 2, binding = 4) uniform subpassInput samplerNormal;
layout (input_attachment_index = 4, set = 2, binding = 5) uniform subpassInput samplerAlbedo;
layout (input_attachment_index = 5, set = 2, binding = 6) uniform subpassInput samplerTangent;

//output
layout (location = 0) out vec4 outColor;

//set bindings
layout(set = 2, binding = 1) uniform sampler3D rVoxelColor;			// Read
layout(set = 3, binding = 0) uniform sampler2D scaledOutput;		// Read

layout (set = 2, binding = 0) uniform UBO 
{
	vec4 voxelRegionWorld[MAXCASCADE];	// list of voxel region worlds
	vec3 cameraPosition;				// Positon of the camera adsfasdfa
	uint voxelGridResolution;			// Resolution of the voxel grid
	uint cascadeCount;					// Number of cascades

	float scaledWidth;					// Scaled width of framebuffer
	float scaledHeight;					// Scaled height of framebuffer
	uint conecount;						// The selected renderer
	uint renderer;				// renderer
	vec3 padding0;
} ubo;

void main()
{
	// Read G-Buffer values from previous sub pass
//	vec3 position = subpassLoad(samplerposition).rgb;
//	vec3 normal = subpassLoad(samplerNormal).rgb;
	vec4 albedo = subpassLoad(samplerAlbedo);
//	vec3 tangent = subpassLoad(samplerTangent).rgb;
	
	// Sample the scaled buffer
	vec4 indir = texture(scaledOutput,inUV);


if		(ubo.renderer == SHOW_ALL)	outColor = (albedo + vec4(indir.xyz,1)) * indir.a* indir.a;
else if	(ubo.renderer == SHOW_AO)	outColor = vec4(indir.a,indir.a,indir.a,1);
else if	(ubo.renderer == SHOW_IL)	outColor = vec4(indir.xyz,1);
}