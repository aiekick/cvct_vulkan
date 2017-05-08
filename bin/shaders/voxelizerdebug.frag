#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

// Input
layout(location = 0) in vec4 inPosition;		// Clip space position
layout(location = 1) in vec4 inColor;			// Voxel color
// Output
layout(location = 0) out vec4 outFragColor;

void main() 
{
	outFragColor = vec4(inColor.xyz,1);	
}