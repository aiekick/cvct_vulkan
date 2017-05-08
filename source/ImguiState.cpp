#include "PipelineStates.h"

#include <glm/gtc/matrix_transform.hpp>
#include "VCTPipelineDefines.h"
#include "SwapChain.h"
#include "VulkanCore.h"
#include "Shader.h"
#include "DataTypes.h"
#include "AnisotropicVoxelTexture.h"
#include "Camera.h"
#include "imgui_impl_glfw_vulkan.h"

struct PushConstantComp
{
	uint32_t cascadeNum;		// Current cascade
};

static void check_vk_result(VkResult err)
{
	// todo: add logs
}

// TODO: UGLY HERE. MAKE PRETTY LATER
const uint32_t VALUESIZE = 90;
float deltaTimeValue[VALUESIZE] = { 0 };
RenderStatesTimeStamps timestampValue[VALUESIZE] = { 0 };
int values_offset = 0;
void BuildCommandBufferImGUIState
(
	RenderState* renderstate,
	VkCommandPool commandpool,
	VulkanCore* core,
	uint32_t framebufferCount,
	VkFramebuffer* framebuffers,
	BYTE* parameters
)
{
	uint32_t width = core->GetSwapChain()->m_width;
	uint32_t height = core->GetSwapChain()->m_height;
	RenderState* renderState = renderstate;

	////////////////////////////////////////////////////////////////////////////////
	// Rebuild the parameters
	////////////////////////////////////////////////////////////////////////////////
	ImGUIParameters* par = (ImGUIParameters*)parameters;
	Camera* camera = par->camera;
	uint32_t currentbuffer = par->currentBuffer;
	uint32_t* renderFlags = par->renderflags;
	uint32_t cascadeCount = par->settings->cascadeCount;
	float gridRegion = par->settings->gridRegion;
	AnisotropicVoxelTexture* avt = par->avt;
	float appversion = par->appversion;
	SwapChain* swapchain = par->swapchain;
	glm::uvec2 screenres = *par->screenres;
	float dt = par->dt;
	RenderStatesTimeStamps* timeStamps = par->timeStamps;
	CVCTSettings* settings = par->settings;

	// offset
	float framerate = 1.0f / dt;
	

	deltaTimeValue[values_offset] = framerate;
	timestampValue[values_offset] = *timeStamps;

	////////////////////////////////////////////////////////////////////////////////
	// Record command buffer
	////////////////////////////////////////////////////////////////////////////////
	// Clear the current frame and depth buffer
	VkCommandBufferBeginInfo cmdBufInfo = {};
	cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	ImGui_ImplGlfwVulkan_NewFrame();

	ImGuiWindowFlags window_flags = 0;
	window_flags |= ImGuiWindowFlags_NoTitleBar;
	window_flags |= ImGuiWindowFlags_ShowBorders;
	window_flags |= ImGuiWindowFlags_NoMove;
	window_flags |= ImGuiWindowFlags_AlwaysAutoResize;
	window_flags |= ImGuiWindowFlags_MenuBar;
	ImGui::SetNextWindowSize(ImVec2(350, 480), ImGuiSetCond_FirstUseEver);
	ImGui::SetNextWindowPos(ImVec2(6, 6));
	bool begin = false;
	if (!ImGui::Begin("ImGui Demo", &begin, window_flags))
	{
		// Early out if the window is collapsed, as an optimization.
		ImGui::End();
		return;
	}

	{
		// Header
		{
			{
				ImGui::Text("Application %.3f ms/frame (%.1f FPS)", dt*1000.0f, framerate);
				ImGui::PlotLines("", deltaTimeValue, VALUESIZE, values_offset, "", 0.0, 60.0f, ImVec2(0, 80));
			}
		}
		if (ImGui::CollapsingHeader("Renderers"))
		{
			//	int all &= & RenderFlags::RENDER_CONETRACE & RenderFlags::RENDER_MAIN;
			int cur = *renderFlags & (RenderFlags::RENDER_FORWARD + RenderFlags::RENDER_CONETRACE + RenderFlags::RENDER_FORWARDMAIN + RenderFlags::RENDER_DEFERREDMAIN);
			if (ImGui::TreeNode("Forward Debug Renderer"))
			{
				ImGui::RadioButton("Enable", &cur, RenderFlags::RENDER_FORWARD);

				int debugoption = *renderFlags & (RenderFlags::RENDER_VOXELDEBUG + RenderFlags::RENDER_VOXELCASCADE);
				ImGui::RadioButton("Debug Voxels", &debugoption, RenderFlags::RENDER_VOXELDEBUG);		
				ImGui::SameLine(); ImGui::RadioButton("None", &debugoption, RenderFlags::RENDER_VOXELCASCADE);
				//ImGui::SameLine();ImGui::RadioButton("Debug Cascade", &debugoption, RenderFlags::RENDER_VOXELCASCADE);

				*renderFlags &= ~RenderFlags::RENDER_VOXELDEBUG & ~RenderFlags::RENDER_VOXELCASCADE;
				*renderFlags |= debugoption;

				ImGui::Text("Use the arrow keys to switch which anisotropic voxel face is being rendered.");
				ImGui::Text("For front and back voxels, hold down shift.");
				ImGui::Text("Use the numbers 1,2,3 to switch the MIP level of the voxels.");

				ImGui::TreePop();
			}

			if (ImGui::TreeNode("Cone Tracer"))
			{
				ImGui::RadioButton("Enable", &cur, RenderFlags::RENDER_CONETRACE);
				ImGui::Separator();

				// options
				// change cone aperture scale
				// fixed cascade
				// fixed mip

				ImGui::TreePop();
			}
			if (ImGui::TreeNode("Forward Main Renderer"))
			{
				ImGui::RadioButton("Enable", &cur, RenderFlags::RENDER_FORWARDMAIN);
				ImGui::Separator();

				ImGui::TreePop();

				// options
				// change cone number
				// change cone scalar
			}
			if (ImGui::TreeNode("Deferred Main Renderer"))
			{
				ImGui::RadioButton("Enable", &cur, RenderFlags::RENDER_DEFERREDMAIN);
				ImGui::Separator();

				static int e = settings->deferredRender;
				if (ImGui::TreeNode("Options"))
				{
					ImGui::RadioButton("All", &e, 0); ImGui::SameLine();
					ImGui::RadioButton("AO", &e, 1); ImGui::SameLine();
					ImGui::RadioButton("Indir", &e, 2);

					settings->deferredRender = (uint32_t)e;

					ImGui::TreePop();
				}

				ImGui::TreePop();

				// options
				// change cone number
				// change cone scalar
			}
			// turn off all flags
			// Todo: give function pointer
			*renderFlags &= ~RenderFlags::RENDER_FORWARD & ~RenderFlags::RENDER_CONETRACE & ~RenderFlags::RENDER_FORWARDMAIN & ~RenderFlags::RENDER_DEFERREDMAIN;
			*renderFlags |= cur;
		}

		if (ImGui::CollapsingHeader("Pipeline State Timing"))
		{
			if (timestampValue[values_offset].voxelizerTimestamp != 0.0)
			{
				// voxelizer timer
				ImGui::Text("Voxelizer Time Stamp %.3f ms/frame", timestampValue[values_offset].voxelizerTimestamp);
				ImGui::PlotLines("", [](void*data, int idx) { RenderStatesTimeStamps* tmp = (RenderStatesTimeStamps*)data; return tmp[idx].voxelizerTimestamp; }, &timestampValue, VALUESIZE, values_offset, "", 0.0, 60.0f, ImVec2(0, 40));
			}
			if (timestampValue[values_offset].postVoxelizerTimestamp != 0.0)
			{
				// post voxelizer timer
				ImGui::Text("Post Voxelizer Time Stamp %.3f ms/frame", timestampValue[values_offset].postVoxelizerTimestamp);
				ImGui::PlotLines("", [](void*data, int idx) { RenderStatesTimeStamps* tmp = (RenderStatesTimeStamps*)data; return tmp[idx].postVoxelizerTimestamp; }, &timestampValue, VALUESIZE, values_offset, "", 0.0, 60.0f, ImVec2(0, 40));
			}
			if (timestampValue[values_offset].mipMapperTimestamp != 0.0)
			{
				// Mipmapper voxelizer timer
				ImGui::Text("Voxel Mipmapper Time Stamp %.3f ms/frame", timestampValue[values_offset].mipMapperTimestamp);
				ImGui::PlotLines("", [](void*data, int idx) { RenderStatesTimeStamps* tmp = (RenderStatesTimeStamps*)data; return tmp[idx].mipMapperTimestamp; }, &timestampValue, VALUESIZE, values_offset, "", 0.0, 60.0f, ImVec2(0, 40));
			}
			if (timestampValue[values_offset].forwardRendererTimestamp != 0.0)
			{
				// Forward renderer timer
				ImGui::Text("Forward Renderer Time Stamp %.3f ms/frame", timestampValue[values_offset].forwardRendererTimestamp);
				ImGui::PlotLines("", [](void*data, int idx) { RenderStatesTimeStamps* tmp = (RenderStatesTimeStamps*)data; return tmp[idx].forwardRendererTimestamp; }, &timestampValue, VALUESIZE, values_offset, "", 0.0, 60.0f, ImVec2(0, 40));
			}
			if (timestampValue[values_offset].conetracerTimestamp != 0.0)
			{
				// Cone Tracer timer
				ImGui::Text("Cone Tracer Time Stamp %.3f ms/frame", timestampValue[values_offset].conetracerTimestamp);
				ImGui::PlotLines("", [](void*data, int idx) { RenderStatesTimeStamps* tmp = (RenderStatesTimeStamps*)data; return tmp[idx].conetracerTimestamp; }, &timestampValue, VALUESIZE, values_offset, "", 0.0, 60.0f, ImVec2(0, 40));
			}
			if (timestampValue[values_offset].forwardMainRendererTimestamp != 0.0)
			{
				// Forward Main renderer timer
				ImGui::Text("Forward Main Renderer Time Stamp %.3f ms/frame", timestampValue[values_offset].forwardMainRendererTimestamp);
				ImGui::PlotLines("", [](void*data, int idx) { RenderStatesTimeStamps* tmp = (RenderStatesTimeStamps*)data; return tmp[idx].forwardMainRendererTimestamp; }, &timestampValue, VALUESIZE, values_offset, "", 0.0, 60.0f, ImVec2(0, 40));
			}
			if (timestampValue[values_offset].deferredMainRendererTimestamp != 0.0)
			{
				// Deferred Main Renderer Timer
				ImGui::Text("Deferred Main Renderer Time Stamp %.3f ms/frame", timestampValue[values_offset].deferredMainRendererTimestamp);
				ImGui::PlotLines("", [](void*data, int idx) { RenderStatesTimeStamps* tmp = (RenderStatesTimeStamps*)data; return tmp[idx].deferredMainRendererTimestamp; }, &timestampValue, VALUESIZE, values_offset, "", 0.0, 60.0f, ImVec2(0, 40));
			}
		}

		if (ImGui::CollapsingHeader("Options"))
		{
			if (ImGui::TreeNode("General Application Options"))
			{
				// change cascade number
				// change base region size
				// change voxel grid size
				// change the
				static int slidercone = settings->conecount;
				if (ImGui::Button("Apply Cones"))
				{
					settings->conecount = slidercone;
				}
				ImGui::SameLine();
				ImGui::SliderInt("Cone Count", &slidercone, 1, 31);
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("Voxelizer Options"))
			{
				// change cascade number
				static int slidercascade = settings->cascadeCount;
				if (ImGui::Button("Apply Cascade"))
				{
					settings->cascadeCount = slidercascade;
				}
				ImGui::SameLine(); 
				ImGui::SliderInt("Cascade Count",&slidercascade, 1, 4);
				
				// change base region size
				static float sliderregion = settings->gridRegion;
				if (ImGui::Button("Apply Region"))
				{
					settings->gridRegion = sliderregion;
				}
				ImGui::SameLine();
				ImGui::SliderFloat("Region Size", &sliderregion, 4.0f, 32.0f);

				// change voxel grid size
				static int slidergrid = settings->gridSize;
				if (ImGui::Button("Apply Grid"))
				{
					settings->gridSize = slidergrid;
				}
				ImGui::SameLine();
				ImGui::SliderInt("Grid Size", &slidergrid, 32, 128);

				ImGui::TreePop();
			}
		}

		ImGui::Text("");
		ImGui::Separator();
		//Info
		ImGui::Text("Window Info:");
		ImGui::Text("Application Version       %.02f ", appversion);
		ImGui::Text("Window Resolution         %i x %i ", screenres.x, screenres.y);
		ImGui::Text("");

		ImGui::Text("Camera Info:");
		glm::vec3 campos = camera->GetPosition();
		ImGui::Text("Camera Position           [%.04f, %.04f, %.04f] ", campos.x, campos.y, campos.z);
		ImGui::Text("Vertical FOV (Degrees)    %.02f", 45.0);
		ImGui::Text("");

		ImGui::Text("Rendering Info:");
		ImGui::Text("Number of Framebuffers    %i", swapchain->m_imageCount);
		ImGui::Text("Current Cascade Count     %i", cascadeCount);
		ImGui::Text("Base Region Size          %.01f", gridRegion);
		ImGui::Text("Voxel Resolution          [%i, %i, %i]", avt->m_width, avt->m_height, avt->m_depth);
		ImGui::Text("Cone Count                %i", settings->conecount);
		ImGui::Text("");

		ImGui::End();
	}

	// add
	values_offset = (values_offset + 1) % VALUESIZE;


	VK_CHECK_RESULT(vkBeginCommandBuffer(renderState->m_commandBuffers[currentbuffer], &cmdBufInfo));
	// Start the first sub pass specified in our default render pass setup by the base class
	// This will clear the color and depth attachment
	VkClearValue clearValues[2];
	VkClearColorValue defaultClearColor = { { 0.025f, 0.025f, 0.025f, 1.0f } };
	clearValues[0].color = defaultClearColor;
	clearValues[1].depthStencil = { 1.0f, 0 };
	VkRenderPassBeginInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	info.renderPass = renderState->m_renderpass;
	info.framebuffer = framebuffers[currentbuffer];
	info.renderArea.extent.width = swapchain->m_width;
	info.renderArea.extent.height = swapchain->m_height;
	info.clearValueCount = 2;
	info.pClearValues = clearValues;
	vkCmdBeginRenderPass(renderState->m_commandBuffers[currentbuffer], &info, VK_SUBPASS_CONTENTS_INLINE);
	//render
	ImGui_ImplGlfwVulkan_Render(renderState->m_commandBuffers[currentbuffer]);
	//end
	vkCmdEndRenderPass(renderState->m_commandBuffers[currentbuffer]);
	//VK_CHECK_RESULT(vkEndCommandBuffer(ImGUIState.m_commandBuffers[m_currentBuffer]));
	//flush
	VKTools::FlushCommandBuffer(renderState->m_commandBuffers[currentbuffer], core->GetGraphicsQueue(), core->GetViewDevice(), core->GetGraphicsCommandPool(), false);
}

void CreateImgGUIPipelineState(
	RenderState& renderState,
	uint32_t framebufferCount,
	VulkanCore* core,
	VkCommandPool commandPool,
	SwapChain* swapchain)
{
	uint32_t width = swapchain->m_width;
	uint32_t height = swapchain->m_height;

	////////////////////////////////////////////////////////////////////////////////
	// Create the pipelineCache
	////////////////////////////////////////////////////////////////////////////////
	// create a default pipelinecache
	if (!renderState.m_pipelineCache)
	{
		VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
		pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
		VK_CHECK_RESULT(vkCreatePipelineCache(core->GetViewDevice(), &pipelineCacheCreateInfo, NULL, &renderState.m_pipelineCache));
	}

	////////////////////////////////////////////////////////////////////////////////
	// Create commandbuffers and semaphores
	////////////////////////////////////////////////////////////////////////////////
	// Set the framebuffer
	renderState.m_framebuffers = NULL;
	renderState.m_framebufferCount = 0;

	// Create the semaphore
	if (!renderState.m_commandBuffers)
	{
		renderState.m_commandBufferCount = framebufferCount;
		renderState.m_commandBuffers = (VkCommandBuffer*)malloc(sizeof(VkCommandBuffer)*renderState.m_commandBufferCount);
		for (uint32_t i = 0; i < renderState.m_commandBufferCount; i++)
			renderState.m_commandBuffers[i] = VKTools::Initializers::CreateCommandBuffer(commandPool, core->GetViewDevice(), VK_COMMAND_BUFFER_LEVEL_PRIMARY, false);
	}
	if (!renderState.m_semaphores)
	{
		renderState.m_semaphoreCount = 1;
		renderState.m_semaphores = (VkSemaphore*)malloc(sizeof(VkSemaphore)*renderState.m_semaphoreCount);
		VkSemaphoreCreateInfo semInfo = VKTools::Initializers::SemaphoreCreateInfo();
		for (uint32_t i = 0; i < renderState.m_semaphoreCount; i++)
			vkCreateSemaphore(core->GetViewDevice(), &semInfo, NULL, &renderState.m_semaphores[i]);
	}

	////////////////////////////////////////////////////////////////////////////////
	// Create the renderpass
	////////////////////////////////////////////////////////////////////////////////
	if (!renderState.m_renderpass)
	{
		VkAttachmentDescription attachments[2] = {};
		// Color attachment
		attachments[0].format = core->m_colorFormat;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;									// We don't use multi sampling in this example
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;							// Clear this attachment at the start of the render pass
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;							// Keep it's contents after the render pass is finished (for displaying it)
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;					// We don't use stencil, so don't care for load
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;				// Same for store
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;						// Layout at render pass start. Initial doesn't matter, so we use undefined
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;					// Layout to which the attachment is transitioned when the render pass is finished
																						// As we want to present the color buffer to the swapchain, we transition to PRESENT_KHR	
		attachments[1].format = core->m_depthFormat;
		attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;							// Clear depth at start of first subpass
		attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;						// We don't need depth after render pass has finished (DONT_CARE may result in better performance)
		attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;					// No stencil
		attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;				// No Stencil
		attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;						// Layout at render pass start. Initial doesn't matter, so we use undefined
		attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;	// Transition to depth/stencil attachment

		VkAttachmentReference colorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
		VkAttachmentReference depthReference = { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.flags = 0;
		subpass.inputAttachmentCount = 0;
		subpass.pInputAttachments = NULL;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorReference;
		subpass.pResolveAttachments = NULL;
		subpass.pDepthStencilAttachment = &depthReference;
		subpass.preserveAttachmentCount = 0;
		subpass.pPreserveAttachments = NULL;

		VkSubpassDependency dependencies[2];
		dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[0].dstSubpass = 0;
		dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		dependencies[1].srcSubpass = 0;
		dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		VkRenderPassCreateInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.pNext = NULL;
		renderPassInfo.attachmentCount = 2;
		renderPassInfo.pAttachments = attachments;
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 2;
		renderPassInfo.pDependencies = dependencies;

		VK_CHECK_RESULT(vkCreateRenderPass(core->GetViewDevice(), &renderPassInfo, NULL, &renderState.m_renderpass));
	}
	
	////////////////////////////////////////////////////////////////////////////////
	// Create descriptor pool
	////////////////////////////////////////////////////////////////////////////////
	if (!renderState.m_descriptorPool)
	{
		VkDescriptorPoolSize poolSize[11] =
		{
			{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
			{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
		};
		VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = VKTools::Initializers::DescriptorPoolCreateInfo(0, 1000 * 11, 11, poolSize);
		VK_CHECK_RESULT(vkCreateDescriptorPool(core->GetViewDevice(), &descriptorPoolCreateInfo, NULL, &renderState.m_descriptorPool));
	}

	// Initialize imgui
	ImGui_ImplGlfwVulkan_Init_Data init_data = {};
	init_data.allocator = NULL;
	init_data.gpu = core->GetPhysicalGPU();
	init_data.device = core->GetViewDevice();
	init_data.render_pass = renderState.m_renderpass;
	init_data.pipeline_cache = renderState.m_pipelineCache;
	init_data.descriptor_pool = renderState.m_descriptorPool;
	init_data.check_vk_result = check_vk_result;
	ImGui_ImplGlfwVulkan_Init(core->GetGLFWWindow(), true, &init_data);

	////////////////////////////////////////////////////////////////////////////////
	// Build command buffers
	////////////////////////////////////////////////////////////////////////////////
	VkCommandBufferBeginInfo begin_info = {};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	VK_CHECK_RESULT(vkBeginCommandBuffer(renderState.m_commandBuffers[0], &begin_info));

	ImGui_ImplGlfwVulkan_CreateFontsTexture(renderState.m_commandBuffers[0]);

	VkSubmitInfo end_info = {};
	end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	end_info.commandBufferCount = 1;
	end_info.pCommandBuffers = &renderState.m_commandBuffers[0];
	VK_CHECK_RESULT(vkEndCommandBuffer(renderState.m_commandBuffers[0]));
	VK_CHECK_RESULT(vkQueueSubmit(core->GetGraphicsQueue(), 1, &end_info, VK_NULL_HANDLE));
	
	VK_CHECK_RESULT(vkDeviceWaitIdle(core->GetViewDevice()));
	ImGui_ImplGlfwVulkan_InvalidateFontUploadObjects();

	////////////////////////////////////////////////////////////////////////////////
	// Build command buffers
	////////////////////////////////////////////////////////////////////////////////
	ImGUIParameters* parameter;
	parameter = (ImGUIParameters*)malloc(sizeof(ImGUIParameters));
	renderState.m_cmdBufferParameters = (BYTE*)parameter;

	renderState.m_CreateCommandBufferFunc = &BuildCommandBufferImGUIState;
}