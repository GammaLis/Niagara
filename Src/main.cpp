// Ref: https://vulkan-tutorial.com
// Ref: https://github.com/KhronosGroup/Vulkan-Samples
// Ref: https://developer.nvidia.com/blog/introduction-turing-mesh-shaders/

#include "pch.h"
#include "Device.h"
#include "Swapchain.h"
#include "Shaders.h"
#include "RenderPass.h"
#include "Pipeline.h"
#include "CommandManager.h"
#include "Image.h"
#include "Camera.h"
#include "Utilities.h"
#include "Config.h"
#include "Geometry.h"

#include <iostream>
#include <fstream>
#include <queue>

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <glm/glm.hpp>
#include <glm/gtc/random.hpp>

using namespace Niagara;


const std::string g_ResourcePath = "../Resources/";
const std::string g_ShaderPath = "./CompiledShaders/";

GLFWwindow* g_Window = nullptr;
VkExtent2D g_ViewportSize{};
bool g_FramebufferResized = false;
double g_DeltaTime = 0.0;

// Window callbacks
void FramebufferResizeCallback(GLFWwindow *window, int width, int height)
{
	g_FramebufferResized = true;
}

void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	using Niagara::CameraManipulator;

	const bool pressed = action != GLFW_RELEASE;

	if (!pressed) return;

	CameraManipulator::Inputs &inputs = Niagara::g_Inputs;
	inputs.ctrl	 = mods & GLFW_MOD_CONTROL;
	inputs.shift = mods & GLFW_MOD_SHIFT;
	inputs.alt	 = mods & GLFW_MOD_ALT;

	auto& cameraManip = CameraManipulator::Singleton();

	auto factor = static_cast<float>(g_DeltaTime);

	// For all pressed keys - apply the action
	// cameraManip.KeyMotion(0, 0, CameraManipulator::Actions::NoAction);
	switch (key)
	{
	case GLFW_KEY_ESCAPE:
		glfwSetWindowShouldClose(window, GLFW_TRUE);
		break;
	case GLFW_KEY_W:
		cameraManip.KeyMotion(factor, 0, CameraManipulator::Actions::Dolly);
		break;
	case GLFW_KEY_S:
		cameraManip.KeyMotion(-factor, 0, CameraManipulator::Actions::Dolly);
		break;
	case GLFW_KEY_A:
	case GLFW_KEY_LEFT:
		cameraManip.KeyMotion(-factor, 0, CameraManipulator::Actions::Pan);
		break;
	case GLFW_KEY_UP:
		cameraManip.KeyMotion(0, factor, CameraManipulator::Actions::Pan);
		break;
	case GLFW_KEY_D:
	case GLFW_KEY_RIGHT:
		cameraManip.KeyMotion(factor, 0, CameraManipulator::Actions::Pan);
		break;
	case GLFW_KEY_DOWN:
		cameraManip.KeyMotion(0, -factor, CameraManipulator::Actions::Pan);
		break;
	}
}

void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
	using Niagara::CameraManipulator;

	double xpos, ypos;
	glfwGetCursorPos(window, &xpos, &ypos);
	CameraManipulator::Singleton().SetMousePosition(static_cast<int>(xpos), static_cast<int>(ypos));

	CameraManipulator::Inputs& inputs = Niagara::g_Inputs;
	inputs.lmb = (button == GLFW_MOUSE_BUTTON_LEFT) && (action == GLFW_PRESS);
	inputs.mmb = (button == GLFW_MOUSE_BUTTON_MIDDLE) && (action == GLFW_PRESS);
	inputs.rmb = (button == GLFW_MOUSE_BUTTON_RIGHT) && (action == GLFW_PRESS);
}

void WindowCloseCallback(GLFWwindow* window)
{
	glfwSetWindowShouldClose(window, GLFW_TRUE);
}

void CursorPosCallback(GLFWwindow *window, double xpos, double ypos)
{
	using Niagara::CameraManipulator;

	const auto& inputs = Niagara::g_Inputs;

	if (inputs.lmb || inputs.mmb || inputs.rmb)
	{
		CameraManipulator::Singleton().MouseMove(static_cast<int>(xpos), static_cast<int>(ypos), inputs);
	}
}


// Vulkan
#ifdef NDEBUG
const bool g_bEnableValidationLayers = false;
#else
const bool g_bEnableValidationLayers = true;
#endif

VkPhysicalDevice g_PhysicalDevice = VK_NULL_HANDLE;
VkDevice g_Device = VK_NULL_HANDLE;

VkCommandPool g_CommandPool = VK_NULL_HANDLE;

// Shaders
struct ShaderManager
{
	Niagara::Shader meshVert;
	Niagara::Shader meshFrag;
	Niagara::Shader meshTask;
	Niagara::Shader meshMesh;

	Niagara::Shader cullComp;
	Niagara::Shader buildHiZComp;

	void Init(const Niagara::Device& device)
	{
		meshTask.Load(device, g_ShaderPath + "SimpleMesh.task.spv");
		meshMesh.Load(device, g_ShaderPath + "SimpleMesh.mesh.spv");
		meshVert.Load(device, g_ShaderPath + "SimpleMesh.vert.spv");
		meshFrag.Load(device, g_ShaderPath + "SimpleMesh.frag.spv");

		cullComp.Load(device, g_ShaderPath + "DrawCommand.comp.spv");
		buildHiZComp.Load(device, g_ShaderPath + "HiZBuild.comp.spv");
	}

	void Cleanup(const Niagara::Device& device)
	{
		meshVert.Cleanup(device);
		meshFrag.Cleanup(device);
		meshTask.Cleanup(device);
		meshMesh.Cleanup(device);

		cullComp.Cleanup(device);
		buildHiZComp.Cleanup(device);
	}
};
ShaderManager g_ShaderMgr{};

// Samplers
struct CommonStates
{
	Niagara::Sampler linearClampSampler;
	Niagara::Sampler linearRepeatSampler;
	Niagara::Sampler pointClampSampler;
	Niagara::Sampler pointRepeatSampler;

	void Init(const Niagara::Device &device)
	{
		linearClampSampler.Init(device, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER);
		linearRepeatSampler.Init(device, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT);
		pointClampSampler.Init(device, VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER);
		pointRepeatSampler.Init(device, VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST, VK_SAMPLER_ADDRESS_MODE_REPEAT);
	}

	void Destroy(const Niagara::Device &device)
	{
		linearClampSampler.Destroy(device);
		linearRepeatSampler.Destroy(device);
		pointClampSampler.Destroy(device);
		pointRepeatSampler.Destroy(device);
	}
};
CommonStates g_CommonStates{};

using Niagara::g_CommandMgr;
using Niagara::g_CommandContext;

// How many frames 
constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;
// We choose the number of 2 because we don't want the CPU to get too far ahead of the GPU. With 2 frames in flight, the CPU and GPU
// can be working on their own tasks at the same time. If the CPU finishes early, it will wait till the GPU finishes rendering before
// submitting more work.
// Each frame should have its own command buffer, set of semaphores, and fence.

static const uint32_t g_BufferSize = 128 * 1024 * 1024;

const std::vector<const char*> g_InstanceExtensions =
{
	VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
};

const std::vector<const char*> g_DeviceExtensions =
{
	VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
	VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME,
	
#if USE_MESHLETS
	// VK_NV_MESH_SHADER_EXTENSION_NAME,
	VK_EXT_MESH_SHADER_EXTENSION_NAME
#endif
};

std::vector<VkDynamicState> g_DynamicStates =
{
	VK_DYNAMIC_STATE_VIEWPORT,
	VK_DYNAMIC_STATE_SCISSOR
};

enum DescriptorBindings : uint32_t
{
	VertexBuffer		= 0,
	MeshBuffer			= 1,
	MeshDrawBuffer		= 2,
	MeshDrawArgsBuffer	= 3,
	MeshletBuffer		= 4,
	MeshletDataBuffer	= 5,

	// TODO: put these another place
	// Globals 
	ViewUniformBuffer	= 6
};


/// Structures

struct alignas(16) MeshDraw
{
	glm::vec4 worldMatRow0;
	glm::vec4 worldMatRow1;
	glm::vec4 worldMatRow2;
	
	int32_t  vertexOffset; // == meshes[meshIndex].vertexOffset, helps data locality in mesh shader
	uint32_t meshIndex;
};

struct MeshDrawCommand
{
	uint32_t drawId;
	VkDrawIndexedIndirectCommand drawIndexedIndirectCommand; // 5 uint32_t
	// Switch from MeshShaderNV to MeshShaderEXT
#if 0
	VkDrawMeshTasksIndirectCommandNV drawMeshTaskIndirectCommand; // 2 uint32_t
#else
	uint32_t taskOffset;
	uint32_t taskCount;
	VkDrawMeshTasksIndirectCommandEXT drawMeshTaskIndirectCommand; // 3 uint32_t
#endif
};


struct GpuBuffer
{
	VkBuffer buffer = VK_NULL_HANDLE;
	VkDeviceMemory memory = VK_NULL_HANDLE;
	void* data = nullptr;
	size_t size = 0;
	uint32_t stride = 0;
	uint32_t elementCount = 0;

	static void Copy(const Niagara::Device &device, GpuBuffer &dstBuffer, const GpuBuffer &srcBuffer, VkDeviceSize size)
	{
		VkCommandBuffer cmd = Niagara::BeginSingleTimeCommands();

		VkBufferCopy copyRegion{};
		copyRegion.srcOffset = 0;
		copyRegion.dstOffset = 0;
		copyRegion.size = size;
		vkCmdCopyBuffer(cmd, srcBuffer.buffer, dstBuffer.buffer, 1, &copyRegion);

		Niagara::EndSingleTimeCommands(cmd);
	}

	void Init(const Niagara::Device &device, uint32_t stride, uint32_t elementCount, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryFlags, const void *pInitialData = nullptr)
	{
		this->elementCount = elementCount;
		this->stride = stride;
		size = elementCount * stride;

		VkBufferCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		createInfo.size = size;
		createInfo.usage = usage;
		createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // Optional

		buffer = VK_NULL_HANDLE;
		VK_CHECK(vkCreateBuffer(device, &createInfo, nullptr, &buffer));

		VkMemoryRequirements memRequirements{};
		vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		
		VkBool32 memTypeFound = VK_FALSE;
		allocInfo.memoryTypeIndex = device.GetMemoryType(memRequirements.memoryTypeBits, memoryFlags, &memTypeFound);

		memory = VK_NULL_HANDLE;
		VK_CHECK(vkAllocateMemory(device, &allocInfo, nullptr, &memory));

		VK_CHECK(vkBindBufferMemory(device, buffer, memory, 0));

		data = nullptr;
		if (memoryFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
		{
			vkMapMemory(device, memory, 0, size, 0, &data);
		}

		if (pInitialData != nullptr)
			Update(device, pInitialData, stride, elementCount);
	}

	void Update(const Niagara::Device &device, const void *pData, uint32_t stride, uint32_t elementCount)
	{
		if (buffer == VK_NULL_HANDLE || pData == nullptr) 
			return;

		uint32_t size = stride * elementCount;

		if (data != nullptr)
		{
			memcpy_s(data, size, pData, size);
		}
		else
		{
			GpuBuffer scratchBuffer{};
			scratchBuffer.Init(device, stride, elementCount, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, pData);

			Copy(device, *this, scratchBuffer, size);

			scratchBuffer.Destroy(device);
		}
	}	

	void Destroy(const Niagara::Device &device)
	{
		if (memory != VK_NULL_HANDLE)
			vkFreeMemory(device, memory, nullptr);
		if (buffer != VK_NULL_HANDLE)
			vkDestroyBuffer(device, buffer, nullptr);
	}
};

struct alignas(16) ViewUniformBufferParameters
{
	glm::mat4 viewProjMatrix;
	glm::mat4 viewMatrix;
	glm::mat4 projMatrix;
	glm::vec4 frustumPlanes[6];
	glm::vec4 frustumValues; // X L/R plane -> (+/-X, 0, Z, 0), Y U/D plane -> (0, +/-Y, Z, 0)
	glm::vec4 zNearFar; // x - near, y - far, zw - not used
	glm::vec4 viewportRect;
	glm::vec4 debugValue;
	glm::vec3 camPos;
	uint32_t drawCount;
};
ViewUniformBufferParameters g_ViewUniformBufferParameters;

struct BufferManager 
{
	// Uniform buffers
	GpuBuffer viewUniformBuffer;

	// Storage buffers
	GpuBuffer vertexBuffer;
	GpuBuffer indexBuffer;
	GpuBuffer meshBuffer;
	GpuBuffer drawDataBuffer;
	GpuBuffer drawArgsBuffer;
	GpuBuffer drawCountBuffer;

#if USE_MESHLETS
	GpuBuffer meshletBuffer;
	GpuBuffer meshletDataBuffer;
#endif

	// View dependent textures
	Niagara::Image colorBuffer;
	Niagara::Image depthBuffer;
	Niagara::Image depthPyramid;

	void InitViewDependentBuffers(const Niagara::Device &device, const VkExtent2D &renderExtent, VkFormat colorFormat = VK_FORMAT_B8G8R8A8_SRGB, VkFormat depthFormat = VK_FORMAT_D32_SFLOAT_S8_UINT)
	{
		// Color texture
		colorBuffer.Init(device,
			VkExtent3D{ renderExtent.width, renderExtent.height, 1 },
			colorFormat,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
			0);
		auto colorView = colorBuffer.CreateImageView(device, VK_IMAGE_VIEW_TYPE_2D);

		// Depth texture
		depthBuffer.Init(
			device,
			VkExtent3D{ renderExtent.width, renderExtent.height, 1 },
			depthFormat,
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
			0);
		auto depthView = depthBuffer.CreateImageView(device, VK_IMAGE_VIEW_TYPE_2D);

		// Depth pyramid
		glm::uvec2 hzbSize;
		hzbSize.x = std::max(1u, Niagara::RoundUpToPowerOfTwo(renderExtent.width >> 1));
		hzbSize.y = std::max(1u, Niagara::RoundUpToPowerOfTwo(renderExtent.height >> 1));
		uint32_t numMips = std::max(1u, Niagara::FloorLog2(std::max(hzbSize.x, hzbSize.y)));
		depthPyramid.Init(device,
			VkExtent3D{ hzbSize.x, hzbSize.y, 1 },
			VK_FORMAT_R32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
			0,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, numMips);
		for (uint32_t i = 0; i < numMips; ++i)
			depthPyramid.CreateImageView(device, VK_IMAGE_VIEW_TYPE_2D, i);
	}

	void Cleanup(const Niagara::Device &device)
	{
		// Uniform buffers
		viewUniformBuffer.Destroy(device);

		// Storage buffers
		vertexBuffer.Destroy(device);
		indexBuffer.Destroy(device);
		meshBuffer.Destroy(device);
		drawDataBuffer.Destroy(device);
		drawArgsBuffer.Destroy(device);
		drawCountBuffer.Destroy(device);

#if USE_MESHLETS
		meshletBuffer.Destroy(device);
		meshletDataBuffer.Destroy(device);
#endif

		// View dependent textures
		colorBuffer.Destroy(device);
		depthBuffer.Destroy(device);
		depthPyramid.Destroy(device);
	}
};
BufferManager g_BufferMgr{};

struct PipelineManager
{
	Niagara::RenderPass meshDrawPass;
	Niagara::GraphicsPipeline meshDrawPipeline;
	
	Niagara::ComputePipeline updateDrawArgsPipeline;
	Niagara::ComputePipeline buildDepthPyramidPipeline;

	void Cleanup(const Niagara::Device &device)
	{
		meshDrawPipeline.Destroy(device);
		updateDrawArgsPipeline.Destroy(device);
		buildDepthPyramidPipeline.Destroy(device);

		meshDrawPass.Destroy(device);
	}
};
PipelineManager g_PipelineMgr{};


/**
* What it takes to draw a triangle
* 1. Instance and physical device selection
* 2. Logical device and queue families
* 3. Window surface and swap chain
* 4. Image views and framebuffers
* 5. Render passes
* 6. Graphics pipeline
* 7. Command pools and command buffers
* 8. Main loop
* 
* Specifically,
* * Create a `VkInstance`
* * Select a supported graphics card (`VkPhysicalDevice`)
* * Create a `VkDevice` and `VkQueue` for drawing and presentation
* * Create a window, window surface and swap chain
* * Wrap the swap chain images into `VkImageView`
* * Create a render pass that specifies the render targets and usage
* * Create the framebuffers for the render pass
* * Set up the graphics pipeline
* * Allocate and record a command buffer with the draw commands for every possible swap chain image 
* * Draw frames by acquiring images, submitting the right draw command buffers and returning the images back to the swap chain
*/

/**
* Coding conventions
* Functions have a lower case `vk` prefix, types like enumerations and structs have a `Vk` prefix and enumeration values have a
* `VK_` prefix. The API heavily uses structs to provide parameters to functions.
* VkXXXCreateInfo createInfo;
* createInfo.sType = VK_STRUCTURE_TYPE_XXX_CREATE_INFO;
* createInfo.pNext = nullptr;
* createInfo.foo = ...;
* createInfo.bar = ...;
* Many structures in Vulkan require you to explicitly specify the type of structure in the `sType` member. The `pNext` member
* can point to an extension structure and will always be `nullptr` in this tutorial.
* Almost all functions return a `VkResult` that is either `VK_SUCCESS` or an error code.
*/

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT * pCreateInfo, const VkAllocationCallbacks * pAllocator, VkDebugUtilsMessengerEXT * pDebugMessenger)
{
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr)
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	else
		return VK_ERROR_EXTENSION_NOT_PRESENT;
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) 
{
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr)
		func(instance, debugMessenger, pAllocator);
}

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, 
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData)
{
	std::cerr << "Validation layer: " << pCallbackData->pMessage << std::endl;
	return VK_FALSE;
}

void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
{
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT; // | VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	createInfo.pfnUserCallback = DebugCallback;
	createInfo.pUserData = nullptr; // Optional
}

VkDebugUtilsMessengerEXT SetupDebugMessenger(VkInstance instance)
{
	VkDebugUtilsMessengerCreateInfoEXT createInfo{};
	PopulateDebugMessengerCreateInfo(createInfo);

	VkDebugUtilsMessengerEXT debugMessenger;
	VK_CHECK(CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger));

	return debugMessenger;
}


#pragma region Pipeline

void GetMeshDrawPipeline(VkDevice device, Niagara::GraphicsPipeline& pipeline, Niagara::RenderPass &renderPass, uint32_t subpass, const VkRect2D& viewportRect)
{
#if DRAW_MODE == DRAW_SIMPLE_MESH

	// Pipeline shaders
#if USE_MESHLETS
	pipeline.taskShader = &g_ShaderMgr.meshTask;
	pipeline.meshShader = &g_ShaderMgr.meshMesh;
#else
	pipeline.vertShader = &g_ShaderMgr.meshVert;
#endif
	pipeline.fragShader = &g_ShaderMgr.meshFrag;
	
#endif

	// Pipeline states
	auto& pipelineState = pipeline.pipelineState;

	// Vertex input state
#if !VERTEX_INPUT_MODE
	pipelineState.bindingDescriptions = { Vertex::GetBindingDescription() };
	pipelineState.attributeDescriptions = Vertex::GetAttributeDescriptions();
#endif

	// Input assembly state
	// ...

	// Viewports and scissors
	pipelineState.viewports = { Niagara::GetViewport(viewportRect, 0, 1, FLIP_VIEWPORT) };
	pipelineState.scissors = { viewportRect };

	// Rasterization state
	auto &rasterizationState = pipelineState.rasterizationState;
	// >>> DEBUG:
	// rasterizationState.cullMode = VK_CULL_MODE_FRONT_BIT;
	// rasterizationState.polygonMode = VK_POLYGON_MODE_LINE;
#if FLIP_VIEWPORT
	rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
#endif

	// Depth stencil state
	if (renderPass.depthAttachment.format != VK_FORMAT_UNDEFINED)
	{
		auto& depthStencilState = pipelineState.depthStencilState;
		depthStencilState.depthTestEnable = VK_TRUE;
		depthStencilState.depthWriteEnable = VK_TRUE;
		depthStencilState.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
	}

	// Color blend state
#if 0
	{
		auto &blendStateAttachments = pipelineState.colorBlendAttachments;
		Niagara::ColorBlendAttachmentState blendAttachmentState{ VK_TRUE };
		blendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		blendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
		blendStateAttachments = { blendAttachmentState };
	}
#endif

	// Dynamic states
	// ...

	// Render pass
	pipeline.renderPass = &renderPass;
	pipeline.subpass = subpass;

	pipeline.Init(device);
}

#pragma endregion

VkFramebuffer GetFramebuffer(VkDevice device, const std::vector<VkImageView> &attachments, const VkExtent2D &size, VkRenderPass renderPass)
{
	VkFramebufferCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	createInfo.renderPass = renderPass;
	createInfo.pAttachments = attachments.data();
	createInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	createInfo.width = size.width;
	createInfo.height = size.height;
	createInfo.layers = 1;

	VkFramebuffer framebuffer;
	VK_CHECK(vkCreateFramebuffer(device, &createInfo, nullptr, &framebuffer));

	return framebuffer;
}

void RecordCommandBuffer(VkCommandBuffer cmd, const std::vector<VkFramebuffer> &framebuffers, const Niagara::Swapchain &swapchain, uint32_t imageIndex, const Niagara::Geometry &geometry)
{
	g_CommandContext.BeginCommandBuffer(cmd);

	// Globals
	auto viewUniformBufferParams = Niagara::DescriptorInfo(g_BufferMgr.viewUniformBuffer.buffer);

	const auto& meshBuffer = g_BufferMgr.meshBuffer;
	const auto& drawDataBuffer = g_BufferMgr.drawDataBuffer;
	const auto& drawArgsBuffer = g_BufferMgr.drawArgsBuffer;
	const auto& drawCountBuffer = g_BufferMgr.drawCountBuffer;

	Niagara::DescriptorInfo meshBufferDescInfo(meshBuffer.buffer, VkDeviceSize(0), VkDeviceSize(meshBuffer.size));
	Niagara::DescriptorInfo drawBufferDescInfo(drawDataBuffer.buffer, VkDeviceSize(0), VkDeviceSize(drawDataBuffer.size));
	Niagara::DescriptorInfo drawArgsDescInfo(drawArgsBuffer.buffer, VkDeviceSize(0), VkDeviceSize(drawArgsBuffer.size));

	uint32_t groupsX = 1, groupsY = 1, groupsZ = 1;

	// Init indirect draw count
	{
		vkCmdFillBuffer(cmd, drawCountBuffer.buffer, VkDeviceSize(0), VkDeviceSize(4), 0);

		g_CommandContext.BufferBarrier(drawCountBuffer.buffer, VkDeviceSize(drawCountBuffer.size), VkDeviceSize(0),
			VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);

		g_CommandContext.PipelineBarriers(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
	}

	// Update draw args
	{
		const uint32_t GroupSize = 32;

		g_CommandContext.BindPipeline(cmd, g_PipelineMgr.updateDrawArgsPipeline);

		g_CommandContext.SetDescriptor(0, meshBufferDescInfo);
		g_CommandContext.SetDescriptor(1, drawBufferDescInfo);
		g_CommandContext.SetDescriptor(2, drawArgsDescInfo);
		
		Niagara::DescriptorInfo drawCountDescInfo(drawCountBuffer.buffer, VkDeviceSize(0), VkDeviceSize(drawCountBuffer.size));
		g_CommandContext.SetDescriptor(3, drawCountDescInfo);

		g_CommandContext.SetDescriptor(DescriptorBindings::ViewUniformBuffer, viewUniformBufferParams);

		g_CommandContext.PushDescriptorSetWithTemplate(cmd);

		groupsX = Niagara::DivideAndRoundUp(drawArgsBuffer.elementCount, GroupSize);
		vkCmdDispatch(cmd, groupsX, groupsY, groupsZ);

		g_CommandContext.BufferBarrier(drawArgsBuffer.buffer, VkDeviceSize(drawArgsBuffer.size), VkDeviceSize(0),
			VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT);
		g_CommandContext.BufferBarrier(drawCountBuffer.buffer, VkDeviceSize(drawCountBuffer.size), VkDeviceSize(0),
			VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT);

		g_CommandContext.PipelineBarriers(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT);
	}

	// Mesh draw pipeline
	auto &colorBuffer = g_BufferMgr.colorBuffer;
	auto &depthBuffer = g_BufferMgr.depthBuffer;

	g_CommandContext.ImageBarrier(colorBuffer.image, VK_IMAGE_ASPECT_COLOR_BIT, 
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 
		0, 0); // VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT

	bool hasStencilBuffer = Niagara::IsDepthStencilFormat(depthBuffer.format);
	VkImageAspectFlags depthAspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT | (hasStencilBuffer ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);
	VkImageLayout depthLayout = hasStencilBuffer ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
	g_CommandContext.ImageBarrier(depthBuffer.image, 
		depthAspectFlags,
		VK_IMAGE_LAYOUT_UNDEFINED, depthLayout,
		0, 0); // VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT

	g_CommandContext.PipelineBarriers(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT);

	auto viewportRect = VkRect2D{ VkOffset2D{0, 0}, swapchain.extent };

	// Set viewports & scissors
	VkViewport dynamicViewport = Niagara::GetViewport(viewportRect, 0, 1, FLIP_VIEWPORT);
	vkCmdSetViewport(cmd, 0, 1, &dynamicViewport);
	vkCmdSetScissor(cmd, 0, 1, &viewportRect);

	auto& meshDrawPipeline = g_PipelineMgr.meshDrawPipeline;

	const auto &renderPass = meshDrawPipeline.renderPass;

	// Clear values

	VkClearValue clearColor;
	clearColor.color = { 0.2f, 0.2f, 0.8f, 1.0f };

	std::vector<VkClearValue> clearValues(renderPass->GetAttachmentCount());
	if (renderPass->activeColorAttachmentCount > 0)
	{
		for (uint32_t i = 0; i < renderPass->activeColorAttachmentCount; ++i)
		{
			if (renderPass->colorLoadStoreInfos[i].loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR)
				clearValues[i] = clearColor;
		}
	}
	if (renderPass->depthAttachment.format != VK_FORMAT_UNDEFINED)
	{
		clearValues[renderPass->activeColorAttachmentCount].depthStencil = { 0.0f, 0 };
	}

	g_CommandContext.BeginRenderPass(cmd, meshDrawPipeline.renderPass->renderPass, framebuffers[0], viewportRect, clearValues);

	g_CommandContext.BindPipeline(cmd, meshDrawPipeline);

	// Descriptors

	// Globals
	g_CommandContext.SetDescriptor(DescriptorBindings::ViewUniformBuffer, viewUniformBufferParams, 0);

	// Meshes
	const auto& vb = g_BufferMgr.vertexBuffer;
	const auto& ib = g_BufferMgr.indexBuffer;

#if VERTEX_INPUT_MODE == 1
	auto vbInfo = Niagara::DescriptorInfo(vb.buffer, VkDeviceSize(0), VkDeviceSize(vb.size));
	g_CommandContext.SetDescriptor(DescriptorBindings::VertexBuffer, vbInfo);

#if USE_MESHLETS
	const auto& mb = g_BufferMgr.meshletBuffer;
	const auto& meshletDataBuffer = g_BufferMgr.meshletDataBuffer;

	g_CommandContext.SetDescriptor(DescriptorBindings::MeshBuffer, meshBufferDescInfo);

	auto mbInfo = Niagara::DescriptorInfo(mb.buffer, VkDeviceSize(0), VkDeviceSize(mb.size));
	g_CommandContext.SetDescriptor(DescriptorBindings::MeshletBuffer, mbInfo);

	auto meshletDataInfo = Niagara::DescriptorInfo(meshletDataBuffer.buffer, VkDeviceSize(0), VkDeviceSize(meshletDataBuffer.size));
	g_CommandContext.SetDescriptor(DescriptorBindings::MeshletDataBuffer, meshletDataInfo);

#endif

	// Indirect draws
#if USE_MULTI_DRAW_INDIRECT
	g_CommandContext.SetDescriptor(DescriptorBindings::MeshDrawBuffer, drawBufferDescInfo);
	g_CommandContext.SetDescriptor(DescriptorBindings::MeshDrawArgsBuffer, drawArgsDescInfo);
#endif // USE_MULTI_DRAW_INDIRECT

#endif

#if 1
	g_CommandContext.PushDescriptorSetWithTemplate(cmd, 0);
#else
	g_CommandContext.PushDescriptorSet(cmd, 0);
#endif

	VkDeviceSize offset = 0;
	vkCmdBindVertexBuffers(cmd, 0, 1, &vb.buffer, &offset);

#if USE_MESHLETS
	
#if USE_MULTI_DRAW_INDIRECT
	// vkCmdDrawMeshTasksIndirectNV(cmd, drawArgsBuffer.buffer, offsetof(MeshDrawCommand, drawMeshTaskIndirectCommand), drawArgsBuffer.elementCount, sizeof(MeshDrawCommand));
#if 0
	vkCmdDrawMeshTasksIndirectCountNV (cmd, drawArgsBuffer.buffer, offsetof(MeshDrawCommand, drawMeshTaskIndirectCommand), drawCountBuffer.buffer, VkDeviceSize(0), drawArgsBuffer.elementCount, sizeof(MeshDrawCommand));
#else
	vkCmdDrawMeshTasksIndirectCountEXT(cmd, drawArgsBuffer.buffer, offsetof(MeshDrawCommand, drawMeshTaskIndirectCommand), drawCountBuffer.buffer, VkDeviceSize(0), drawArgsBuffer.elementCount, sizeof(MeshDrawCommand));
#endif

#else
	uint32_t nTask = static_cast<uint32_t>(mesh.meshlets.size());
	vkCmdDrawMeshTasksNV(cmd, Niagara::DivideAndRoundUp(nTask, 32), 0);
#endif

#else // USE_MESHLETS
	if (ib.size > 0)
	{
		vkCmdBindIndexBuffer(cmd, ib.buffer, 0, VK_INDEX_TYPE_UINT32);

#if USE_MULTI_DRAW_INDIRECT
		// vkCmdDrawIndexedIndirect(cmd, drawArgsBuffer.buffer, offsetof(MeshDrawCommand, drawIndexedIndirectCommand), drawArgsBuffer.elementCount, sizeof(MeshDrawCommand));
		vkCmdDrawIndexedIndirectCount(cmd, drawArgsBuffer.buffer, offsetof(MeshDrawCommand, drawIndexedIndirectCommand), drawCountBuffer.buffer, VkDeviceSize(0), drawArgsBuffer.elementCount, sizeof(MeshDrawCommand));

#else
		vkCmdDrawIndexed(cmd, ib.elementCount, 1, 0, 0, 0);
#endif
	}
	else
		vkCmdDraw(cmd, vb.elementCount, 1, 0, 0);
#endif // USE_MESHLETS

	g_CommandContext.EndRenderPass(cmd);

#if 1
	// Generate depth pyramid
	{
		const auto& depthPyramid = g_BufferMgr.depthPyramid;
		
		g_CommandContext.BindPipeline(cmd, g_PipelineMgr.buildDepthPyramidPipeline);

		g_CommandContext.ImageBarrier(depthBuffer.image, 
			depthAspectFlags,
			depthLayout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);

		VkImageSubresourceRange subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		VkImageView srcImageView = VK_NULL_HANDLE;
		uint32_t w = depthPyramid.extent.width, h = depthPyramid.extent.height;
		const uint32_t GroupSize = 8;

		struct Constants
		{
			glm::vec4 srcSize;
			glm::vec2 viewportMaxBoundUV;
		} constants{};

		for (uint32_t i = 0; i < depthPyramid.subresource.mipLevel; ++i)
		{
			constants.viewportMaxBoundUV = glm::vec2(1.0f, 1.0f);

			VkImageLayout srcLayout = VK_IMAGE_LAYOUT_UNDEFINED;

			if (i == 0)
			{
				srcImageView = depthBuffer.views[0].view;

				srcLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

				constants.srcSize = Niagara::GetSizeAndInvSize(depthBuffer.extent.width, depthBuffer.extent.height);

				g_CommandContext.ImageBarrier(depthPyramid.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 0, VK_ACCESS_SHADER_WRITE_BIT);

				g_CommandContext.PipelineBarriers(cmd, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
			}
			else
			{
				srcImageView = depthPyramid.views[i-1].view;

				srcLayout = VK_IMAGE_LAYOUT_GENERAL;

				constants.srcSize = Niagara::GetSizeAndInvSize(w, h);

				subresourceRange.baseMipLevel = i-1;
				g_CommandContext.ImageBarrier(depthPyramid.image, subresourceRange, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);

				subresourceRange.baseMipLevel = i;
				g_CommandContext.ImageBarrier(depthPyramid.image, subresourceRange, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 0, VK_ACCESS_SHADER_READ_BIT);

				g_CommandContext.PipelineBarriers(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

				w = std::max(1u, w >> 1);
				h = std::max(1u, h >> 1);
			}

			g_CommandContext.PushConstants(cmd, "_Constants", 0, sizeof(constants), &constants);

			g_CommandContext.SetDescriptor(0, Niagara::DescriptorInfo(g_CommonStates.linearClampSampler.sampler, srcImageView, srcLayout));
			g_CommandContext.SetDescriptor(1, Niagara::DescriptorInfo(VK_NULL_HANDLE, depthPyramid.views[i].view, VK_IMAGE_LAYOUT_GENERAL));

			g_CommandContext.PushDescriptorSetWithTemplate(cmd, 0);

			groupsX = Niagara::DivideAndRoundUp(w, GroupSize);
			groupsY = Niagara::DivideAndRoundUp(h, GroupSize);
			vkCmdDispatch(cmd, groupsX, groupsY, groupsZ);
		}
	}

#endif

	// Update backbuffer
	{
		auto swapchainImage = swapchain.images[imageIndex];
		
		g_CommandContext.ImageBarrier(swapchainImage, VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
			0, VK_ACCESS_TRANSFER_WRITE_BIT);
		g_CommandContext.ImageBarrier(colorBuffer.image, VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);

		g_CommandContext.PipelineBarriers(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

		g_CommandContext.Blit(cmd, colorBuffer.image, swapchain.images[imageIndex], 
			VkRect2D{ {0, 0}, { colorBuffer.extent.width, colorBuffer.extent.height} }, { {0, 0}, swapchain.extent });

		g_CommandContext.ImageBarrier(swapchainImage, VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
			VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT);
		g_CommandContext.ImageBarrier(colorBuffer.image, VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
			VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_MEMORY_READ_BIT);

		g_CommandContext.PipelineBarriers(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT); // VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT
	}

	g_CommandContext.EndCommandBuffer(cmd);
}

struct SyncObjects
{
	VkSemaphore imageAvailableSemaphore;
	VkSemaphore renderFinishedSemaphore;
	VkFence inFlightFence;
	
	void Cleanup(VkDevice device)
	{
		vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);	
		vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);	
		vkDestroyFence(device, inFlightFence, nullptr);	
	}
};

VkSemaphore GetSemaphore(VkDevice device)
{
	VkSemaphoreCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkSemaphore semaphore = VK_NULL_HANDLE;
	VK_CHECK(vkCreateSemaphore(device, &createInfo, nullptr, &semaphore));

	return semaphore;
}

VkFence GetFence(VkDevice device, VkFenceCreateFlags flags = VK_FENCE_CREATE_SIGNALED_BIT)
{
	VkFenceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	// Create the fence in the signaled state, so the first call to `vkWaitForFences()` returns immediately since the fence is already signaled
	createInfo.flags = flags;

	VkFence fence = VK_NULL_HANDLE;
	VK_CHECK(vkCreateFence(device, &createInfo, nullptr, &fence));

	return fence;
}

SyncObjects GetSyncObjects(VkDevice device)
{
	SyncObjects syncObjects{};

	{
		VkSemaphoreCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VK_CHECK(vkCreateSemaphore(device, &createInfo, nullptr, &syncObjects.imageAvailableSemaphore));
		VK_CHECK(vkCreateSemaphore(device, &createInfo, nullptr, &syncObjects.renderFinishedSemaphore));
	}

	{
		syncObjects.inFlightFence = GetFence(device);
	}

	return syncObjects;
}

struct FrameResources
{
	VkCommandBuffer commandBuffer;
	SyncObjects syncObjects;

	void Cleanup(VkDevice device)
	{
		syncObjects.Cleanup(device);
	}
};

void GetFrameResources(const Niagara::Device &device, std::vector<FrameResources> &frameResources)
{
	frameResources.resize(MAX_FRAMES_IN_FLIGHT);
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
	{
		frameResources[i].commandBuffer = g_CommandMgr.GetCommandBuffer(device);
		frameResources[i].syncObjects = GetSyncObjects(device);
	}
}

VkQueryPool GetQueryPool(VkDevice device, uint32_t queryCount)
{
	VkQueryPoolCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
	createInfo.queryCount = queryCount;
	createInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
	// createInfo.pipelineStatistics = VkQueryPipelineStatisticFlagBits::

	VkQueryPool queryPool;
	VK_CHECK(vkCreateQueryPool(device, &createInfo, nullptr, &queryPool));

	return queryPool;
}


/// Main

void Render(VkCommandBuffer cmd, const std::vector<VkFramebuffer> &framebuffers, const Niagara::Swapchain& swapchain, uint32_t imageIndex, const Niagara::Geometry& geometry, VkQueue graphicsQueue, SyncObjects& syncObjects)
{
	vkResetCommandBuffer(cmd, 0);

	// Recording the command buffer
 	RecordCommandBuffer(cmd, framebuffers, swapchain, imageIndex, geometry);

	// Submitting the command buffer
	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore waitSemaphores[] = { syncObjects.imageAvailableSemaphore };
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_TRANSFER_BIT };
	submitInfo.waitSemaphoreCount  = ARRAYSIZE(waitSemaphores);
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;

	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cmd;

	VkSemaphore signalSemaphores[] = { syncObjects.renderFinishedSemaphore };
	submitInfo.pSignalSemaphores = signalSemaphores;
	submitInfo.signalSemaphoreCount = ARRAYSIZE(signalSemaphores);

	VK_CHECK(vkQueueSubmit(graphicsQueue, 1, &submitInfo, syncObjects.inFlightFence));
}


int main()
{
	std::cout << "Hello, Vulkan!" << std::endl;

	// Window
	int rc = glfwInit();
	if (rc == GLFW_FALSE) 
	{
		std::cout << "GLFW init failed!" << std::endl;
		return -1;
	}

	// Because GLFW was originally designed to create an OpenGL context, we need to tell it to not create an OpenGL context
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	auto* monitor = glfwGetPrimaryMonitor();
	const auto* videoMode = glfwGetVideoMode(monitor);
	GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Niagara", nullptr, nullptr);
	g_Window = window;
	// Inputs
	glfwSetWindowUserPointer(window, nullptr);
	glfwSetFramebufferSizeCallback(window, FramebufferResizeCallback);
	glfwSetKeyCallback(window, KeyCallback);
	glfwSetMouseButtonCallback(window, MouseButtonCallback);
	glfwSetWindowCloseCallback(window, WindowCloseCallback);
	glfwSetCursorPosCallback(window, CursorPosCallback);

	// Vulkan
	VK_CHECK(volkInitialize());

	VkInstance instance = GetVulkanInstance(g_InstanceExtensions, true);
	assert(instance);

	volkLoadInstance(instance);

	VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
	if (g_bEnableValidationLayers)
	{
		debugMessenger = SetupDebugMessenger(instance);
		assert(debugMessenger);
	}

	// Device features
	VkPhysicalDeviceFeatures physicalDeviceFeatures{};
	physicalDeviceFeatures.multiDrawIndirect = VK_TRUE;
	physicalDeviceFeatures.samplerAnisotropy = VK_TRUE;
	physicalDeviceFeatures.shaderInt16 = VK_TRUE;
	physicalDeviceFeatures.fillModeNonSolid = VK_TRUE;

	// Device extensions
	void* pNextChain = nullptr;
	void** pNext = &pNextChain;

	VkPhysicalDeviceVulkan13Features features13{};
	features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	features13.maintenance4 = VK_TRUE;
	features13.synchronization2 = VK_TRUE;
	features13.dynamicRendering = VK_TRUE;
	features13.synchronization2 = VK_TRUE;

	VkPhysicalDeviceVulkan12Features features12{};
	features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	features12.drawIndirectCount = VK_TRUE;
	features12.storageBuffer8BitAccess = VK_TRUE;
	features12.uniformAndStorageBuffer8BitAccess = VK_TRUE;
	features12.storagePushConstant8 = VK_TRUE;
	features12.shaderFloat16 = VK_TRUE;
	features12.shaderInt8 = VK_TRUE;
	features12.samplerFilterMinmax = VK_TRUE;
	features12.scalarBlockLayout = VK_TRUE;
	features12.bufferDeviceAddress = VK_TRUE;

	features13.pNext = &features12;

	VkPhysicalDeviceVulkan11Features features11{};
	features11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
	features11.storageBuffer16BitAccess = VK_TRUE;
	features11.uniformAndStorageBuffer16BitAccess = VK_TRUE;
	features11.storagePushConstant16 = VK_TRUE;
	// TODO: Can not create device if enabled
	// features11.storageInputOutput16 = VK_TRUE;
	features11.shaderDrawParameters = VK_TRUE;

	features12.pNext = &features11;

	*pNext = &features13;
	pNext = &features11.pNext;

#if USE_MESHLETS
	VkPhysicalDeviceMeshShaderFeaturesEXT featureMeshShader{};
	featureMeshShader.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;
	featureMeshShader.meshShader = VK_TRUE;
	featureMeshShader.taskShader = VK_TRUE;

	*pNext = &featureMeshShader;
	pNext = &featureMeshShader.pNext;
#endif

#if USE_FRAGMENT_SHADING_RATE
	VkPhysicalDeviceFragmentShadingRateFeaturesKHR featureSR{};
	featureSR.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR;
	featureSR.primitiveFragmentShadingRate = VK_TRUE;
	featureSR.pipelineFragmentShadingRate = VK_TRUE;

	*pNext = &featureSR;
	pNext = &featureSR.pNext;
#endif

	Niagara::Device device{};
	device.Init(instance, physicalDeviceFeatures, g_DeviceExtensions, pNextChain);

	volkLoadDevice(device);

	Niagara::Swapchain swapchain{};
	swapchain.Init(instance, device, window);

	g_ViewportSize = swapchain.extent;

	auto renderExtent = swapchain.extent;

	g_CommandMgr.Init(device);

	// Common states
	g_CommonStates.Init(device);

	// Shaders
	g_ShaderMgr.Init(device);

	// Retrieving queue handles
	VkQueue graphicsQueue = g_CommandMgr.graphicsQueue;
	assert(graphicsQueue);

	VkFormat colorFormat = swapchain.colorFormat;
	VkFormat depthFormat = device.GetSupportedDepthFormat(false);
	g_BufferMgr.InitViewDependentBuffers(device, renderExtent, colorFormat, depthFormat);

#if 1
	Niagara::RenderPass &meshDrawPass = g_PipelineMgr.meshDrawPass;
	std::vector<Niagara::Attachment> colorAttachments{ Niagara::Attachment{ colorFormat } };
	std::vector<Niagara::LoadStoreInfo> colorLoadStoreInfos{ Niagara::LoadStoreInfo{} };
	Niagara::Attachment depthAttachment{ depthFormat };
	Niagara::LoadStoreInfo depthLoadStoreInfo{};
	meshDrawPass.SetAttachments(
		colorAttachments.data(), static_cast<uint32_t>(colorAttachments.size()), colorLoadStoreInfos.data(),
		&depthAttachment, &depthLoadStoreInfo);
	meshDrawPass.Init(device);

	VkRenderPass renderPass = meshDrawPass.renderPass;

#else
	VkRenderPass renderPass = GetRenderPass(device, swapchain.colorFormat);
#endif

	assert(renderPass);

	// Framebuffer
	const uint32_t framebufferCount = 2;
	std::vector<VkFramebuffer> framebuffers(framebufferCount);
	for (uint32_t i = 0; i < framebufferCount; ++i)
	{
		std::vector<VkImageView> frameAttachments = { g_BufferMgr.colorBuffer.views[0].view, g_BufferMgr.depthBuffer.views[0].view };
		framebuffers[i] = GetFramebuffer(device, frameAttachments, renderExtent, renderPass);
	}
	
	// Pipeline
	Niagara::GraphicsPipeline &pipeline = g_PipelineMgr.meshDrawPipeline;
	GetMeshDrawPipeline(device, pipeline, meshDrawPass, 0, { {0, 0}, renderExtent });

	Niagara::ComputePipeline &updateDrawArgsPipeline = g_PipelineMgr.updateDrawArgsPipeline;
	updateDrawArgsPipeline.compShader = &g_ShaderMgr.cullComp;
	updateDrawArgsPipeline.Init(device);

	Niagara::ComputePipeline& buildDepthPyramidPipeline = g_PipelineMgr.buildDepthPyramidPipeline;
	buildDepthPyramidPipeline.compShader = &g_ShaderMgr.buildHiZComp;
	buildDepthPyramidPipeline.Init(device);

	// Command buffers

	// Creating the synchronization objects
	// We'll need one semaphore to signal that an image has been acquired from the swapchain and is ready for rendering, another one to signal that
	// rendering has finished and presentation can happen, and a fence to make sure only one frame is rendering at a time.
	SyncObjects syncObjects = GetSyncObjects(device);

	std::vector<FrameResources> frameResources;
	GetFrameResources(device, frameResources);

	VkQueryPool queryPool = GetQueryPool(device, 128);
	assert(queryPool);

	// Scenes
	
	// Camera
	glm::vec3 eye{ 0, 0, 10 };
	glm::vec3 center{ };
	glm::vec3 up{ 0, 1, 0 };
	auto& camera = Niagara::CameraManipulator::Singleton();
	camera.SetWindowSize((int)swapchain.extent.width, (int)swapchain.extent.height);
	camera.SetSpeed(0.02f);
	// Currently the camera far plane is Inf
	camera.m_ClipPlanes.x = 1.0f;
	camera.SetCamera( { eye, center, up, glm::radians(90.0f) } );
	
	// Buffers
	VkMemoryPropertyFlags hostVisibleMemPropertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	VkMemoryPropertyFlags deviceLocalMemPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	
	// Uniforms
	g_ViewUniformBufferParameters.viewportRect = glm::vec4(0.0f, 0.0f, renderExtent.width, renderExtent.height);
	g_ViewUniformBufferParameters.debugValue = glm::vec4(1.0f); // glm::vec4(0.8, 0.2, 0.2, 1);

	auto& viewUniformBuffer = g_BufferMgr.viewUniformBuffer;
	viewUniformBuffer.Init(device, sizeof(ViewUniformBufferParameters), 1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, hostVisibleMemPropertyFlags);

	// Geometry
	Geometry geometry{};
	
	const std::vector<std::string> objFileNames{ "kitten.obj", /*"bunny.obj"*/ }; // kitten bunny
	for (const auto& fileName : objFileNames)
	{
		bool bLoaded = LoadMesh(geometry, std::string(g_ResourcePath + fileName).data(), USE_MESHLETS);
		if (!bLoaded)
			std::cout << "Load mesh failed: " << fileName << std::endl;
	}

	if (geometry.meshes.empty())
	{
		std::cout << "ERROR::No mesh loaded!\n";
		return -1;
	}

	const uint32_t meshCount = static_cast<uint32_t>(geometry.meshes.size());

	GpuBuffer &vb = g_BufferMgr.vertexBuffer, &ib = g_BufferMgr.indexBuffer;
	vb.Init(device, sizeof(Vertex), static_cast<uint32_t>(geometry.vertices.size()), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, deviceLocalMemPropertyFlags, geometry.vertices.data());

	if (!geometry.indices.empty())
	{
		ib.Init(device, sizeof(uint32_t), static_cast<uint32_t>(geometry.indices.size()), VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, deviceLocalMemPropertyFlags, geometry.indices.data());
	}

	GpuBuffer& meshBuffer = g_BufferMgr.meshBuffer;
	meshBuffer.Init(device, sizeof(Mesh), static_cast<uint32_t>(geometry.meshes.size()), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, deviceLocalMemPropertyFlags, geometry.meshes.data());

	// Indirect draw command buffers
#if USE_MULTI_DRAW_INDIRECT
	// Preparing indirect draw commands
	const uint32_t DrawCount = DRAW_COUNT;
	const float SceneRadius = SCENE_RADIUS;

	g_ViewUniformBufferParameters.drawCount = DrawCount;

	std::vector<MeshDraw> meshDraws(DrawCount);
	for (uint32_t i = 0; i < DrawCount; ++i)
	{
		auto& draw = meshDraws[i];

		// World matrix
		auto t = glm::ballRand<float>(SceneRadius);
		auto s = glm::vec3(glm::linearRand(1.0f, 2.0f));
		
		auto theta = glm::radians(glm::linearRand<float>(0.0f, 180.0));
		auto axis = glm::sphericalRand(1.0f);
		auto r = glm::quat(cosf(theta), axis * sinf(theta));

		glm::mat4 worldMat = glm::translate(glm::mat4_cast(r) * glm::scale(glm::mat4(1.0f), s), t);
		draw.worldMatRow0 = glm::vec4(worldMat[0][0], worldMat[1][0], worldMat[2][0], worldMat[3][0]);
		draw.worldMatRow1 = glm::vec4(worldMat[0][1], worldMat[1][1], worldMat[2][1], worldMat[3][1]);
		draw.worldMatRow2 = glm::vec4(worldMat[0][2], worldMat[1][2], worldMat[2][2], worldMat[3][2]);

		// Draw command data
		uint32_t meshId = glm::linearRand<uint32_t>(0, meshCount-1);
		const Mesh& mesh = geometry.meshes[meshId];

		draw.meshIndex = meshId;
		draw.vertexOffset = mesh.vertexOffset;
	}
	// Indirect draw command buffer
	GpuBuffer& drawBuffer = g_BufferMgr.drawDataBuffer;
	drawBuffer.Init(device, sizeof(MeshDraw), static_cast<uint32_t>(meshDraws.size()), VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		deviceLocalMemPropertyFlags, meshDraws.data());

	GpuBuffer& drawArgsBuffer = g_BufferMgr.drawArgsBuffer;
	drawArgsBuffer.Init(device, sizeof(MeshDrawCommand), DrawCount, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, deviceLocalMemPropertyFlags);

	GpuBuffer& drawCountBuffer = g_BufferMgr.drawCountBuffer;
	drawCountBuffer.Init(device, 4, 1, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, deviceLocalMemPropertyFlags);
#endif

#if USE_MESHLETS
	GpuBuffer& meshletBuffer = g_BufferMgr.meshletBuffer;
	meshletBuffer.Init(device, sizeof(Meshlet), static_cast<uint32_t>(geometry.meshlets.size()), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, deviceLocalMemPropertyFlags, geometry.meshlets.data());

	GpuBuffer& meshletDataBuffer = g_BufferMgr.meshletDataBuffer;
	meshletDataBuffer.Init(device, 4, static_cast<uint32_t>(geometry.meshletData.size()), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, deviceLocalMemPropertyFlags, geometry.meshletData.data());
#endif

	uint32_t currentFrame = 0;
	double currentFrameTime = glfwGetTime() * 1000.0;

	// Resize
	auto windowResize = [&](SyncObjects &currentSyncObjects) 
	{
		vkDeviceWaitIdle(device);

		// Recreate swapchain
		swapchain.UpdateSwapchain(device, window, false);

		renderExtent = swapchain.extent;
		colorFormat = swapchain.colorFormat;

		g_ViewUniformBufferParameters.viewportRect = glm::vec4(0.0f, 0.0f, renderExtent.width, renderExtent.height);

		// recreate view dependent textures
		g_BufferMgr.InitViewDependentBuffers(device, renderExtent, colorFormat, depthFormat);

		// Recreate framebuffers
		{
			for (auto& framebuffer : framebuffers)
				vkDestroyFramebuffer(device, framebuffer, nullptr);
			
			framebuffers.resize(framebufferCount);
			
			for (uint32_t i = 0; i < framebufferCount; ++i)
			{
				std::vector<VkImageView> frameAttachments = { g_BufferMgr.colorBuffer.views[0].view, g_BufferMgr.depthBuffer.views[0].view };
				framebuffers[i] = GetFramebuffer(device, frameAttachments, renderExtent, renderPass);
			}
		}

		// Recreate semaphores
		{
			vkDestroySemaphore(device, currentSyncObjects.imageAvailableSemaphore, nullptr);

			currentSyncObjects.imageAvailableSemaphore = GetSemaphore(device);
			assert(currentSyncObjects.imageAvailableSemaphore);
		}

		g_FramebufferResized = false;
	};

	// SPACE

	// Main loop
	/**
	 * Outline of a frame
	 * * Wait for the previous frame to finish
	 * * Acquire an image from the swap chain
	 * * Record a command buffer which draws the scene onto that image
	 * * Submit the recorded command buffer
	 * * Present the swap chain image
	 */
	while (!glfwWindowShouldClose(window))
	{
		camera.KeyMotion(0, 0, Niagara::CameraManipulator::Actions::NoAction);
		
		glfwPollEvents();

		// Update
		camera.UpdateAnim();

		g_ViewUniformBufferParameters.viewProjMatrix = camera.GetViewProjMatrix();
		g_ViewUniformBufferParameters.viewMatrix = camera.GetViewMatrix();
		g_ViewUniformBufferParameters.projMatrix = camera.GetProjMatrix();
		g_ViewUniformBufferParameters.camPos = camera.GetCamera().eye;
		Niagara::GetFrustumPlanes(g_ViewUniformBufferParameters.frustumPlanes, camera.GetProjMatrix(), /* reversedZ = */ true, /* needZPlanes = */ false);
		g_ViewUniformBufferParameters.frustumValues = glm::vec4(
			g_ViewUniformBufferParameters.frustumPlanes[0].x,
			g_ViewUniformBufferParameters.frustumPlanes[0].z,
			g_ViewUniformBufferParameters.frustumPlanes[1].y,
			g_ViewUniformBufferParameters.frustumPlanes[1].z);
		g_ViewUniformBufferParameters.zNearFar = glm::vec4(camera.m_ClipPlanes.x, MAX_DRAW_DISTANCE, 0, 0);
		// Max draw distance
		g_ViewUniformBufferParameters.frustumPlanes[5] = glm::vec4(0, 0, -1, -MAX_DRAW_DISTANCE);
		viewUniformBuffer.Update(device, &g_ViewUniformBufferParameters, sizeof(g_ViewUniformBufferParameters), 1);

		// Get resources
		SyncObjects &currentSyncObjects = frameResources[currentFrame].syncObjects;
		VkCommandBuffer currentCommandBuffer = frameResources[currentFrame].commandBuffer;

		// Semaphores
		// A semaphores is used to add order between queue operations.
		// There happens to be 2 kinds of semaphores in Vulkan, binary and timeline.
		// A semaphores is either unsignaled or signaled. It begins life as unsignaled. The way we use a semaphore to order queue operations is
		// by providing the same semaphore as a `signal` semaphore in one queue operation and as a `wait` semaphore in another queue operation.

		// Fences
		// A fence has a similar purpose, in that it is used to synchronize execution, but it is for ordering the execution on the CPU.
		// Simply put, if the host needs to know when the GPU has finished something, we use a fence.

		// Fetch back buffer
		vkWaitForFences(device, 1, &currentSyncObjects.inFlightFence, VK_TRUE, UINT64_MAX);
		
		uint32_t imageIndex = 0;
		VkResult result = swapchain.AcquireNextImage(device, currentSyncObjects.imageAvailableSemaphore, &imageIndex);
		if (result == VK_ERROR_OUT_OF_DATE_KHR || g_FramebufferResized)
		{
			windowResize(currentSyncObjects);
			continue;
		}

		// Only reset the fence if we are submitting work
		vkResetFences(device, 1, &currentSyncObjects.inFlightFence);

		/*
		{
			auto cmd = Niagara::BeginSingleTimeCommands();

			g_CommandContext.ImageBarrier(swapchain.images[imageIndex], VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

			g_CommandContext.PipelineBarriers(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);

			Niagara::EndSingleTimeCommands(cmd);
		}*/

		Render(currentCommandBuffer, framebuffers, swapchain, imageIndex, geometry, graphicsQueue, currentSyncObjects);

		{
			auto cmd = Niagara::BeginSingleTimeCommands();

			g_CommandContext.ImageBarrier(swapchain.images[imageIndex], VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

			g_CommandContext.PipelineBarriers(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);

			Niagara::EndSingleTimeCommands(cmd);
		}

		// Present
		result = swapchain.QueuePresent(graphicsQueue, imageIndex, currentSyncObjects.renderFinishedSemaphore);
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || g_FramebufferResized)
		{
			windowResize(currentSyncObjects);
			continue;
		}

		currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

		double frameEndTime = glfwGetTime() * 1000.0;
		double deltaTime = frameEndTime - currentFrameTime;
		currentFrameTime = frameEndTime;

		g_DeltaTime = deltaTime;

		char title[256];
		sprintf_s(title, "Cpu %.1f ms, Gpu %.1f ms", deltaTime, 0.f);
		glfwSetWindowTitle(window, title);
	}

	// Wait for the logical device to finish operations before exiting main loop and destroying the window
	vkDeviceWaitIdle(device);

	glfwDestroyWindow(window);
	glfwTerminate();

	// SPACE

	// Clean up Vulkan

	// Resources

	vkDestroyQueryPool(device, queryPool, nullptr);

	for (auto &frameResource : frameResources)
		frameResource.Cleanup(device);
	
	syncObjects.Cleanup(device);

	for (auto &framebuffer : framebuffers)
		vkDestroyFramebuffer(device, framebuffer, nullptr);
	
	g_BufferMgr.Cleanup(device);

	g_CommandMgr.Cleanup(device);

	g_PipelineMgr.Cleanup(device);

	g_ShaderMgr.Cleanup(device);

	g_CommonStates.Destroy(device);

	swapchain.Destroy(device);

	device.Destroy();
	
	if (g_bEnableValidationLayers)
		DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);

	vkDestroyInstance(instance, nullptr);

	return 0;
}
