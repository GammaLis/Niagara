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
#include "VkQuery.h"

#include <iostream>
#include <fstream>
#include <queue>

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <glm/glm.hpp>
#include <glm/gtc/random.hpp>

using namespace Niagara;

#define DEBUG_SINGLE_DRAWCALL 0
#define TOY_FOR_FUN 1
const std::string g_ResourcePath = "../Resources/";
const std::string g_ShaderPath = "./CompiledShaders/";

GLFWwindow* g_Window = nullptr;
VkExtent2D g_ViewportSize{};
double g_DeltaTime = 0.0;
bool g_UseTaskSubmit = false;
bool g_FramebufferResized = false;
bool g_DrawVisibilityInited = false;
bool g_MeshletVisibilityInited = false;

enum DebugParam
{
	DrawFrustumCulling = 0,
	DrawOcclusionCulling,

	MeshletConeCulling,
	MeshletFrustumCulling,
	MeshletOcclusionCulling,

	TriBackfaceCulling,
	TriSmallCulling,

	MeshShading,
	ToyDraw,

	ParamCount
};

struct DebugParams
{
	uint32_t params[ParamCount] = {};

	std::string paramNames[ParamCount] = 
	{
		std::string("DrawFrustumCulling"),
		std::string("DrawOcclusionCulling"),
		std::string("MeshletConeCulling"),
		std::string("MeshletFrusutmCulling"),
		std::string("MeshletOcclusionCulling"),
		std::string("TriBackfaceCulling"),
		std::string("TriSmallCulling"),

		std::string("MeshShading"),
		std::string("ToyDraw"),
	};

	// Default params
	void Init()
	{
		params[DrawFrustumCulling] = 1;
		params[DrawOcclusionCulling] = 1;
		params[MeshletConeCulling] = 1;
		params[MeshletFrustumCulling] = 1;
		params[MeshletOcclusionCulling] = 0;
		params[TriBackfaceCulling] = 1;
		params[TriSmallCulling] = 1;

		params[MeshShading] = 1;
		params[ToyDraw] = 0;
	}

	void SwitchParam(DebugParam param)
	{
		params[param] ^= 1;
		printf("Param: %s - %d\n", paramNames[param].c_str(), params[param]);
	}

	void SwitchCulling()
	{
		uint32_t c = params[DrawFrustumCulling] ^ 1;

		params[DrawFrustumCulling] = c;
		params[DrawOcclusionCulling] = c;
		params[MeshletConeCulling] = c;
		params[MeshletFrustumCulling] = c;
		params[MeshletOcclusionCulling] = c;
		params[TriBackfaceCulling] = c;
		params[TriSmallCulling] = c;

		printf("Culling %s", c > 0 ? "ON" : "OFF");
	}
};
DebugParams g_DebugParams{};


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

	case GLFW_KEY_0:
		g_DebugParams.SwitchParam(DebugParam::DrawFrustumCulling);
		break;
	case GLFW_KEY_1:
		g_DebugParams.SwitchParam(DebugParam::DrawOcclusionCulling);
		break;
	case GLFW_KEY_2:
		g_DebugParams.SwitchParam(DebugParam::MeshletConeCulling);
		break;
	case GLFW_KEY_3:
		g_DebugParams.SwitchParam(DebugParam::MeshletFrustumCulling);
		break;
	case GLFW_KEY_4:
		g_DebugParams.SwitchParam(DebugParam::MeshletOcclusionCulling);
		break;
	case GLFW_KEY_5:
		g_DebugParams.SwitchParam(DebugParam::TriBackfaceCulling);
		break;
	case GLFW_KEY_6:
		g_DebugParams.SwitchParam(DebugParam::TriSmallCulling);
		break;
	case GLFW_KEY_9:
		g_DebugParams.SwitchCulling();
		break;
	case GLFW_KEY_M:
		g_DebugParams.SwitchParam(DebugParam::MeshShading);
		break;
	case GLFW_KEY_F:
		g_DebugParams.SwitchParam(DebugParam::ToyDraw);
		break;

	case GLFW_KEY_T:
		g_UseTaskSubmit = !g_UseTaskSubmit;
		printf("Task submit: %d\n", g_UseTaskSubmit);
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

	Shader toyMesh;
	Shader toyFullScreenFrag;

	void Init(const Niagara::Device& device)
	{
		meshTask.Load(device, g_ShaderPath + "SimpleMesh.task.spv");
		meshMesh.Load(device, g_ShaderPath + "SimpleMesh.mesh.spv");
		meshVert.Load(device, g_ShaderPath + "SimpleMesh.vert.spv");
		meshFrag.Load(device, g_ShaderPath + "SimpleMesh.frag.spv");

		cullComp.Load(device, g_ShaderPath + "DrawCommand.comp.spv");
		buildHiZComp.Load(device, g_ShaderPath + "HiZBuild.comp.spv");

#if 1
		toyMesh.Load(device, g_ShaderPath + "ToyMesh.mesh.spv");
		toyFullScreenFrag.Load(device, g_ShaderPath + "ToyFullScreen.frag.spv");
#endif
	}

	void Cleanup(const Niagara::Device& device)
	{
		meshVert.Cleanup(device);
		meshFrag.Cleanup(device);
		meshTask.Cleanup(device);
		meshMesh.Cleanup(device);

		cullComp.Cleanup(device);
		buildHiZComp.Cleanup(device);

		toyMesh.Cleanup(device);
		toyFullScreenFrag.Cleanup(device);
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
	Sampler minClampSampler;
	Sampler maxClampSampler;

	void Init(const Niagara::Device &device)
	{
		linearClampSampler.Init(device, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER);
		linearRepeatSampler.Init(device, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT);
		pointClampSampler.Init(device, VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER);
		pointRepeatSampler.Init(device, VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST, VK_SAMPLER_ADDRESS_MODE_REPEAT);
		minClampSampler.Init(device, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, 0.0f, VK_COMPARE_OP_NEVER, VK_SAMPLER_REDUCTION_MODE_MIN);
		maxClampSampler.Init(device, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, 0.0f, VK_COMPARE_OP_NEVER, VK_SAMPLER_REDUCTION_MODE_MAX);
	}

	void Destroy(const Niagara::Device &device)
	{
		linearClampSampler.Destroy(device);
		linearRepeatSampler.Destroy(device);
		pointClampSampler.Destroy(device);
		pointRepeatSampler.Destroy(device);
		minClampSampler.Destroy(device);
		maxClampSampler.Destroy(device);
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
	
	// VK_NV_MESH_SHADER_EXTENSION_NAME,
	VK_EXT_MESH_SHADER_EXTENSION_NAME
};

std::vector<VkDynamicState> g_DynamicStates =
{
	VK_DYNAMIC_STATE_VIEWPORT,
	VK_DYNAMIC_STATE_SCISSOR
};

enum DescriptorBindings : uint32_t
{
	VertexBuffer		= 0,
	MeshBuffer			= 1, // Actually not used in the task/mesh shader
	MeshDrawBuffer		= 2,
	MeshDrawArgsBuffer	= 3,
	MeshletBuffer		= 4,
	MeshletDataBuffer	= 5,

	MeshletVisibilityBuffer	= 1,
	DepthPyramid		= 6,

	// TODO: put these another place
	// Globals 
	ViewUniformBuffer	= 7,
	DebugUniformBuffer,
};


/// Structures

struct alignas(16) MeshDraw
{
	glm::vec4 worldMatRow0;
	glm::vec4 worldMatRow1;
	glm::vec4 worldMatRow2;
	
	int32_t  vertexOffset; // == meshes[meshIndex].vertexOffset, helps data locality in mesh shader
	uint32_t meshIndex;
	uint32_t meshletVisibilityOffset;
};

struct MeshDrawCommand
{
	uint32_t drawId;
	VkDrawIndexedIndirectCommand drawIndexedIndirectCommand; // 5 uint32_t

	// Used by mesh shading path
	uint32_t drawVisibility;
	uint32_t meshletVisibilityOffset;

	// Switch from MeshShaderNV to MeshShaderEXT
#if 0
	VkDrawMeshTasksIndirectCommandNV drawMeshTaskIndirectCommand; // 2 uint32_t
#else
	uint32_t taskOffset;
	uint32_t taskCount;
	VkDrawMeshTasksIndirectCommandEXT drawMeshTaskIndirectCommand; // 3 uint32_t
#endif
};

struct MeshTaskCommand
{
	uint32_t drawId;
	uint32_t taskOffset;
	uint32_t taskCount;
	uint32_t meshletVisibilityData; // low bit: draw visibility, higher 31 bit: meshVisibilityOffset
};

struct GpuBuffer
{
	VkBuffer buffer = VK_NULL_HANDLE;
	VkDeviceMemory memory = VK_NULL_HANDLE;
	void* data = nullptr;
	uint32_t offset = 0;
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

	operator VkBuffer() const { buffer; }
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
	glm::vec4 depthPyramidSize;
	glm::vec4 debugValue;
	glm::vec3 camPos;
	uint32_t drawCount;
};
ViewUniformBufferParameters g_ViewUniformBufferParameters;

struct BufferManager 
{
	// Uniform buffers
	GpuBuffer viewUniformBuffer;
	GpuBuffer debugUniformBuffer;

	// Storage buffers
	GpuBuffer vertexBuffer;
	GpuBuffer indexBuffer;
	GpuBuffer meshBuffer;
	GpuBuffer drawDataBuffer;
	GpuBuffer drawArgsBuffer;
	GpuBuffer drawCountBuffer;
	GpuBuffer drawVisibilityBuffer;

#if USE_MESHLETS
	GpuBuffer meshletBuffer;
	GpuBuffer meshletDataBuffer;
	GpuBuffer meshletVisibilityBuffer;
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

		// Depth texture
		depthBuffer.Init(
			device,
			VkExtent3D{ renderExtent.width, renderExtent.height, 1 },
			depthFormat,
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
			0);

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
		debugUniformBuffer.Destroy(device);

		// Storage buffers
		vertexBuffer.Destroy(device);
		indexBuffer.Destroy(device);
		meshBuffer.Destroy(device);
		drawDataBuffer.Destroy(device);
		drawArgsBuffer.Destroy(device);
		drawCountBuffer.Destroy(device);
		drawVisibilityBuffer.Destroy(device);

#if USE_MESHLETS
		meshletBuffer.Destroy(device);
		meshletDataBuffer.Destroy(device);
		meshletVisibilityBuffer.Destroy(device);
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

	// Tasks
	GraphicsPipeline meshTaskPipeline;
	ComputePipeline updateTaskArgsPipeline;

	GraphicsPipeline toyDrawPipeline;

	void Cleanup(const Niagara::Device &device)
	{
		meshDrawPipeline.Destroy(device);
		updateDrawArgsPipeline.Destroy(device);
		buildDepthPyramidPipeline.Destroy(device);

		meshTaskPipeline.Destroy(device);
		updateTaskArgsPipeline.Destroy(device);

		toyDrawPipeline.Destroy(device);

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

void GetMeshDrawPipeline(VkDevice device, Niagara::GraphicsPipeline& pipeline, Niagara::RenderPass &renderPass, uint32_t subpass, const VkRect2D& viewportRect, const std::vector<VkFormat> &colorAttachmentFormats, VkFormat depthFormat = VK_FORMAT_UNDEFINED, bool bUseTaskSubmit = false)
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
	if (depthFormat != VK_FORMAT_UNDEFINED)
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

#if 0
	// Render pass
	pipeline.renderPass = &renderPass;
	pipeline.subpass = subpass;
#else
	// Use dynamic rendering
	pipeline.SetAttachments(colorAttachmentFormats.data(), static_cast<uint32_t>(colorAttachmentFormats.size()), depthFormat);
#endif

	// Set specialization constants
	if (bUseTaskSubmit)
		pipeline.SetSpecializationConstant(0, 1);

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

	// Profiling
	auto& timestampQueryPool = g_CommonQueryPools.queryPools[0];
	timestampQueryPool.Reset(cmd);
	timestampQueryPool.WriteTimestamp(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0);

	auto& pipelineQueryPool = g_CommonQueryPools.queryPools[1];

	// Globals
	auto viewUniformBufferInfo = Niagara::DescriptorInfo(g_BufferMgr.viewUniformBuffer.buffer);
	auto debugUniformBufferInfo = DescriptorInfo(g_BufferMgr.debugUniformBuffer.buffer, VkDeviceSize(g_BufferMgr.debugUniformBuffer.offset), VkDeviceSize(g_BufferMgr.debugUniformBuffer.size));

	const auto& meshBuffer = g_BufferMgr.meshBuffer;
	const auto& drawDataBuffer = g_BufferMgr.drawDataBuffer;
	const auto& drawArgsBuffer = g_BufferMgr.drawArgsBuffer;
	const auto& drawCountBuffer = g_BufferMgr.drawCountBuffer;
	const auto& drawVisibilityBuffer = g_BufferMgr.drawVisibilityBuffer;

	Niagara::DescriptorInfo meshBufferDescInfo(meshBuffer.buffer, VkDeviceSize(meshBuffer.offset), VkDeviceSize(meshBuffer.size));
	Niagara::DescriptorInfo drawBufferDescInfo(drawDataBuffer.buffer, VkDeviceSize(drawDataBuffer.offset), VkDeviceSize(drawDataBuffer.size));
	Niagara::DescriptorInfo drawArgsDescInfo(drawArgsBuffer.buffer, VkDeviceSize(drawArgsBuffer.offset), VkDeviceSize(drawArgsBuffer.size));
	DescriptorInfo drawVisibilityInfo(drawVisibilityBuffer.buffer, VkDeviceSize(drawVisibilityBuffer.offset), VkDeviceSize(drawVisibilityBuffer.size));

	uint32_t groupsX = 1, groupsY = 1, groupsZ = 1;

	if (!g_DrawVisibilityInited)
	{
		vkCmdFillBuffer(cmd, drawVisibilityBuffer.buffer, VkDeviceSize(drawVisibilityBuffer.offset), VkDeviceSize(drawVisibilityBuffer.size), 0);

		g_CommandContext.BufferBarrier2(drawVisibilityBuffer.buffer, VkDeviceSize(drawVisibilityBuffer.offset), VkDeviceSize(drawVisibilityBuffer.size),
			VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
			VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT);
		g_CommandContext.PipelineBarriers2(cmd);

		g_DrawVisibilityInited = true;
	}
	
#if USE_MESHLETS
	if (!g_MeshletVisibilityInited)
	{
		const auto& meshletVisibilityBuffer = g_BufferMgr.meshletVisibilityBuffer;
		auto meshletVisibilityOffset = VkDeviceSize(meshletVisibilityBuffer.offset);
		auto meshletVisibilitySize = VkDeviceSize(meshletVisibilityBuffer.size);

		vkCmdFillBuffer(cmd, meshletVisibilityBuffer.buffer, meshletVisibilityOffset, meshletVisibilitySize, 0);

		g_CommandContext.BufferBarrier2(meshletVisibilityBuffer.buffer, meshletVisibilityOffset, meshletVisibilitySize,
			VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_TASK_SHADER_BIT_EXT,
			VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT);
		g_CommandContext.PipelineBarriers2(cmd);

		g_MeshletVisibilityInited = true;
	}
#endif

	// Init indirect draw count
	{
		vkCmdFillBuffer(cmd, drawCountBuffer.buffer, VkDeviceSize(0), VkDeviceSize(4), 0); // draw count / dispatch X groups
		vkCmdFillBuffer(cmd, drawCountBuffer.buffer, VkDeviceSize(4), VkDeviceSize(8), 1); // dispatch Y/Z groups

		g_CommandContext.BufferBarrier2(drawCountBuffer.buffer, VkDeviceSize(drawCountBuffer.offset), VkDeviceSize(drawCountBuffer.size),
			VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
			VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT);

		g_CommandContext.PipelineBarriers2(cmd);
	}

	// Update draw args
	auto cull = [&](uint32_t pass)
	{
		const uint32_t GroupSize = 64;

		VkDeviceSize drawCountOffset = drawCountBuffer.offset;
		VkDeviceSize drawCountSize = drawCountBuffer.stride;

		// Init draw call count
		{
			g_CommandContext.BufferBarrier2(drawCountBuffer.buffer, VkDeviceSize(drawCountBuffer.offset), VkDeviceSize(drawCountBuffer.size),
				VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
				VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);

			g_CommandContext.PipelineBarriers2(cmd);
			
			vkCmdFillBuffer(cmd, drawCountBuffer.buffer, drawCountOffset, drawCountSize, 0); // draw count or dispatch X groups

			g_CommandContext.BufferBarrier2(drawCountBuffer.buffer, VkDeviceSize(drawCountBuffer.offset), VkDeviceSize(drawCountBuffer.size),
				VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
				VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT);

			g_CommandContext.PipelineBarriers2(cmd);
		}

		auto rasterizationStage = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;

		// Sync
		g_CommandContext.BufferBarrier2(drawArgsBuffer.buffer, VkDeviceSize(drawArgsBuffer.offset), VkDeviceSize(drawArgsBuffer.size),
			VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT | rasterizationStage, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
			VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_2_SHADER_READ_BIT, VK_ACCESS_2_SHADER_WRITE_BIT);

		const auto& depthPyramid = g_BufferMgr.depthPyramid;
		VkImageSubresourceRange subresourceRange = { depthPyramid.subresource.aspectMask, 0, depthPyramid.subresource.mipLevel, 0, depthPyramid.subresource.arrayLayer };
		if (pass == 0)
		{
			g_CommandContext.ImageBarrier2(depthPyramid.image, subresourceRange,
				VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
				VK_PIPELINE_STAGE_2_NONE, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
				VK_ACCESS_2_NONE, VK_ACCESS_2_SHADER_READ_BIT);
		}
		else
		{
			g_CommandContext.ImageBarrier2(depthPyramid.image, subresourceRange,
				VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
				VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
				VK_ACCESS_2_SHADER_WRITE_BIT, VK_ACCESS_2_SHADER_READ_BIT);
		}

		g_CommandContext.PipelineBarriers2(cmd);

		// Update draw args
		if (g_UseTaskSubmit)
			g_CommandContext.BindPipeline(cmd, g_PipelineMgr.updateTaskArgsPipeline);
		else
			g_CommandContext.BindPipeline(cmd, g_PipelineMgr.updateDrawArgsPipeline);

		// Uniforms
		g_CommandContext.SetDescriptor(DescriptorBindings::ViewUniformBuffer, viewUniformBufferInfo);
		g_CommandContext.SetDescriptor(DescriptorBindings::DebugUniformBuffer, debugUniformBufferInfo);

		g_CommandContext.SetDescriptor(0, meshBufferDescInfo);
		g_CommandContext.SetDescriptor(1, drawBufferDescInfo);
		g_CommandContext.SetDescriptor(2, drawArgsDescInfo);

		Niagara::DescriptorInfo drawCountDescInfo(drawCountBuffer.buffer, drawCountOffset, drawCountSize);
		g_CommandContext.SetDescriptor(3, drawCountDescInfo);

		g_CommandContext.SetDescriptor(4, drawVisibilityInfo);

		DescriptorInfo depthPyramidInfo(g_CommonStates.minClampSampler, depthPyramid.views[0], VK_IMAGE_LAYOUT_GENERAL);
		g_CommandContext.SetDescriptor(5, depthPyramidInfo);

		g_CommandContext.PushDescriptorSetWithTemplate(cmd);

		struct States
		{
			uint32_t pass;
		} states = { pass };
		g_CommandContext.PushConstants(cmd, "_States", 0, sizeof(states), &states);

		groupsX = Niagara::DivideAndRoundUp(drawDataBuffer.elementCount, GroupSize);
		vkCmdDispatch(cmd, groupsX, groupsY, groupsZ);

		// Sync
		g_CommandContext.BufferBarrier2(drawArgsBuffer.buffer, VkDeviceSize(drawArgsBuffer.offset), VkDeviceSize(drawArgsBuffer.size),
			VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
			VK_ACCESS_2_SHADER_WRITE_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT);
		g_CommandContext.BufferBarrier2(drawCountBuffer.buffer, drawCountOffset, drawCountSize,
			VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
			VK_ACCESS_2_SHADER_WRITE_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT);

		g_CommandContext.PipelineBarriers2(cmd);
	};

	// Mesh draw pipeline
	auto &colorBuffer = g_BufferMgr.colorBuffer;
	auto &depthBuffer = g_BufferMgr.depthBuffer;

	bool hasStencilBuffer = Niagara::IsDepthStencilFormat(depthBuffer.format);
	VkImageAspectFlags depthAspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT | (hasStencilBuffer ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);
	VkImageLayout depthAttachmentLayout = hasStencilBuffer ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;

	VkClearColorValue clearColor = { 0.2f, 0.2f, 0.8f, 1.0f };
	VkClearDepthStencilValue clearDepth = { 0.0f, 1 };
	
	auto viewportRect = VkRect2D{ VkOffset2D{0, 0}, swapchain.extent };
	VkViewport dynamicViewport = Niagara::GetViewport(viewportRect, 0, 1, FLIP_VIEWPORT);

	auto draw = [&](uint32_t pass, VkClearColorValue clearColor, VkClearDepthStencilValue clearDepth, uint32_t query)
	{
		pipelineQueryPool.BeginQuery(cmd, query);

		VkDeviceSize drawCountOffset = drawCountBuffer.offset + 0;
		VkDeviceSize drawCountSize = drawCountBuffer.stride;

		if (pass == 0)
		{
			g_CommandContext.ImageBarrier2(colorBuffer.image, VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_PIPELINE_STAGE_2_NONE, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
				VK_ACCESS_2_NONE, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT); // VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
			g_CommandContext.ImageBarrier2(depthBuffer.image, depthAspectFlags,
				VK_IMAGE_LAYOUT_UNDEFINED, depthAttachmentLayout,
				VK_PIPELINE_STAGE_2_NONE, VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
				VK_ACCESS_2_NONE, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);

			g_CommandContext.PipelineBarriers2(cmd);
		}
		else
		{
			g_CommandContext.ImageBarrier2(depthBuffer.image, depthAspectFlags,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, depthAttachmentLayout,
				VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
				VK_ACCESS_2_SHADER_READ_BIT, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);

			g_CommandContext.PipelineBarriers2(cmd);
		}

		vkCmdSetViewport(cmd, 0, 1, &dynamicViewport);
		vkCmdSetScissor(cmd, 0, 1, &viewportRect);

		CommandContext::Attachment colorAttachment(&colorBuffer.views[0], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		CommandContext::Attachment depthAttachment(&depthBuffer.views[0], depthAttachmentLayout);
		LoadStoreInfo loadStoreInfo = pass == 0 ? LoadStoreInfo(VK_ATTACHMENT_LOAD_OP_CLEAR) : LoadStoreInfo(VK_ATTACHMENT_LOAD_OP_LOAD);
		g_CommandContext.SetAttachments(&colorAttachment, 1, &loadStoreInfo, &clearColor, &depthAttachment, &loadStoreInfo, &clearDepth);

		g_CommandContext.BeginRendering(cmd, viewportRect);

		if (g_UseTaskSubmit)
			g_CommandContext.BindPipeline(cmd, g_PipelineMgr.meshTaskPipeline);
		else
			g_CommandContext.BindPipeline(cmd, g_PipelineMgr.meshDrawPipeline);

		// Descriptors

		// Globals
		g_CommandContext.SetDescriptor(DescriptorBindings::ViewUniformBuffer, viewUniformBufferInfo, 0);
		g_CommandContext.SetDescriptor(DescriptorBindings::DebugUniformBuffer, debugUniformBufferInfo, 0);

		// Meshes
		const auto& vb = g_BufferMgr.vertexBuffer;
		const auto& ib = g_BufferMgr.indexBuffer;

#if VERTEX_INPUT_MODE == 1
		auto vbInfo = Niagara::DescriptorInfo(vb.buffer, VkDeviceSize(vb.offset), VkDeviceSize(vb.size));
		g_CommandContext.SetDescriptor(DescriptorBindings::VertexBuffer, vbInfo);

#if USE_MESHLETS
		const auto& meshletBuffer = g_BufferMgr.meshletBuffer;
		const auto& meshletDataBuffer = g_BufferMgr.meshletDataBuffer;
		const auto& meshletVisibilityBuffer = g_BufferMgr.meshletVisibilityBuffer;

		// Not used now
		// g_CommandContext.SetDescriptor(DescriptorBindings::MeshBuffer, meshBufferDescInfo);

		auto meshletInfo = Niagara::DescriptorInfo(meshletBuffer.buffer, VkDeviceSize(meshletBuffer.offset), VkDeviceSize(meshletBuffer.size));
		g_CommandContext.SetDescriptor(DescriptorBindings::MeshletBuffer, meshletInfo);

		auto meshletDataInfo = Niagara::DescriptorInfo(meshletDataBuffer.buffer, VkDeviceSize(meshletDataBuffer.offset), VkDeviceSize(meshletDataBuffer.size));
		g_CommandContext.SetDescriptor(DescriptorBindings::MeshletDataBuffer, meshletDataInfo);

		auto meshletVisibilityInfo = DescriptorInfo(meshletVisibilityBuffer.buffer, VkDeviceSize(meshletVisibilityBuffer.offset), VkDeviceSize(meshletVisibilityBuffer.size));
		g_CommandContext.SetDescriptor(DescriptorBindings::MeshletVisibilityBuffer, meshletVisibilityInfo);

		const auto& depthPyramid = g_BufferMgr.depthPyramid;
		DescriptorInfo depthPyramidInfo(g_CommonStates.minClampSampler, depthPyramid.views[0], VK_IMAGE_LAYOUT_GENERAL);
		g_CommandContext.SetDescriptor(DescriptorBindings::DepthPyramid, depthPyramidInfo);
#endif

		// Indirect draws
#if USE_MULTI_DRAW_INDIRECT
		g_CommandContext.SetDescriptor(DescriptorBindings::MeshDrawBuffer, drawBufferDescInfo);
		g_CommandContext.SetDescriptor(DescriptorBindings::MeshDrawArgsBuffer, drawArgsDescInfo);
#endif // USE_MULTI_DRAW_INDIRECT

#endif

		g_CommandContext.PushDescriptorSetWithTemplate(cmd, 0); // or g_CommandContext.PushDescriptorSet(cmd, 0);

		struct
		{
			uint32_t pass;
		} states = { pass };
		g_CommandContext.PushConstants(cmd, "_States", 0, sizeof(states), &states);

		VkDeviceSize offset = 0;
		vkCmdBindVertexBuffers(cmd, 0, 1, &vb.buffer, &offset);

#if USE_MESHLETS

#if USE_MULTI_DRAW_INDIRECT
		if (g_UseTaskSubmit)
		{
			vkCmdDrawMeshTasksIndirectEXT(cmd, drawCountBuffer.buffer, drawCountOffset, 1, 12);
		}
		else
		{
			// vkCmdDrawMeshTasksIndirectNV(cmd, drawArgsBuffer.buffer, offsetof(MeshDrawCommand, drawMeshTaskIndirectCommand), drawDataBuffer.elementCount, sizeof(MeshDrawCommand));
			// vkCmdDrawMeshTasksIndirectCountNV(cmd, drawArgsBuffer.buffer, offsetof(MeshDrawCommand, drawMeshTaskIndirectCommand), drawCountBuffer.buffer, drawCountOffset, drawDataBuffer.elementCount, sizeof(MeshDrawCommand));
			vkCmdDrawMeshTasksIndirectCountEXT(cmd, drawArgsBuffer.buffer, offsetof(MeshDrawCommand, drawMeshTaskIndirectCommand), drawCountBuffer.buffer, drawCountOffset, drawDataBuffer.elementCount, sizeof(MeshDrawCommand));
		}
#else
		uint32_t nTask = static_cast<uint32_t>(mesh.meshlets.size());
		vkCmdDrawMeshTasksNV(cmd, Niagara::DivideAndRoundUp(nTask, TASK_GROUP_SIZE), 0);
#endif

#else // USE_MESHLETS
		
		vkCmdBindIndexBuffer(cmd, ib.buffer, 0, VK_INDEX_TYPE_UINT32);

#if USE_MULTI_DRAW_INDIRECT
		// vkCmdDrawIndexedIndirect(cmd, drawArgsBuffer.buffer, offsetof(MeshDrawCommand, drawIndexedIndirectCommand), drawDataBuffer.elementCount, sizeof(MeshDrawCommand));
		vkCmdDrawIndexedIndirectCount(cmd, drawArgsBuffer.buffer, offsetof(MeshDrawCommand, drawIndexedIndirectCommand), drawCountBuffer.buffer, drawCountOffset, drawDataBuffer.elementCount, sizeof(MeshDrawCommand));
#else
		vkCmdDrawIndexed(cmd, ib.elementCount, 1, 0, 0, 0);
#endif

#endif // USE_MESHLETS

		g_CommandContext.EndRendering(cmd);

		pipelineQueryPool.EndQuery(cmd, query);
	};

	// Generate depth pyramid
	auto buildDepthPyramid = [&]()
	{
		const auto& depthPyramid = g_BufferMgr.depthPyramid;
		const uint32_t GroupSize = 8;

		g_CommandContext.ImageBarrier2(depthBuffer.image, depthAspectFlags,
			depthAttachmentLayout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
			VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_2_SHADER_READ_BIT);

		VkImageSubresourceRange subresourceRange = { depthPyramid.subresource.aspectMask, 0, depthPyramid.subresource.mipLevel, 0, depthPyramid.subresource.arrayLayer };

		g_CommandContext.ImageBarrier2(depthPyramid.image, subresourceRange,
			VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
			VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
			VK_ACCESS_2_SHADER_READ_BIT, VK_ACCESS_2_SHADER_WRITE_BIT);

		g_CommandContext.PipelineBarriers2(cmd);

		g_CommandContext.BindPipeline(cmd, g_PipelineMgr.buildDepthPyramidPipeline);

		VkImageView srcImageView = VK_NULL_HANDLE;
		uint32_t w = depthPyramid.extent.width, h = depthPyramid.extent.height;

		subresourceRange.levelCount = 1;

		struct Constants
		{
			glm::vec4 srcSize;
			glm::vec2 viewportMaxBoundUV;
		} constants{};

		uint32_t viewIndexOffset = 1;
		for (uint32_t i = 0; i < depthPyramid.subresource.mipLevel; ++i)
		{
			constants.viewportMaxBoundUV = glm::vec2(1.0f, 1.0f);

			VkImageLayout srcLayout = VK_IMAGE_LAYOUT_UNDEFINED;

			if (i == 0)
			{
				srcImageView = depthBuffer.views[0].view;
				srcLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				constants.srcSize = Niagara::GetSizeAndInvSize(depthBuffer.extent.width, depthBuffer.extent.height);
			}
			else
			{
				srcImageView = depthPyramid.views[i - 1 + viewIndexOffset].view;
				srcLayout = VK_IMAGE_LAYOUT_GENERAL;
				constants.srcSize = Niagara::GetSizeAndInvSize(w, h);
				subresourceRange.baseMipLevel = i - 1;

				g_CommandContext.ImageBarrier2(depthPyramid.image, subresourceRange,
					VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
					VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
					VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);

				g_CommandContext.PipelineBarriers2(cmd);

				w = std::max(1u, w >> 1);
				h = std::max(1u, h >> 1);
			}

			g_CommandContext.PushConstants(cmd, "_Constants", 0, sizeof(constants), &constants);

			g_CommandContext.SetDescriptor(0, Niagara::DescriptorInfo(g_CommonStates.linearClampSampler.sampler, srcImageView, srcLayout));
			g_CommandContext.SetDescriptor(1, Niagara::DescriptorInfo(VK_NULL_HANDLE, depthPyramid.views[i + viewIndexOffset].view, VK_IMAGE_LAYOUT_GENERAL));

			g_CommandContext.PushDescriptorSetWithTemplate(cmd, 0);

			groupsX = Niagara::DivideAndRoundUp(w, GroupSize);
			groupsY = Niagara::DivideAndRoundUp(h, GroupSize);
			vkCmdDispatch(cmd, groupsX, groupsY, groupsZ);
		}
	};

	pipelineQueryPool.Reset(cmd);

	// Early cull : frustum cull & fill objects that were visible last frame
	cull(/* pass =  */ 0);
	// Early draw : render objects that were visible last frame
	draw(/* pass =  */ 0, clearColor, clearDepth, /* query = */ 0);

	buildDepthPyramid();
	// Late cull : frustum cull & fill objects that were not visible last frame
	cull(/* pass =  */ 1);
	// Late draw : render objects that are visible this frame but weren't drawn in the early pass
	draw(/* pass =  */ 1, clearColor, clearDepth, /* query = */ 1);

	// TODO: Update the final depth pyramid
	// ...

	// Toy draw pass
	auto toyDraw = [&](VkCommandBuffer cmd, Image &colorBuffer, VkImageLayout oldLayout, VkPipelineStageFlags2 srcStageMask, VkAccessFlagBits2 srcAccessMask)
	{
		VkImageLayout newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		if (oldLayout != newLayout)
		{
			g_CommandContext.ImageBarrier2(colorBuffer.image, VK_IMAGE_ASPECT_COLOR_BIT,
				oldLayout, newLayout, srcStageMask, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
				srcAccessMask, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
			g_CommandContext.PipelineBarriers2(cmd);
		}

		CommandContext::Attachment attachment(&colorBuffer.views[0], newLayout);
		LoadStoreInfo loadStoreInfo(VK_ATTACHMENT_LOAD_OP_DONT_CARE);
		g_CommandContext.SetAttachments(&attachment, 1, &loadStoreInfo, nullptr);

		vkCmdSetViewport(cmd, 0, 1, &dynamicViewport);
		vkCmdSetScissor(cmd, 0, 1, &viewportRect);

		g_CommandContext.BeginRendering(cmd, viewportRect);

		g_CommandContext.BindPipeline(cmd, g_PipelineMgr.toyDrawPipeline);

		g_CommandContext.SetDescriptor(DescriptorBindings::ViewUniformBuffer, viewUniformBufferInfo);

		g_CommandContext.SetDescriptor(0, meshBufferDescInfo);
		g_CommandContext.SetDescriptor(1, drawBufferDescInfo);

		DescriptorInfo drawCountBufferInfo(drawCountBuffer.buffer, VkDeviceSize(drawCountBuffer.offset), VkDeviceSize(drawCountBuffer.size));
		g_CommandContext.SetDescriptor(3, drawCountBufferInfo);
		g_CommandContext.SetDescriptor(4, drawVisibilityInfo);

		const auto& depthPyramid = g_BufferMgr.depthPyramid;
		DescriptorInfo depthPyramidInfo(g_CommonStates.minClampSampler, depthPyramid.views[0], VK_IMAGE_LAYOUT_GENERAL); // pointClampSampler
		g_CommandContext.SetDescriptor(5, depthPyramidInfo);

		g_CommandContext.PushDescriptorSetWithTemplate(cmd); // or g_CommandContext.PushDescriptorSet(cmd, 0);

		vkCmdDrawMeshTasksEXT(cmd, 1, 1, 1);

		g_CommandContext.EndRendering(cmd);
	};

	if (g_DebugParams.params[DebugParam::ToyDraw])
		toyDraw(cmd, colorBuffer, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);

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

	timestampQueryPool.WriteTimestamp(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 1);

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
	physicalDeviceFeatures.pipelineStatisticsQuery = VK_TRUE;

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

	VkPhysicalDeviceMeshShaderFeaturesEXT featureMeshShader{};
	featureMeshShader.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;
	featureMeshShader.meshShader = VK_TRUE;
	featureMeshShader.taskShader = VK_TRUE;

	*pNext = &featureMeshShader;
	pNext = &featureMeshShader.pNext;

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

	// Queries
	g_CommonQueryPools.Init(device);

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
	std::vector<VkFormat> colorAttachmentFormats = { colorFormat };
	Niagara::GraphicsPipeline &meshDrawPipeline = g_PipelineMgr.meshDrawPipeline;
	GetMeshDrawPipeline(device, meshDrawPipeline, meshDrawPass, 0, { {0, 0}, renderExtent }, colorAttachmentFormats, depthFormat);

	GraphicsPipeline& meshTaskPipeline = g_PipelineMgr.meshTaskPipeline;
	GetMeshDrawPipeline(device, meshTaskPipeline, meshDrawPass, 0, { {0, 0}, renderExtent }, colorAttachmentFormats, depthFormat, true);

	Niagara::ComputePipeline &updateDrawArgsPipeline = g_PipelineMgr.updateDrawArgsPipeline;
	{
		updateDrawArgsPipeline.compShader = &g_ShaderMgr.cullComp;
		updateDrawArgsPipeline.Init(device);
	}

	Niagara::ComputePipeline& updateTaskArgsPipeline = g_PipelineMgr.updateTaskArgsPipeline;
	{
		updateTaskArgsPipeline.compShader = &g_ShaderMgr.cullComp;
		updateTaskArgsPipeline.SetSpecializationConstant(0, 1);
		updateTaskArgsPipeline.Init(device);
	}

	Niagara::ComputePipeline& buildDepthPyramidPipeline = g_PipelineMgr.buildDepthPyramidPipeline;
	{
		buildDepthPyramidPipeline.compShader = &g_ShaderMgr.buildHiZComp;
		buildDepthPyramidPipeline.Init(device);
	}

	GraphicsPipeline& toyDrawPipeline = g_PipelineMgr.toyDrawPipeline;
	{
		toyDrawPipeline.meshShader = &g_ShaderMgr.toyMesh;
		toyDrawPipeline.fragShader = &g_ShaderMgr.toyFullScreenFrag;
		toyDrawPipeline.SetAttachments(colorAttachmentFormats.data(), static_cast<uint32_t>(colorAttachmentFormats.size()));
		// toyDrawPipeline.SetSpecializationConstant(0, 1); // specialization constant
		toyDrawPipeline.Init(device);
	}

	// Command buffers

	// Creating the synchronization objects
	// We'll need one semaphore to signal that an image has been acquired from the swapchain and is ready for rendering, another one to signal that
	// rendering has finished and presentation can happen, and a fence to make sure only one frame is rendering at a time.
	SyncObjects syncObjects = GetSyncObjects(device);

	std::vector<FrameResources> frameResources;
	GetFrameResources(device, frameResources);

	// Scenes
	
	// Camera
	glm::vec3 eye{ 0, 0, 10 };
	glm::vec3 center{ };
	glm::vec3 up{ 0, 1, 0 };
	auto& camera = Niagara::CameraManipulator::Singleton();
	camera.SetWindowSize((int)swapchain.extent.width, (int)swapchain.extent.height);
	camera.SetSpeed(0.02f);
	// Currently the camera far plane is Inf
	camera.m_ClipPlanes.x = 0.5f; // 1.0f
	camera.SetCamera( { eye, center, up, glm::radians(90.0f) } );
	
	// Buffers
	VkMemoryPropertyFlags hostVisibleMemPropertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	VkMemoryPropertyFlags deviceLocalMemPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	
	// Uniforms
	const auto& depthPyramid = g_BufferMgr.depthPyramid;
	g_ViewUniformBufferParameters.viewportRect = glm::vec4(0.0f, 0.0f, renderExtent.width, renderExtent.height);
	g_ViewUniformBufferParameters.depthPyramidSize = glm::vec4(renderExtent.width / 2, renderExtent.height / 2, // depth pyramid viewport size
		depthPyramid.extent.width, depthPyramid.extent.height); // depth pyramid buffer size
	g_ViewUniformBufferParameters.debugValue = glm::vec4(1.0f); // glm::vec4(0.8, 0.2, 0.2, 1);

	g_DebugParams.Init();

	auto& viewUniformBuffer = g_BufferMgr.viewUniformBuffer;
	viewUniformBuffer.Init(device, sizeof(ViewUniformBufferParameters), 1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, hostVisibleMemPropertyFlags);

	auto& debugUniformBuffer = g_BufferMgr.debugUniformBuffer;
	debugUniformBuffer.Init(device, sizeof(DebugParams), 1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, hostVisibleMemPropertyFlags);

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
#if DEBUG_SINGLE_DRAWCALL
	const uint32_t DrawCount = 5; //  DRAW_COUNT;

	float testZ = 7.0f, farZ = -10.0f;
	std::vector<glm::vec3> positions
	{
		glm::vec3(0.0f, 0.0f, testZ + 0.5), glm::vec3(3.0f, 0.0f, testZ), glm::vec3(-1.8f, 2.0f, testZ),
		glm::vec3(0.0f, 0.0f, farZ), glm::vec3(1.0f, 0.0f, farZ)// occlusion culling test
	};
#else
	const uint32_t DrawCount = DRAW_COUNT;
#endif
	const float SceneRadius = SCENE_RADIUS;

	g_ViewUniformBufferParameters.drawCount = DrawCount;

	std::srand(42);

	std::vector<MeshDraw> meshDraws(DrawCount);
	uint32_t meshletVisibilityCount = 0;
	uint32_t taskGroupCount = 0;
	for (uint32_t i = 0; i < DrawCount; ++i)
	{
		auto& draw = meshDraws[i];

		// World matrix
		auto t = glm::ballRand<float>(SceneRadius);
		auto s = glm::linearRand(1.0f, 2.0f);
		s *= 2.0f;
		
		auto theta = glm::radians(glm::linearRand<float>(0.0f, 180.0));
		auto axis = glm::sphericalRand(1.0f);
		auto r = glm::quat(cosf(theta), axis * sinf(theta));

#if DEBUG_SINGLE_DRAWCALL
		t = positions[i];
		s = 1.0f;
		r = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
#endif

		glm::mat4 worldMat = glm::mat4_cast(r);
		worldMat[0] = worldMat[0] * s;
		worldMat[1] = worldMat[1] * s;
		worldMat[2] = worldMat[2] * s;
		worldMat[3] = glm::vec4(t.x, t.y, t.z, +1.0f);

		draw.worldMatRow0 = glm::vec4(worldMat[0][0], worldMat[1][0], worldMat[2][0], worldMat[3][0]);
		draw.worldMatRow1 = glm::vec4(worldMat[0][1], worldMat[1][1], worldMat[2][1], worldMat[3][1]);
		draw.worldMatRow2 = glm::vec4(worldMat[0][2], worldMat[1][2], worldMat[2][2], worldMat[3][2]);

		// Draw command data
		uint32_t meshId = glm::linearRand<uint32_t>(0, meshCount-1);
		const Mesh& mesh = geometry.meshes[meshId];

		draw.meshIndex = meshId;
		draw.vertexOffset = mesh.vertexOffset;
		draw.meshletVisibilityOffset = meshletVisibilityCount;

		meshletVisibilityCount += mesh.lods[0].meshletCount;
		taskGroupCount += DivideAndRoundUp(mesh.lods[0].meshletCount, TASK_GROUP_SIZE);
	}

	printf("Total meshlet visibility count: %d, total task group count: %d.\n", meshletVisibilityCount, taskGroupCount);

	// Indirect draw command buffer
	GpuBuffer& drawBuffer = g_BufferMgr.drawDataBuffer;
	drawBuffer.Init(device, sizeof(MeshDraw), DrawCount, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		deviceLocalMemPropertyFlags, meshDraws.data());

	GpuBuffer& drawArgsBuffer = g_BufferMgr.drawArgsBuffer;
#if 0
	drawArgsBuffer.Init(device, sizeof(MeshDrawCommand), DrawCount, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, deviceLocalMemPropertyFlags);
#else
	drawArgsBuffer.Init(device, 4, g_BufferSize/4, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, deviceLocalMemPropertyFlags);
#endif

	const uint32_t drawCountArgsCount = 3; // draw count or dispatch task X/Y/Z groups
	GpuBuffer& drawCountBuffer = g_BufferMgr.drawCountBuffer;
	drawCountBuffer.Init(device, sizeof(uint32_t), drawCountArgsCount, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, deviceLocalMemPropertyFlags);

	GpuBuffer& drawVisibilityBuffer = g_BufferMgr.drawVisibilityBuffer;
	drawVisibilityBuffer.Init(device, sizeof(uint32_t), DrawCount, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, deviceLocalMemPropertyFlags);
#endif

#if USE_MESHLETS
	GpuBuffer& meshletBuffer = g_BufferMgr.meshletBuffer;
	meshletBuffer.Init(device, sizeof(Meshlet), static_cast<uint32_t>(geometry.meshlets.size()), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, deviceLocalMemPropertyFlags, geometry.meshlets.data());

	GpuBuffer& meshletDataBuffer = g_BufferMgr.meshletDataBuffer;
	meshletDataBuffer.Init(device, sizeof(uint32_t), static_cast<uint32_t>(geometry.meshletData.size()), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, deviceLocalMemPropertyFlags, geometry.meshletData.data());

	GpuBuffer& meshletVisibilityBuffer = g_BufferMgr.meshletVisibilityBuffer;
	uint32_t mvbSize = DivideAndRoundUp(meshletVisibilityCount, 32); // 1 bit per meshlet visibility
	meshletVisibilityBuffer.Init(device, sizeof(uint32_t), mvbSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, deviceLocalMemPropertyFlags);
#endif

	// Frame time
	uint32_t currentFrame = 0;
	double currentFrameTime = glfwGetTime() * 1000.0;
	double avgCpuFrame = 0.0;

	const double timestampPeriod = device.properties.limits.timestampPeriod * 1e-6;
	double avgGpuFrame = 0.0;

	// Pipeline statistics
	uint32_t pipelineQueryResults[4] = {};
	uint32_t triangleCount = 0;

	// Resize
	auto windowResize = [&](SyncObjects &currentSyncObjects) 
	{
		vkDeviceWaitIdle(device);

		// Recreate swapchain
		swapchain.UpdateSwapchain(device, window, false);

		renderExtent = swapchain.extent;
		colorFormat = swapchain.colorFormat;

		// recreate view dependent textures
		g_BufferMgr.InitViewDependentBuffers(device, renderExtent, colorFormat, depthFormat);

		g_ViewUniformBufferParameters.viewportRect = glm::vec4(0.0f, 0.0f, renderExtent.width, renderExtent.height);
		g_ViewUniformBufferParameters.depthPyramidSize = glm::vec4(renderExtent.width / 2, renderExtent.height / 2, // depth pyramid viewport size
			depthPyramid.extent.width, depthPyramid.extent.height); // depth pyramid buffer size

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

		// Update camera
		camera.UpdateAnim();

		// Update uniforms

		g_ViewUniformBufferParameters.viewProjMatrix = camera.GetViewProjMatrix();
		g_ViewUniformBufferParameters.viewMatrix = camera.GetViewMatrix();
		g_ViewUniformBufferParameters.projMatrix = camera.GetProjMatrix();
		g_ViewUniformBufferParameters.camPos = camera.GetCamera().eye;
		Niagara::GetFrustumPlanes(g_ViewUniformBufferParameters.frustumPlanes, camera.GetProjMatrix(), /* reversedZ = */ true, /* needZPlanes = */ false);
		g_ViewUniformBufferParameters.frustumValues = glm::vec4(
			g_ViewUniformBufferParameters.frustumPlanes[0].x,
			g_ViewUniformBufferParameters.frustumPlanes[0].z,
			g_ViewUniformBufferParameters.frustumPlanes[2].y,
			g_ViewUniformBufferParameters.frustumPlanes[2].z);
		g_ViewUniformBufferParameters.zNearFar = glm::vec4(camera.m_ClipPlanes.x, MAX_DRAW_DISTANCE, 0, 0);
		// Max draw distance
		g_ViewUniformBufferParameters.frustumPlanes[5] = glm::vec4(0, 0, -1, -MAX_DRAW_DISTANCE);
		viewUniformBuffer.Update(device, &g_ViewUniformBufferParameters, sizeof(g_ViewUniformBufferParameters), 1);

		g_DebugParams.params[DebugParam::MeshShading] = USE_MESHLETS;
		debugUniformBuffer.Update(device, &g_DebugParams, sizeof(g_DebugParams), 1);

		// Get frame resources
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

		// Cpu times
		{
			double frameEndTime = glfwGetTime() * 1000.0;
			g_DeltaTime = frameEndTime - currentFrameTime;
			currentFrameTime = frameEndTime;

			avgCpuFrame = avgCpuFrame * 0.95 + g_DeltaTime * 0.05;
		}

		// Gpu times
		{
			uint64_t timestampResults[2] = {};
			VK_CHECK(g_CommonQueryPools.queryPools[0].GetResults(device, 0, ARRAYSIZE(timestampResults), sizeof(timestampResults), timestampResults, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT));

			double frameGpuBegin = double(timestampResults[0]) * timestampPeriod;
			double frameGpuEnd = double(timestampResults[1]) * timestampPeriod;
			double deltaGpuTime = frameGpuEnd - frameGpuBegin;

			avgGpuFrame = avgGpuFrame * 0.95 + deltaGpuTime * 0.05;
		}

		// Pipeline queries
		double trianglesPerSec = 0;
		{
			g_CommonQueryPools.queryPools[1].GetResults(device, 0, 2, sizeof(pipelineQueryResults), pipelineQueryResults, sizeof(uint32_t), VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);

			triangleCount = pipelineQueryResults[0] + pipelineQueryResults[1];
		}

		char title[256];
		sprintf_s(title, "Cpu %.2f ms, Gpu %.2f ms, Tris %.2fM", avgCpuFrame, avgGpuFrame, double(triangleCount) * 1e-6);
		glfwSetWindowTitle(window, title);
	}

	// Wait for the logical device to finish operations before exiting main loop and destroying the window
	vkDeviceWaitIdle(device);

	glfwDestroyWindow(window);
	glfwTerminate();

	// SPACE

	// Clean up Vulkan

	// Resources

	for (auto &frameResource : frameResources)
		frameResource.Cleanup(device);
	
	syncObjects.Cleanup(device);

	for (auto &framebuffer : framebuffers)
		vkDestroyFramebuffer(device, framebuffer, nullptr);
	
	g_BufferMgr.Cleanup(device);

	g_CommandMgr.Cleanup(device);

	g_PipelineMgr.Cleanup(device);

	g_ShaderMgr.Cleanup(device);

	g_CommonQueryPools.Destroy(device);

	g_CommonStates.Destroy(device);

	swapchain.Destroy(device);

	device.Destroy();
	
	if (g_bEnableValidationLayers)
		DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);

	vkDestroyInstance(instance, nullptr);

	return 0;
}
