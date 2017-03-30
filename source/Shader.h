#ifndef SHADER_H
#define SHADER_H

#include <vulkan.h>

class Shader
{
private:
public:
	VkPipelineShaderStageCreateInfo m_shaderStage;

	Shader();
	Shader(VkPipelineShaderStageCreateInfo shaderStage);
	~Shader();
};

namespace VKTools
{
	Shader LoadShader(const char* fileName, const char* entryPoint, VkDevice device, VkShaderStageFlagBits stage, const char* globalFile = NULL);
};

#endif	//SHADER_H