#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

//input locations
layout(location = 0) in uint inVertexID;

//output locations
layout(location = 0) out uint outVertexID;

void main() 
{
	outVertexID = inVertexID;
}
