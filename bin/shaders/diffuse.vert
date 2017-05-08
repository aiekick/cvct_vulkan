#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

//input locations
layout(location = 0) in vec3 inPos;
layout(location = 1) in vec2 inTex;
layout(location = 2) in vec3 inNorm;
layout(location = 3) in vec3 inTan;
layout(location = 4) in vec3 inBtan;

//output locations
layout(location = 0) out vec2 outTex;
layout(location = 1) out vec3 outWPos;
layout(location = 2) out vec3 outWNormal;
layout(location = 3) out vec3 outWTan;
layout(location = 4) out vec3 outWBitan;

layout (set = 0, binding = 0) uniform UBO 
{
	mat4 modelMatrix;
	mat4 viewMatrix;
	mat4 projectionMatrix;
} ubo;

out gl_PerVertex 
{
    vec4 gl_Position;   
};

void main() 
{
	mat4 mvp = ubo.projectionMatrix * ubo.viewMatrix * ubo.modelMatrix;
	gl_Position = mvp * vec4(inPos.xyz, 1.0);
	vec4 wpos = ubo.modelMatrix * vec4(inPos.xyz, 1.0);
	outTex = inTex;
	outWPos = wpos.xyz;
	outWNormal = (ubo.modelMatrix * vec4(inNorm.xyz, 1.0)).xyz;
	outWTan = (ubo.modelMatrix * vec4(inTan.xyz, 1.0)).xyz;
	outWBitan = (ubo.modelMatrix * vec4(inBtan.xyz, 1.0)).xyz;
}
