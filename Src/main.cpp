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

#include <iostream>
#include <fstream>
#include <queue>

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_XYZW_ONLY
#include <glm/glm.hpp>
#include <glm/gtc/random.hpp>


/// Mesh
#include "meshoptimizer.h"

#define FAST_OBJ_IMPLEMENTATION
#include "fast_obj.h"


/// Custom settings
#define DRAW_SIMPLE_TRIANGLE 0
#define DRAW_SIMPLE_MESH 1

#define DRAW_MODE DRAW_SIMPLE_MESH

#define VERTEX_INPUT_MODE 1
#define FLIP_VIEWPORT 1
#define USE_MESHLETS 1
#define USE_MULTI_DRAW_INDIRECT 1
#define USE_DEVICE_8BIT_16BIT_EXTENSIONS 1
#define USE_DEVICE_MAINTENANCE4_EXTENSIONS 1
// Mesh shader needs the 8bit_16bit_extension


/// Global variables

constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;

constexpr uint32_t MESHLET_MAX_VERTICES = 64;
constexpr uint32_t MESHLET_MAX_PRIMITIVES = 84;

constexpr uint32_t MESH_MAX_LODS = 8;

constexpr uint32_t TASK_GROUP_SIZE = 32;
constexpr uint32_t DRAW_COUNT = 100;
constexpr float SCENE_RADIUS = 10.0f;
constexpr float MAX_DRAW_DISTANCE = 10.0f;

const std::string g_ResourcePath = "../Resources/";
VkExtent2D g_ViewportSize{};

GLFWwindow* g_Window = nullptr;
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
		meshTask.Load(device, "./CompiledShaders/SimpleMesh.task.spv");
		meshMesh.Load(device, "./CompiledShaders/SimpleMesh.mesh.spv");
		meshVert.Load(device, "./CompiledShaders/SimpleMesh.vert.spv");
		meshFrag.Load(device, "./CompiledShaders/SimpleMesh.frag.spv");

		cullComp.Load(device, "./CompiledShaders/DrawCommand.comp.spv");
		buildHiZComp.Load(device, "./CompiledShaders/HiZBuild.comp.spv");
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
	VK_KHR_16BIT_STORAGE_EXTENSION_NAME,
	VK_KHR_8BIT_STORAGE_EXTENSION_NAME,
	VK_KHR_SPIRV_1_4_EXTENSION_NAME,

	// VK 1.3
	VK_KHR_MAINTENANCE_4_EXTENSION_NAME,

#if USE_MESHLETS
	VK_NV_MESH_SHADER_EXTENSION_NAME,
	// VK_EXT_MESH_SHADER_EXTENSION_NAME
#endif

	VK_KHR_DRAW_INDIRECT_COUNT_EXTENSION_NAME,
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

struct Vertex
{
	glm::vec3 p;
#if USE_DEVICE_8BIT_16BIT_EXTENSIONS
	// glm::u16vec3 p;
	glm::u8vec4 n;
	glm::u16vec2 uv;
#else
	glm::vec3 n;
	glm::vec2 uv;
#endif

	static VkVertexInputBindingDescription GetBindingDescription()
	{
		// A vertex binding describes at which rate to load data from memory throughout the vertices.
		// It specifies the number of bytes between data entries and whether to move to the next data entry after 
		// each vertex or after each instance.
		VkVertexInputBindingDescription desc{};
		// All of the per-vertex data is packed together in one array, so we're only going to have 1 binding.
		desc.binding = 0;
		desc.stride = sizeof(Vertex);
		desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		return desc;
	}

	static std::vector<VkVertexInputAttributeDescription> GetAttributeDescriptions()
	{
		std::vector<VkVertexInputAttributeDescription> attributeDescs(3);

		// float	VK_FORAMT_R32_SFLOAT
		// vec2		VK_FORMAT_R32G32_SFLOAT
		// vec3		VK_FORMAT_R32G32B32_SFLOAT
		// vec4		VK_FORMAT_R32G32B32A32_SFLOAT
		// ivec2	VK_FORMAT_R32G32_SINT
		// uvec4	VK_FORMAT_R32G32B32A32_UINT
		// double	VK_FORMAT_R64_SFLOAT

		uint32_t vertexAttribOffset = 0;

		attributeDescs[0].binding = 0;
		attributeDescs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescs[0].location = 0;
		attributeDescs[0].offset = offsetof(Vertex, p); // vertexAttribOffset 
		vertexAttribOffset += 3 * 4;

#if USE_DEVICE_8BIT_16BIT_EXTENSIONS
		attributeDescs[1].binding = 0;
		attributeDescs[1].format = VK_FORMAT_R8G8B8A8_UINT;
		attributeDescs[1].location = 1;
		attributeDescs[1].offset = offsetof(Vertex, n); // vertexAttribOffset;
		vertexAttribOffset += 4;

		attributeDescs[2].binding = 0;
		attributeDescs[2].format = VK_FORMAT_R16G16_SFLOAT;
		attributeDescs[2].location = 2;
		attributeDescs[2].offset = offsetof(Vertex, uv); // vertexAttribOffset;
		vertexAttribOffset += 2 * 2;

#else
		attributeDescs[1].binding = 0;
		attributeDescs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescs[1].location = 1;
		attributeDescs[1].offset = offsetof(Vertex, n); // vertexAttribOffset;
		vertexAttribOffset += 3 * 4;

		attributeDescs[2].binding = 0;
		attributeDescs[2].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescs[2].location = 2;
		attributeDescs[2].offset = offsetof(Vertex, uv); // vertexAttribOffset;
		vertexAttribOffset += 2 * 4;
#endif

		return attributeDescs;
	}
};

/**
* Meshlets
* Each meshlet represents a variable number of vertices and primitives. There are no restrictions regarding the connectivity of
* these primitives. However, they must stay below a maximum amount, specified within the shader code.
* We recommend using up to 64 vertices and 126 primitives. The `6` in 126 is not a typo. The first generation hardware allocates
* primitive indices in 128 byte granularity and needs to reserve 4 bytes for the primitive count. Therefore 3 * 126 + 4 maximizes
* the fit into a 3 * 128 = 384 bytes block. Going beyond 126 triangles would allocate the next 128 bytes. 84 and 40 are other 
* maxima that work well for triangles.
*/
struct alignas(16) Meshlet
{
	glm::vec4 boundingSphere; // xyz - center, w - radius
	glm::vec4 coneApex; // w - unused
	glm::vec4 cone; // xyz - cone direction, w - cosAngle
	// uint32_t vertices[MESHLET_MAX_VERTICES];
	// uint8_t indices[MESHLET_MAX_PRIMITIVES*3]; // up to MESHLET_MAX_PRIMITIVES triangles
	uint32_t vertexOffset;
	uint8_t vertexCount = 0;
	uint8_t triangleCount = 0;
};

struct MeshLod
{
	uint32_t indexOffset;
	uint32_t indexCount;
	uint32_t meshletOffset;
	uint32_t meshletCount;
};

struct alignas(16) Mesh
{
	glm::vec4 boundingSphere;

	uint32_t vertexOffset;
	uint32_t vertexCount;
	
	uint32_t lodCount;
	MeshLod lods[MESH_MAX_LODS];
};

struct Geometry
{
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
	std::vector<uint32_t> meshletData;
	std::vector<Meshlet> meshlets;

	std::vector<Mesh> meshes;
};

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
	VkDrawMeshTasksIndirectCommandNV drawMeshTaskIndirectCommand; // 2 uint32_t
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

bool CheckValidationLayerSupport(const std::vector<const char*> &validationLayers) 
{
	uint32_t layerCount = 0;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	for (const char* layerName : validationLayers)
	{
		bool layerFound = false;

		for (const auto& layerProperties : availableLayers)
		{
			if (strcmp(layerName, layerProperties.layerName) == 0)
			{
				layerFound = true;
				break;
			}
		}

		if (!layerFound)
			return false;
	}

	return true;
}

std::vector<const char*> GetInstanceExtensions()
{
#if 0
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

#else
	std::vector<const char*> extensions = { VK_KHR_SURFACE_EXTENSION_NAME };

	// Now it's only win32
#if defined(_WIN32)
	extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#endif

#endif

	if (g_bEnableValidationLayers)
	{
		extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);	// SRS - Dependency when VK_EXT_DEBUG_MARKER is enabled
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}

	return extensions;
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

// The very first thing you need to do is initialize the Vulkan library by creating an instance. 
// The instance is the connection between your application and the Vulkan library and creating it 
// involves specifying some details about your application to the driver.
VkInstance GetVulkanInstance()
{
	// In real Vulkan applications you should probably check if 1.3 is available via
	// vkEnumerateInstanceVersion(...)
	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	// Using value initialization to leave it as `nullptr`
	// appInfo.pNext = nullptr;
	appInfo.pApplicationName = "Hello Vulkan";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "No Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_3;

	const std::vector<const char*> validationLayers =
	{
		"VK_LAYER_KHRONOS_validation"
	};

	assert(!g_bEnableValidationLayers || CheckValidationLayerSupport(validationLayers));

	// Get extensions supported by the instance
	std::vector<std::string> supportedExtensions;

	uint32_t extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
	if (extensionCount > 0)
	{
		std::vector<VkExtensionProperties> extensions(extensionCount);
		if (vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data()) == VK_SUCCESS)
		{
			for (const auto &ext : extensions)
				supportedExtensions.push_back(ext.extensionName);
		}
	}

	std::vector<const char*> extensions = GetInstanceExtensions();

	if (!g_InstanceExtensions.empty())
	{
		for (const char *enabledExt : g_InstanceExtensions)
		{
			// Output message if requested extension is not available
			if (std::find(supportedExtensions.begin(), supportedExtensions.end(), enabledExt) == supportedExtensions.end())
			{
				std::cerr << "Enabled instance extension: " << enabledExt << " is not present at instance level.\n";
				continue;
			}
			extensions.push_back(enabledExt);
		}
	}

	VkInstance instance = VK_NULL_HANDLE;
	{
		VkInstanceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;
		createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		createInfo.ppEnabledExtensionNames = extensions.data();

		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
		if (g_bEnableValidationLayers)
		{
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();

			PopulateDebugMessengerCreateInfo(debugCreateInfo);
			createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
		}
		else 
		{
			createInfo.enabledLayerCount = 0;
			createInfo.pNext = nullptr;
		}

		VK_CHECK(vkCreateInstance(&createInfo, nullptr, &instance));
	}

	return instance;
}


void GetImageViews(VkDevice device, std::vector<VkImageView> &imageViews, const std::vector<VkImage> &images, VkFormat imageFormat)
{
	imageViews.resize(images.size());

	VkImageViewCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	createInfo.format = imageFormat;
	createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	// The `subresourceRange` field describes what the image's purpose is and which part of the image should be accessed.
	createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	createInfo.subresourceRange.baseMipLevel = 0;
	createInfo.subresourceRange.levelCount = 1;
	createInfo.subresourceRange.baseArrayLayer = 0;
	createInfo.subresourceRange.layerCount = 1;
	for (size_t i = 0; i < images.size(); ++i)
	{
		createInfo.image = images[i];
		
		VK_CHECK(vkCreateImageView(device, &createInfo, nullptr, &imageViews[i]));
	}
}

#pragma region Pipeline

VkRenderPass GetRenderPass(VkDevice device, VkFormat format)
{
	// Attachment description
	VkAttachmentDescription colorAttachment{};
	colorAttachment.format = format;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	// The `loadOp` and `storeOp` determine what to do with the data in the attachment before rendering and after rendering.
	// * LOAD: Preserve the existing contents of the attachments
	// * CLEAR: Clear the values to a constant at the start
	// * DONT_CARE: Existing contents are undefined; we don't care about them
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	// Subpasses and attachment references
	// A single render pass can consist of multiple subpasses. Subpasses are subsequent rendering operations that depend on
	// the contents of framebuffers in previous passes, for example a sequence of post-processing effects that are applied one after another.
	// If you group these rendering operations into one render pass, then Vulkan is able to reorder the operations and conserve memory bandwidth
	// for possibly better performance.
	VkAttachmentReference colorAttachementRef{};
	colorAttachementRef.attachment = 0; // attachment index
	colorAttachementRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachementRef;

	VkRenderPassCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	createInfo.attachmentCount = 1;
	createInfo.pAttachments = &colorAttachment;
	createInfo.subpassCount = 1;
	createInfo.pSubpasses = &subpass;

	VkRenderPass renderPass = VK_NULL_HANDLE;
	VK_CHECK(vkCreateRenderPass(device, &createInfo, nullptr, &renderPass));

	return renderPass;
}

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

void RecordCommandBuffer(VkCommandBuffer cmd, const std::vector<VkFramebuffer> &framebuffers, const Niagara::Swapchain &swapchain, uint32_t imageIndex, const Geometry &geometry)
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
	vkCmdDrawMeshTasksIndirectCountNV(cmd, drawArgsBuffer.buffer, offsetof(MeshDrawCommand, drawMeshTaskIndirectCommand), drawCountBuffer.buffer, VkDeviceSize(0), drawArgsBuffer.elementCount, sizeof(MeshDrawCommand));

#else
	uint32_t nTask = static_cast<uint32_t>(mesh.meshlets.size());
	vkCmdDrawMeshTasksNV(cmd, Niagara::DivideAndRoundUp(nTask, 32), 0);
#endif

#else
	if (ib.size > 0)
	{
		vkCmdBindIndexBuffer(cmd, ib.buffer, 0, VK_INDEX_TYPE_UINT32);

#if USE_MULTI_DRAW_INDIRECT
		// vkCmdDrawIndexedIndirect(cmd, drawArgsBuffer.buffer, offsetof(MeshDrawCommand, drawIndexedIndirectCommand), drawArgsBuffer.elementCount, sizeof(MeshDrawCommand));
		vkCmdDrawIndexedIndirectCountKHR(cmd, drawArgsBuffer.buffer, offsetof(MeshDrawCommand, drawIndexedIndirectCommand), drawCountBuffer.buffer, VkDeviceSize(0), drawArgsBuffer.elementCount, sizeof(MeshDrawCommand));

#else
		vkCmdDrawIndexed(cmd, ib.elementCount, 1, 0, 0, 0);
#endif
	}
	else
		vkCmdDraw(cmd, vb.elementCount, 1, 0, 0);
#endif

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

void Render(VkCommandBuffer cmd, const std::vector<VkFramebuffer> &framebuffers, const Niagara::Swapchain& swapchain, uint32_t imageIndex, const Geometry& geometry, VkQueue graphicsQueue, SyncObjects& syncObjects)
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


bool LoadObj(std::vector<Vertex> &vertices, const char* path)
{
	fastObjMesh* obj = fast_obj_read(path);

	if (obj == nullptr)
		return false;

	size_t vertexCount = 0, indexCount = 0;
	for (uint32_t i = 0; i < obj->face_count; ++i)
	{
		vertexCount += obj->face_vertices[i];
		indexCount += (obj->face_vertices[i] - 2) * 3; // 3 -> 3, 4 -> 6
	}

	// Currently, duplicated vertices, vertexCount == indexCount
	vertexCount = indexCount;

	vertices.resize(vertexCount);

	size_t vertexOffset = 0, indexOffset = 0;
	for (uint32_t i = 0; i < obj->face_count; ++i)
	{
		for (uint32_t j = 0; j < obj->face_vertices[i]; ++j)
		{
			fastObjIndex objIndex = obj->indices[indexOffset + j];

			// Triangulate polygon on the fly; offset - 3 is always the first polygon vertex
			if (j >= 3)
			{
				vertices[vertexOffset + 0] = vertices[vertexOffset - 3];
				vertices[vertexOffset + 1] = vertices[vertexOffset - 1];
				vertexOffset += 2;
			}

			Vertex& v = vertices[vertexOffset++];

			// P
			v.p.x = obj->positions[objIndex.p * 3 + 0];
			v.p.y = obj->positions[objIndex.p * 3 + 1];
			v.p.z = obj->positions[objIndex.p * 3 + 2];
			
#if USE_DEVICE_8BIT_16BIT_EXTENSIONS
			// P
			// v.p.x = meshopt_quantizeHalf(obj->positions[objIndex.p * 3 + 0]);
			// v.p.y = meshopt_quantizeHalf(obj->positions[objIndex.p * 3 + 1]);
			// v.p.z = meshopt_quantizeHalf(obj->positions[objIndex.p * 3 + 2]);
			// N
			v.n.x = static_cast<uint8_t>(obj->normals[objIndex.n * 3 + 0] * 127.f + 127.5f);
			v.n.y = static_cast<uint8_t>(obj->normals[objIndex.n * 3 + 1] * 127.f + 127.5f);
			v.n.z = static_cast<uint8_t>(obj->normals[objIndex.n * 3 + 2] * 127.f + 127.5f);
			// UV
			v.uv.x = meshopt_quantizeHalf(obj->texcoords[objIndex.t * 2 + 0]);
			v.uv.y = meshopt_quantizeHalf(obj->texcoords[objIndex.t * 2 + 1]);
#else
			// N
			v.n.x = obj->normals[objIndex.n * 3 + 0];
			v.n.y = obj->normals[objIndex.n * 3 + 1];
			v.n.z = obj->normals[objIndex.n * 3 + 2];
			// UV
			v.uv.x = obj->texcoords[objIndex.t * 2 + 0];
			v.uv.y = obj->texcoords[objIndex.t * 2 + 1];
#endif
		}

		indexOffset += obj->face_vertices[i];
	}

	assert(vertexOffset == indexCount);

	fast_obj_destroy(obj);
	
	return true;
}

size_t BuildOptMeshlets(Geometry& result, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
{
	static const float ConeWeight = 0.25f;
	auto meshletCount = meshopt_buildMeshletsBound(indices.size(), MESHLET_MAX_VERTICES, MESHLET_MAX_PRIMITIVES);
	std::vector<meshopt_Meshlet> optMeshlets(meshletCount);
	std::vector<uint32_t> meshlet_vertices(meshletCount * MESHLET_MAX_VERTICES);
	std::vector<uint8_t> meshlet_triangles(meshletCount * MESHLET_MAX_PRIMITIVES * 3);
	meshletCount = meshopt_buildMeshlets(
		optMeshlets.data(),
		meshlet_vertices.data(),
		meshlet_triangles.data(),
		indices.data(),
		indices.size(),
		&vertices[0].p.x,
		vertices.size(),
		sizeof(Vertex),
		MESHLET_MAX_VERTICES,
		MESHLET_MAX_PRIMITIVES,
		ConeWeight);
	optMeshlets.resize(meshletCount);

	// Append meshlet data
	uint32_t vertexCount = 0, indexGroupCount = 0;
	for (const auto& optMeshlet : optMeshlets)
	{
		vertexCount += optMeshlet.vertex_count;
		indexGroupCount += Niagara::DivideAndRoundUp(optMeshlet.triangle_count * 3, 4);
	}
	uint32_t meshletDataOffset = static_cast<uint32_t>(result.meshletData.size());
	result.meshletData.insert(result.meshletData.end(), vertexCount + indexGroupCount, 0);

	uint32_t meshletOffset = static_cast<uint32_t>(result.meshlets.size());
	result.meshlets.insert(result.meshlets.end(), meshletCount, {});

	for (uint32_t i = 0; i < meshletCount; ++i)
	{
		const auto& optMeshlet = optMeshlets[i];
		auto& meshlet = result.meshlets[meshletOffset + i];

		// Meshlet
		meshopt_Bounds bounds = meshopt_computeMeshletBounds(
			&meshlet_vertices[optMeshlet.vertex_offset],
			&meshlet_triangles[optMeshlet.triangle_offset],
			optMeshlet.triangle_count,
			&vertices[0].p.x,
			vertices.size(),
			sizeof(Vertex));
		meshlet.vertexCount = static_cast<uint8_t>(optMeshlets[i].vertex_count);
		meshlet.triangleCount = static_cast<uint8_t>(optMeshlets[i].triangle_count);
		meshlet.vertexOffset = meshletDataOffset;
		meshlet.boundingSphere = glm::vec4(bounds.center[0], bounds.center[1], bounds.center[2], bounds.radius);
		meshlet.coneApex = glm::vec4(bounds.cone_apex[0], bounds.cone_apex[1], bounds.cone_apex[0], 0);
		meshlet.cone = glm::vec4(bounds.cone_axis[0], bounds.cone_axis[1], bounds.cone_axis[2], bounds.cone_cutoff);

		// Vertex indices
		for (uint32_t j = 0; j < optMeshlet.vertex_count; ++j)
			result.meshletData[meshletDataOffset + j] = meshlet_vertices[optMeshlet.vertex_offset + j];

		meshletDataOffset += optMeshlet.vertex_count;

		// Triangle indices (packed in 4 bytes)
		const uint32_t* indexGroups = reinterpret_cast<uint32_t*>(&meshlet_triangles[optMeshlet.triangle_offset]);
		uint32_t indexGroupCount = Niagara::DivideAndRoundUp(optMeshlet.triangle_count * 3, 4);
		for (uint32_t j = 0; j < indexGroupCount; ++j)
			result.meshletData[meshletDataOffset + j] = indexGroups[j];

		meshletDataOffset += indexGroupCount;
	}

	// Padding
	if (meshletCount % TASK_GROUP_SIZE != 0)
	{
		size_t paddingCount = TASK_GROUP_SIZE - meshletCount % TASK_GROUP_SIZE;
		result.meshlets.insert(result.meshlets.end(), paddingCount, {});
		meshletCount += paddingCount;
	}

	return meshletCount;
}

bool LoadMesh(Geometry &result, const char* path, bool bBuildMeshlets = true, bool bIndexless = false)
{
	std::vector<Vertex> triVertices;
	if (!LoadObj(triVertices, path))
		return false;

	size_t indexCount = triVertices.size();

	// FIXME: not used now
	if (bIndexless)
	{
		Mesh mesh{};
		mesh.vertexOffset = static_cast<uint32_t>(result.vertices.size());
		mesh.vertexCount = static_cast<uint32_t>(triVertices.size());

		result.vertices.insert(result.vertices.end(), triVertices.begin(), triVertices.end());
#if 0
		mesh.indices.resize(indexCount);
		for (size_t i = 0; i < indexCount; ++i)
			mesh.indices[i] = i;
#endif
	}
	else
	{	
		std::vector<uint32_t> remap(indexCount);
		size_t vertexCount = meshopt_generateVertexRemap(remap.data(), nullptr, indexCount, triVertices.data(), indexCount, sizeof(Vertex));

		std::vector<Vertex> vertices(vertexCount);
		std::vector<uint32_t> indices(indexCount);

		meshopt_remapVertexBuffer(vertices.data(), triVertices.data(), indexCount, sizeof(Vertex), remap.data());
		meshopt_remapIndexBuffer(indices.data(), nullptr, indexCount, remap.data());

		meshopt_optimizeVertexCache(indices.data(), indices.data(), indexCount, vertexCount);
		meshopt_optimizeVertexFetch(vertices.data(), indices.data(), indexCount, vertices.data(), vertexCount, sizeof(Vertex));

		// Mesh
		result.meshes.push_back({});
		auto& mesh = result.meshes.back();
		{
			// Vertices
			mesh.vertexOffset = static_cast<uint32_t>(result.vertices.size());
			mesh.vertexCount = static_cast<uint32_t>(vertexCount);
			result.vertices.insert(result.vertices.end(), vertices.begin(), vertices.end());

			// Bounding sphere
			glm::vec3 center{ 0.0f };
			for (const auto& vert : vertices)
				center += vert.p;
			center /= vertexCount;

			float radius = 0.0f;
			glm::vec3 tempVec{};
			for (const auto& vert : vertices)
			{
				tempVec = vert.p - center;
				radius = std::max(radius, glm::dot(tempVec, tempVec));
			}
			radius = sqrtf(radius);

			mesh.boundingSphere = glm::vec4(center, radius);
			
			// Lods
			size_t lodIndexCount = indexCount;
			std::vector<uint32_t> lodIndices = indices;
			while (mesh.lodCount < MESH_MAX_LODS)
			{
				auto& meshLod = mesh.lods[mesh.lodCount++];

				// Indices
				meshLod.indexOffset = static_cast<uint32_t>(result.indices.size());
				meshLod.indexCount = static_cast<uint32_t>(lodIndexCount);

				result.indices.insert(result.indices.end(), lodIndices.begin(), lodIndices.end());

				// Meshlets
				meshLod.meshletOffset = static_cast<uint32_t>(result.meshlets.size());
				meshLod.meshletCount = bBuildMeshlets ? static_cast<uint32_t>(BuildOptMeshlets(result, vertices, lodIndices)) : 0;

				// Simplify
				size_t nextTargetIndexCount = size_t(double(lodIndexCount * 0.5f)); // 0.75, 0.5
				size_t nextIndexCount = meshopt_simplify(lodIndices.data(), lodIndices.data(), lodIndexCount, &vertices[0].p.x, vertexCount, sizeof(Vertex), nextTargetIndexCount, 1e-2f);
				assert(nextIndexCount <= lodIndexCount);

				// We've reched the error bound
				if (nextIndexCount == lodIndexCount)
					break;

				lodIndexCount = nextIndexCount;
				lodIndices.resize(lodIndexCount);
				meshopt_optimizeVertexCache(lodIndices.data(), lodIndices.data(), lodIndexCount, vertexCount);
			}
		}
	}

	// TODO: optimize the mesh for more efficient GPU rendering
	return true;
}

#if 0

void BuildMeshlets(Mesh& mesh)
{
	std::vector<Meshlet> meshlets;
	Meshlet meshlet{};
	std::vector<uint8_t> meshletVertices(mesh.vertices.size(), 0xFF);
	uint8_t indexOffset = 0, vertexOffset = 0;
	for (size_t i = 0; i < mesh.indices.size(); i += 3)
	{
		uint32_t i0 = mesh.indices[i];
		uint32_t i1 = mesh.indices[i+1];
		uint32_t i2 = mesh.indices[i+2];

		uint8_t &v0 = meshletVertices[i0];
		uint8_t &v1 = meshletVertices[i1];
		uint8_t &v2 = meshletVertices[i2];

		if (meshlet.vertexCount + (v0 == 0xFF) + (v1 == 0xFF) + (v2 == 0xFF) > MESHLET_MAX_VERTICES ||
			meshlet.triangleCount >= MESHLET_MAX_PRIMITIVES)
		{
			mesh.meshlets.push_back(meshlet);
			for (uint8_t mv = 0; mv < meshlet.vertexCount; ++mv)
				meshletVertices[meshlet.vertices[mv]] = 0xFF;
			meshlet = {};
		}

		if (v0 == 0xFF)
		{
			v0 = meshlet.vertexCount;
			meshlet.vertices[meshlet.vertexCount++] = i0;
		}
		if (v1 == 0xFF)
		{
			v1 = meshlet.vertexCount;
			meshlet.vertices[meshlet.vertexCount++] = i1;
		}
		if (v2 == 0xFF)
		{
			v2 = meshlet.vertexCount;
			meshlet.vertices[meshlet.vertexCount++] = i2;
		}

		meshlet.indices[meshlet.triangleCount * 3 + 0] = v0;
		meshlet.indices[meshlet.triangleCount * 3 + 1] = v1;
		meshlet.indices[meshlet.triangleCount * 3 + 2] = v2;
		meshlet.triangleCount++;
	}

	if (meshlet.triangleCount > 0)
		mesh.meshlets.push_back(meshlet);
}

void BuildMeshletCones(Mesh &mesh)
{
	for (auto &meshlet : mesh.meshlets)
	{
		glm::vec3 normals[MESHLET_MAX_PRIMITIVES];
		for (uint32_t i = 0; i < meshlet.triangleCount; ++i)
		{
			uint32_t i0 = meshlet.indices[3 * i + 0];
			uint32_t i1 = meshlet.indices[3 * i + 1];
			uint32_t i2 = meshlet.indices[3 * i + 2];

			const Vertex &v0 = mesh.vertices[meshlet.vertices[i0]];
			const Vertex &v1 = mesh.vertices[meshlet.vertices[i1]];
			const Vertex &v2 = mesh.vertices[meshlet.vertices[i2]];
#if 0
			glm::vec3 p0{ Niagara::ToFloat(v0.p.x),  Niagara::ToFloat(v0.p.y) , Niagara::ToFloat(v0.p.z) };
			glm::vec3 p1{ Niagara::ToFloat(v1.p.x),  Niagara::ToFloat(v1.p.y) , Niagara::ToFloat(v1.p.z) };
			glm::vec3 p2{ Niagara::ToFloat(v2.p.x),  Niagara::ToFloat(v2.p.y) , Niagara::ToFloat(v2.p.z) };
#else
			glm::vec3 p0 = v0.p, p1 = v1.p, p2 = v2.p;
#endif
			glm::vec3 n = glm::cross(p1 - p0, p2 - p0);

			normals[i] = Niagara::SafeNormalize(n);
		}

		glm::vec3 avgNormal{};
		for (uint32_t i = 0; i < meshlet.triangleCount; ++i)
			avgNormal += normals[i];
		float n = sqrtf(avgNormal.x * avgNormal.x + avgNormal.y * avgNormal.y + avgNormal.z * avgNormal.z);
		if (n < Niagara::EPS)
			avgNormal = glm::vec3(1.0f, 0.0f, 0.0f);
		else
			avgNormal = avgNormal / (n);

		float minDot = 1.0f;
		for (uint32_t i = 0; i < meshlet.triangleCount; ++i)
		{
			float dot = normals[i].x * avgNormal.x + normals[i].y * avgNormal.y + normals[i].z * avgNormal.z;
			if (dot < minDot)
				minDot = dot;
		}

		meshlet.cone = glm::vec4(avgNormal, minDot);
	}
}

#endif


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

	VkInstance instance = GetVulkanInstance();
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
	physicalDeviceFeatures.multiDrawIndirect = true;
	physicalDeviceFeatures.samplerAnisotropy = true;

	// Device extensions
	void* pNextChain = nullptr;

	void** pNext = &pNextChain;

#if USE_MESHLETS
	VkPhysicalDeviceMeshShaderFeaturesNV featureMeshShader{};
	featureMeshShader.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_NV;
	featureMeshShader.meshShader = VK_TRUE;
	featureMeshShader.taskShader = VK_TRUE;

	*pNext = &featureMeshShader;
	pNext = &featureMeshShader.pNext;
#endif

#if USE_DEVICE_8BIT_16BIT_EXTENSIONS 
	VkPhysicalDevice8BitStorageFeatures features8Bit{};
	features8Bit.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES;
	features8Bit.storageBuffer8BitAccess = VK_TRUE;

	VkPhysicalDevice16BitStorageFeatures features16Bit{};
	features16Bit.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES;
	features16Bit.storageBuffer16BitAccess = VK_TRUE;

	features16Bit.pNext = &features8Bit;

	*pNext = &features16Bit;
	pNext = &features8Bit.pNext;
#endif

#if USE_DEVICE_MAINTENANCE4_EXTENSIONS
	VkPhysicalDeviceMaintenance4Features featureMaintenance4{};
	featureMaintenance4.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_4_FEATURES;
	featureMaintenance4.maintenance4 = VK_TRUE;

	*pNext = &featureMaintenance4;
	pNext = &featureMaintenance4.pNext;
#endif

#if USE_MULTI_DRAW_INDIRECT
	VkPhysicalDeviceShaderDrawParametersFeatures featureDrawParams{};
	featureDrawParams.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES;
	featureDrawParams.shaderDrawParameters = VK_TRUE;

	*pNext = &featureDrawParams;
	pNext = &featureDrawParams.pNext;
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
	camera.m_ClipPlanes.x = 1.0f;
	camera.SetCamera( { eye, center, up, glm::radians(90.0f) } );
	
	// Buffers
	VkMemoryPropertyFlags hostVisibleMemPropertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	VkMemoryPropertyFlags deviceLocalMemPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	
	// Uniforms
	g_ViewUniformBufferParameters.debugValue = glm::vec4(0.8, 0.2, 0.2, 1);

	auto& viewUniformBuffer = g_BufferMgr.viewUniformBuffer;
	viewUniformBuffer.Init(device, sizeof(ViewUniformBufferParameters), 1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, hostVisibleMemPropertyFlags);

	// Geometry
	Geometry geometry{};
	
	const std::vector<std::string> objFileNames{ "kitten.obj", "bunny.obj"}; // kitten bunny
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
		Niagara::GetFrustumPlanes(g_ViewUniformBufferParameters.frustumPlanes, camera.GetProjMatrix(), /* reversedZ = */ true);
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
