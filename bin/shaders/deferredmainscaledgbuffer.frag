#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

//input
layout(location = 0) in vec2 inTex;
layout(location = 1) in vec3 inWPos;
layout(location = 2) in vec3 inWNormal;
layout(location = 3) in vec3 inWTan;
layout(location = 4) in vec3 inWBitan;

//output
layout (location = 0) out vec4 outColor;
layout (location = 1) out vec4 outPosition;
layout (location = 2) out vec4 outNormal;
layout (location = 3) out vec4 outAlbedo;
layout (location = 4) out vec4 outTangent;

//set bindings
layout(set = 0, binding = 1) uniform sampler textureSampler;
layout(set = 1, binding = 0) uniform texture2D diffuseTexture;
layout(set = 1, binding = 1) uniform texture2D normalTexture;
layout(set = 1, binding = 2) uniform texture2D maskTexture;

void main() 
{
	outPosition	= vec4(inWPos,1.0);
	outNormal	= vec4(normalize(inWNormal),1.0);
	outAlbedo	= texture ( sampler2D ( diffuseTexture, textureSampler ), inTex).rgba;
	outTangent	= vec4(normalize(inWTan),1.0);

	// Write to gbuffer
	outColor = vec4(0.0);
}