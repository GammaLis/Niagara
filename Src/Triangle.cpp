#include "pch.h"
#include "Camera.h"
#include "Utilities.h"
#include "Config.h"
#include "Geometry.h"
#include "VkQuery.h"

#include "Renderer.h"

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
double g_Time = 0.0;
bool g_UseTaskSubmit = false;
bool g_FramebufferResized = false;
bool g_DrawVisibilityInited = false;
bool g_MeshletVisibilityInited = false;

// Window callbacks
void FramebufferResizeCallback(GLFWwindow* window, int width, int height)
{
	g_FramebufferResized = true;

	auto pRenderer = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));
	if (pRenderer)
		pRenderer->Resize();
}

void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	using Niagara::CameraManipulator;

	const bool pressed = action != GLFW_RELEASE;

	if (!pressed) return;

	CameraManipulator::Inputs& inputs = Niagara::g_Inputs;
	inputs.ctrl = mods & GLFW_MOD_CONTROL;
	inputs.shift = mods & GLFW_MOD_SHIFT;
	inputs.alt = mods & GLFW_MOD_ALT;

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

void CursorPosCallback(GLFWwindow* window, double xpos, double ypos)
{
	using Niagara::CameraManipulator;

	const auto& inputs = Niagara::g_Inputs;

	if (inputs.lmb || inputs.mmb || inputs.rmb)
	{
		CameraManipulator::Singleton().MouseMove(static_cast<int>(xpos), static_cast<int>(ypos), inputs);
	}
}

// How many frames 
constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;
// We choose the number of 2 because we don't want the CPU to get too far ahead of the GPU. With 2 frames in flight, the CPU and GPU
// can be working on their own tasks at the same time. If the CPU finishes early, it will wait till the GPU finishes rendering before
// submitting more work.
// Each frame should have its own command buffer, set of semaphores, and fence.

/// Structures

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

/// Main

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
	GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Triangle", nullptr, nullptr);
	g_Window = window;
	
	Renderer triangleRenderer;

	// Inputs
	glfwSetWindowUserPointer(window, &triangleRenderer);
	glfwSetFramebufferSizeCallback(window, FramebufferResizeCallback);
	glfwSetKeyCallback(window, KeyCallback);
	glfwSetMouseButtonCallback(window, MouseButtonCallback);
	glfwSetWindowCloseCallback(window, WindowCloseCallback);
	glfwSetCursorPosCallback(window, CursorPosCallback);

	triangleRenderer.Init(window);

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();

		triangleRenderer.Update(0.0f);
		triangleRenderer.Render();

#if 0
		char title[256];
		sprintf_s(title, "Cpu %.2f ms, Gpu %.2f ms, Tris %.2fM", avgCpuFrame, avgGpuFrame, double(triangleCount) * 1e-6);
		glfwSetWindowTitle(window, title);
#endif
	}
	triangleRenderer.Idle();

	glfwDestroyWindow(window);
	glfwTerminate();

	triangleRenderer.Destroy();

	return 0;
}
