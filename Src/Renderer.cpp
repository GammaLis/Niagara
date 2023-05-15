#include "Renderer.h"
#include "CommandManager.h"
#include "VkQuery.h"
#include <iostream>


namespace Niagara
{
	CommonStates g_CommonStates{};
	BufferManager g_BufferMgr{};


	/// Debug

	static VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger)
	{
		auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
		if (func != nullptr)
			return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
		else
			return VK_ERROR_EXTENSION_NOT_PRESENT;
	}

	static void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator)
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

	static void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
	{
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT; // | VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
		createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		createInfo.pfnUserCallback = DebugCallback;
		createInfo.pUserData = nullptr; // Optional
	}

	static VkDebugUtilsMessengerEXT SetupDebugMessenger(VkInstance instance)
	{
		VkDebugUtilsMessengerCreateInfoEXT createInfo{};
		PopulateDebugMessengerCreateInfo(createInfo);

		VkDebugUtilsMessengerEXT debugMessenger;
		VK_CHECK(CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger));

		return debugMessenger;
	}


	/// CommonStates 
	void CommonStates::Init(const Device& device)
	{
		// Rasterization states
		{
			rasterizerDefaultCcw.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
			rasterizerTwoSided.cullMode = VK_CULL_MODE_NONE;

			rasterizerShadow = rasterizerDefault;
			rasterizerShadow.depthBiasSlopeFactor = -1.5f;
			rasterizerShadow.depthBiasEnable = VK_TRUE;
			rasterizerShadow.depthBiasClamp = -100;

			rasterizerShadowCcw = rasterizerShadow;
			rasterizerShadowCcw.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

			rasterizerShadowTwoSided = rasterizerShadow;
			rasterizerShadowTwoSided.cullMode = VK_CULL_MODE_NONE;
		}

		// DepthStencilStates
		{
			depthStateDisabled.depthTestEnable = VK_FALSE;
			depthStateDisabled.depthWriteEnable = VK_FALSE;
			depthStateDisabled.depthCompareOp = VK_COMPARE_OP_ALWAYS;
			depthStateDisabled.stencilTestEnable = VK_FALSE;
			depthStateDisabled.front.writeMask = 0xFF;
			depthStateDisabled.front.compareMask = 0xFF;
			depthStateDisabled.front.passOp = VK_STENCIL_OP_KEEP;
			depthStateDisabled.front.failOp = VK_STENCIL_OP_KEEP;
			depthStateDisabled.front.depthFailOp = VK_STENCIL_OP_KEEP;
			depthStateDisabled.front.compareOp = VK_COMPARE_OP_ALWAYS;
			depthStateDisabled.back = depthStateDisabled.front;

			depthStateReadWrite = depthStateDisabled;
			depthStateReadWrite.depthWriteEnable = VK_TRUE;
			depthStateReadWrite.depthWriteEnable = VK_TRUE;
			depthStateReadWrite.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;

			depthStateReadOnly = depthStateReadWrite;
			depthStateReadOnly.depthWriteEnable = VK_FALSE;

			depthStateReadOnlyReversed = depthStateReadOnly;
			depthStateReadOnlyReversed.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

			depthStateTestEqual = depthStateReadOnly;
			depthStateTestEqual.depthCompareOp = VK_COMPARE_OP_EQUAL;
		}

		// Blend states
		{
			attachmentBlendDisable.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
			attachmentBlendDisable.blendEnable = VK_FALSE;
			attachmentBlendDisable.colorBlendOp = VK_BLEND_OP_ADD;
			attachmentBlendDisable.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
			attachmentBlendDisable.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
			attachmentBlendDisable.alphaBlendOp = VK_BLEND_OP_ADD;
			attachmentBlendDisable.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			attachmentBlendDisable.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;

			attachmentNoColorWrite = attachmentBlendDisable;
			attachmentNoColorWrite.colorWriteMask = 0;

			attachmentAlphaBlend = attachmentBlendDisable;
			attachmentAlphaBlend.blendEnable = VK_TRUE;

			attachmentPreMultiplied = attachmentAlphaBlend;
			attachmentPreMultiplied.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;

			attachmentBlendAdditive = attachmentPreMultiplied;
			attachmentBlendAdditive.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
		}

		// Samplers
		{
			linearClampSampler.Init(device, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER);
			linearRepeatSampler.Init(device, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT);
			pointClampSampler.Init(device, VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER);
			pointRepeatSampler.Init(device, VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST, VK_SAMPLER_ADDRESS_MODE_REPEAT);
			minClampSampler.Init(device, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, 0.0f, VK_COMPARE_OP_NEVER, VK_SAMPLER_REDUCTION_MODE_MIN);
			maxClampSampler.Init(device, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, 0.0f, VK_COMPARE_OP_NEVER, VK_SAMPLER_REDUCTION_MODE_MAX);
		}
	}

	void CommonStates::Destroy(const Device& device)
	{
		linearClampSampler.Destroy(device);
		linearRepeatSampler.Destroy(device);
		pointClampSampler.Destroy(device);
		pointRepeatSampler.Destroy(device);
		minClampSampler.Destroy(device);
		maxClampSampler.Destroy(device);
	}

	/// BufferManager

	void BufferManager::InitViewDependentBuffers(const Renderer& renderer)
	{
		auto& renderExtent = renderer.RenderExtent();

		// Color texture
		VkClearValue clear = { {0.2f, 0.4f, 0.8f, 1.0f} };
		colorBuffer.Init(renderer.GetDevice(),
			VkExtent3D{ renderExtent.width, renderExtent.height, 1 },
			renderer.m_ColorFormat,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
			0, 1, 1, 1, clear);

		// Depth texture
		depthBuffer.Init(
			renderer.GetDevice(),
			VkExtent3D{ renderExtent.width, renderExtent.height, 1 },
			renderer.m_DepthFormat,
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
			0);
	}

	void BufferManager::Cleanup(const Device& device)
	{
		viewUniformBuffer.Destroy(device);

		colorBuffer.Destroy(device);
		depthBuffer.Destroy(device);
	}


	/// Renderer

	const std::string Renderer::s_ResourcePath = "../Resources/";
	const std::string Renderer::s_ShaderPath = "./CompiledShaders/";

	Renderer::Renderer()
	{
		m_InstanceExtensions.insert(m_InstanceExtensions.end(),
			{
				VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
			}
		);

		m_DeviceExtensions.insert(m_DeviceExtensions.end(),
			{ 
				VK_KHR_SWAPCHAIN_EXTENSION_NAME,
				VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
				VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME,
				VK_EXT_MESH_SHADER_EXTENSION_NAME 
			}
		);

		// Device features
		m_PhysicalDeviceFeatures.multiDrawIndirect = VK_TRUE;
		m_PhysicalDeviceFeatures.samplerAnisotropy = VK_TRUE;
		m_PhysicalDeviceFeatures.shaderInt16 = VK_TRUE;
		m_PhysicalDeviceFeatures.fillModeNonSolid = VK_TRUE;
		m_PhysicalDeviceFeatures.pipelineStatisticsQuery = VK_TRUE;
		m_PhysicalDeviceFeatures.independentBlend = VK_TRUE;

		// Device extensions
		{
			m_ExtNextChain = &m_ExtChain;

			auto& features13 = m_DeviceFeatures.features13;
			features13.maintenance4 = VK_TRUE;
			features13.synchronization2 = VK_TRUE;
			features13.dynamicRendering = VK_TRUE;
			features13.synchronization2 = VK_TRUE;
			m_ExtChain = &features13;

			auto& features12 = m_DeviceFeatures.features12;
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

			auto& features11 = m_DeviceFeatures.features11;
			features11.storageBuffer16BitAccess = VK_TRUE;
			features11.uniformAndStorageBuffer16BitAccess = VK_TRUE;
			features11.storagePushConstant16 = VK_TRUE;
			features11.shaderDrawParameters = VK_TRUE;
			features12.pNext = &features11;

			m_ExtNextChain = &features11.pNext;
		}
	}

	Renderer::~Renderer() 
	{
	}

	bool Renderer::Init(GLFWwindow *window)
	{
		VK_CHECK(volkInitialize());

		// VkInstance
		m_Instance = GetVulkanInstance(m_InstanceExtensions, true);
		volkLoadInstance(m_Instance);

		// Debug
#if defined(_DEBUG)
		m_DebugMessenger = SetupDebugMessenger(m_Instance);
		assert(m_DebugMessenger);
#endif

		// Device
		if (!m_Device.Init(m_Instance, m_PhysicalDeviceFeatures, m_DeviceExtensions, m_ExtChain))
			return false;
		volkLoadDevice(m_Device);

		// Swapchain
		m_Window = window;
		m_Swapchain.Init(m_Instance, m_Device, window);

		m_ViewportSize = m_Swapchain.extent;
		m_RenderExtent = m_ViewportSize;
		m_MainViewport = GetViewport({ {0, 0}, m_RenderExtent }, 0, 1, m_bFlipViewport);
		m_ColorFormat = m_Swapchain.colorFormat;
		m_DepthFormat = m_Device.GetSupportedDepthFormat(false);

		g_CommandMgr.Init(m_Device);
		// Common states
		g_CommonStates.Init(m_Device);
		g_CommonQueryPools.Init(m_Device);

		g_BufferMgr.InitViewDependentBuffers(*this);
		g_TextureMgr.Init(m_Device, s_ResourcePath + "Textures/");

		// Sync
		InitFrameResources();

		OnInit();

		return true;
	}

	void Renderer::Destroy()
	{
		OnDestroy();

		DestroyFrameResources();

		g_BufferMgr.Cleanup(m_Device);
		g_TextureMgr.Cleanup(m_Device);

		g_CommandMgr.Cleanup(m_Device);

		g_CommonStates.Destroy(m_Device);
		g_CommonQueryPools.Destroy(m_Device);

#if defined(_DEBUG)
		DestroyDebugUtilsMessengerEXT(m_Instance, m_DebugMessenger, nullptr);
#endif

		m_Swapchain.Destroy(m_Device);
		m_Device.Destroy();

		vkDestroyInstance(m_Instance, nullptr);
	}

	void Renderer::Update(float deltaTime) 
	{
		OnUpdate();
	}

	void Renderer::Render()
	{
		auto& frameResource = m_FrameResources[m_FrameIndex % MAX_FRAMES_IN_FLIGHT];
		SyncObjects& sync = frameResource.syncObjects;

		m_ActiveCmds.clear();
		g_CommandContext.Invalidate();

		// Fetch back buffer
		VkFence waitFences[] = { sync.inFlightFence };
		{
			vkWaitForFences(m_Device, ARRAYSIZE(waitFences), waitFences, VK_TRUE, UINT64_MAX);
		}
		
		uint32_t imageIndex{ 0 };
		VkResult result = m_Swapchain.AcquireNextImage(m_Device, sync.presentCompleteSemaphore, &imageIndex);
		if (result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			OnResize();
			return;
		}
		else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
		{
			throw std::runtime_error("Failed to acquire swap chain image!");
		}

		// Only reset fences if we are submitting work
		vkResetFences(m_Device, ARRAYSIZE(waitFences), waitFences);

		OnRender();

		auto presentCmd = GetCommandBuffer();
		{
			g_CommandContext.BeginCommandBuffer(presentCmd);

			auto &colorBuffer = g_BufferMgr.colorBuffer;
			auto backBuffer = m_Swapchain.images[imageIndex];

			g_CommandContext.ImageBarrier2(colorBuffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
				VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);
			g_CommandContext.ImageBarrier2(backBuffer, VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
				VK_ACCESS_2_NONE, VK_ACCESS_2_TRANSFER_WRITE_BIT);
			g_CommandContext.PipelineBarriers2(presentCmd);

			g_CommandContext.Blit(presentCmd, colorBuffer, backBuffer,
				VkRect2D{ {0,0}, {colorBuffer.extent.width, colorBuffer.extent.height} },
				VkRect2D{ {0,0}, {m_Swapchain.extent} });

			g_CommandContext.ImageBarrier2(colorBuffer, VK_IMAGE_LAYOUT_GENERAL,
				VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
				VK_ACCESS_2_TRANSFER_READ_BIT, VK_ACCESS_2_MEMORY_READ_BIT);
			g_CommandContext.ImageBarrier2(backBuffer, VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
				VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
				VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_ACCESS_2_MEMORY_READ_BIT);
			g_CommandContext.PipelineBarriers2(presentCmd);

			g_CommandContext.EndCommandBuffer(presentCmd);
		}
		
		// Submitting the command buffer
		{
			VkSubmitInfo submitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };

			submitInfo.commandBufferCount = static_cast<uint32_t>(m_ActiveCmds.size());
			submitInfo.pCommandBuffers = m_ActiveCmds.data();

			VkSemaphore waitSemaphores[] = { sync.presentCompleteSemaphore };
			VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_TRANSFER_BIT };
			submitInfo.pWaitSemaphores = waitSemaphores;
			submitInfo.waitSemaphoreCount = ARRAYSIZE(waitSemaphores);
			submitInfo.pWaitDstStageMask = waitStages;

			VkSemaphore signalSemaphores[] = { sync.renderCompleteSemaphore };
			submitInfo.pSignalSemaphores = signalSemaphores;
			submitInfo.signalSemaphoreCount = ARRAYSIZE(signalSemaphores);

			VK_CHECK(vkQueueSubmit(g_CommandMgr.GraphicsQueue(), 1, &submitInfo, sync.inFlightFence));
		}

		// Present
		{
			VkResult result = m_Swapchain.QueuePresent(g_CommandMgr.GraphicsQueue(), imageIndex, sync.renderCompleteSemaphore);
			if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_bResized)
			{
				OnResize();
				return;
			}
		}

		m_FrameIndex++;
	}

	void Renderer::Resize()
	{
		m_bResized = true;
	}

	void Renderer::OnResize()
	{
		vkDeviceWaitIdle(m_Device);

		// Recreate swapchain
		{
			m_Swapchain.UpdateSwapchain(m_Device, m_Window, false);
			m_ViewportSize = m_Swapchain.extent;
			m_RenderExtent = m_ViewportSize;
			m_MainViewport = GetViewport({ {0, 0}, m_RenderExtent }, 0, 1, m_bFlipViewport);
			m_ColorFormat = m_Swapchain.colorFormat;
		}

		// Recreate buffers 
		{
			g_BufferMgr.InitViewDependentBuffers(*this);
		}

		// Recreate syncs
		{
			auto& sync = m_FrameResources[m_FrameIndex % MAX_FRAMES_IN_FLIGHT].syncObjects;

			// vkDestroySemaphore(m_Device, sync.presentCompleteSemaphore, nullptr);
			// sync.presentCompleteSemaphore = SyncObjects::GetSemaphore(m_Device);
			sync.Init(m_Device);
		}
		
		m_bResized = false;
	}
	
	void Renderer::WaitForFences()
	{
		auto& sync = m_FrameResources[m_FrameIndex % MAX_FRAMES_IN_FLIGHT].syncObjects;

		VkFence waitFences[] = { sync.inFlightFence };
		vkWaitForFences(m_Device, ARRAYSIZE(waitFences), waitFences, VK_TRUE, UINT64_MAX);
	}

	VkCommandBuffer Renderer::GetCommandBuffer(EQueueFamily queueFamily)
	{
		auto cmd = g_CommandMgr.GetCommandBuffer(m_Device, m_FrameIndex + 1, queueFamily);
		m_ActiveCmds.emplace_back(cmd);
		return cmd;
	}

	void Renderer::FrameResource::Init(const Device& device)
	{
		syncObjects.Init(device);
	}

	void Renderer::FrameResource::Destroy(const Device& device)
	{
		syncObjects.Destroy(device);
	}

	void Renderer::InitFrameResources()
	{
		for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		{
			m_FrameResources[i].Init(m_Device);
		}
	}

	void Renderer::DestroyFrameResources()
	{
		for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		{
			m_FrameResources[i].Destroy(m_Device);
		}
	}

	void Renderer::OnInit()
	{
		m_TriVertShader.Load(m_Device, s_ShaderPath + "SimpleTriangle.vert.spv");
		m_TriFragShader.Load(m_Device, s_ShaderPath + "SimpleTriangle.frag.spv");

		// Shaders
		{
			m_TrianglePipeline.vertShader = &m_TriVertShader;
			m_TrianglePipeline.fragShader = &m_TriFragShader;
		}

		// Resources
		{
			m_ToyTexture = g_TextureMgr.LoadFromFile(m_Device, "lena_top.png");
		}

		// States
		{
			auto& pipelineState = m_TrianglePipeline.pipelineState;

			pipelineState.rasterizationState = m_bFlipViewport ? g_CommonStates.rasterizerDefaultCcw : g_CommonStates.rasterizerDefault;
			pipelineState.depthStencilState = g_CommonStates.depthStateDisabled;
			pipelineState.colorBlendAttachments = { g_CommonStates.attachmentBlendDisable };
		}
		m_TrianglePipeline.SetAttachments(&m_ColorFormat, 1);
		m_TrianglePipeline.Init(m_Device);
	}

	void Renderer::OnDestroy()
	{
		m_TrianglePipeline.Destroy(m_Device);

		m_TriVertShader.Cleanup(m_Device);
		m_TriFragShader.Cleanup(m_Device);
	}

	void Renderer::OnUpdate() 
	{

	}

	void Renderer::OnRender()
	{
		auto cmd = GetCommandBuffer();
		g_CommandContext.BeginCommandBuffer(cmd);

		Image& colorBuffer = g_BufferMgr.colorBuffer;

		// Transition
		{
			g_CommandContext.ImageBarrier2(colorBuffer, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
				VK_ACCESS_2_NONE, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);

			g_CommandContext.PipelineBarriers2(cmd);
		}

		// Render
		VkRect2D renderArea = { {0, 0}, m_RenderExtent };
		{
			std::vector<std::pair<Image*, LoadStoreInfo>> colorAttachments = {
				{ &colorBuffer, LoadStoreInfo()}
			};
			ScopedRendering scopedRendering(cmd, renderArea, colorAttachments);

			vkCmdSetViewport(cmd, 0, 1, &m_MainViewport);
			vkCmdSetScissor(cmd, 0, 1, &renderArea);

			g_CommandContext.BindPipeline(cmd, m_TrianglePipeline);

			DescriptorInfo toyTexInfo(g_CommonStates.linearClampSampler, m_ToyTexture->views[0], m_ToyTexture->layout);
			g_CommandContext.SetDescriptor(0, toyTexInfo);

			g_CommandContext.PushDescriptorSet(cmd);

			vkCmdDraw(cmd, 3, 1, 0, 0);
		}

		g_CommandContext.EndCommandBuffer(cmd);
	}
}
