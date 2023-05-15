#pragma once

#include "pch.h"
#include "Device.h"
#include "Swapchain.h"
#include "Image.h"
#include "Buffer.h"
#include "Shaders.h"
#include "Pipeline.h"
#include "CommandManager.h"


namespace Niagara
{
	class Renderer;

	struct CommonStates
	{
		// Rasterization states
		RasterizationState rasterizerDefault;
		RasterizationState rasterizerDefaultCcw;
		RasterizationState rasterizerTwoSided;
		RasterizationState rasterizerShadow;
		RasterizationState rasterizerShadowCcw;
		RasterizationState rasterizerShadowTwoSided;

		// Depth stencil states
		DepthStencilState depthStateDisabled;
		DepthStencilState depthStateReadWrite;
		DepthStencilState depthStateReadOnly;
		DepthStencilState depthStateReadOnlyReversed;
		DepthStencilState depthStateTestEqual;

		// Blend states
		ColorBlendAttachmentState attachmentBlendDisable;
		ColorBlendAttachmentState attachmentNoColorWrite;
		ColorBlendAttachmentState attachmentAlphaBlend;
		ColorBlendAttachmentState attachmentPreMultiplied;
		ColorBlendAttachmentState attachmentBlendAdditive;

		DynamicState ds;

		// Samplers
		Sampler linearClampSampler;
		Sampler linearRepeatSampler;
		Sampler pointClampSampler;
		Sampler pointRepeatSampler;
		Sampler minClampSampler;
		Sampler maxClampSampler;

		void Init(const Device& device);
		void Destroy(const Device& device);
	};
	extern CommonStates g_CommonStates;


	struct BufferManager
	{
		// Uniform buffers
		Buffer viewUniformBuffer;

		// View dependent textures
		Image colorBuffer;
		Image depthBuffer;

		void InitViewDependentBuffers(const Renderer& renderer);
		void Cleanup(const Device& device);
	};
	extern BufferManager g_BufferMgr;


	class Renderer
	{
		friend BufferManager;

	public:
		Renderer();
		~Renderer();

		static constexpr int MAX_FRAMES_IN_FLIGHT = 3;
		static const std::string s_ResourcePath;
		static const std::string s_ShaderPath;

		virtual bool Init(GLFWwindow *window);
		virtual void Destroy();

		virtual void Update(float deltaTime);
		virtual void Render();
		
		virtual void Resize();

		void WaitForFences();
		void Idle() { vkDeviceWaitIdle(m_Device); }

		const Device& GetDevice() const { return m_Device; }
		const VkExtent2D& ViewportExtent() const { return m_ViewportSize; }
		const VkExtent2D& RenderExtent() const { return m_RenderExtent; }
		VkCommandBuffer GetCommandBuffer(EQueueFamily queueFamily = EQueueFamily::Graphics);

	protected:
		std::vector<const char*> m_InstanceExtensions;
		std::vector<const char*> m_DeviceExtensions;
		VkPhysicalDeviceFeatures m_PhysicalDeviceFeatures{};
		void* m_ExtChain{ nullptr };
		void** m_ExtNextChain{ nullptr };

		struct DeviceFeatures 
		{
			VkPhysicalDeviceVulkan13Features features13{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
			VkPhysicalDeviceVulkan12Features features12{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
			VkPhysicalDeviceVulkan11Features features11{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES };
		} m_DeviceFeatures{};

		struct SyncObjects 
		{
			VkSemaphore presentCompleteSemaphore{ VK_NULL_HANDLE };
			VkSemaphore renderCompleteSemaphore{ VK_NULL_HANDLE };
			VkFence inFlightFence{ VK_NULL_HANDLE };

			static VkSemaphore GetSemaphore(const Device& device) 
			{
				VkSemaphoreCreateInfo createInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
				
				VkSemaphore semaphore{ VK_NULL_HANDLE };
				VK_CHECK(vkCreateSemaphore(device, &createInfo, nullptr, &semaphore));
				return semaphore;
			}

			static VkFence GetFence(const Device& device, VkFenceCreateFlags flags = VK_FENCE_CREATE_SIGNALED_BIT) 
			{
				VkFenceCreateInfo createInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
				// Create the fence in the signaled state, so the first call to `vkWaitForFences()` returns immediately since the fence is already signaled
				createInfo.flags = flags;
				VkFence fence{ VK_NULL_HANDLE };
				VK_CHECK(vkCreateFence(device, &createInfo, nullptr, &fence));
				return fence;
			}

			void Init(const Device& device)
			{
				Destroy(device);

				presentCompleteSemaphore = GetSemaphore(device);
				renderCompleteSemaphore = GetSemaphore(device);
				inFlightFence = GetFence(device);
			}

			void Destroy(const Device& device) 
			{
				if (presentCompleteSemaphore)
				{
					vkDestroySemaphore(device, presentCompleteSemaphore, nullptr);
					presentCompleteSemaphore = VK_NULL_HANDLE;
				}
				if (renderCompleteSemaphore)
				{
					vkDestroySemaphore(device, renderCompleteSemaphore, nullptr);
					renderCompleteSemaphore = VK_NULL_HANDLE;
				}
				if (inFlightFence)
				{
					vkDestroyFence(device, inFlightFence, nullptr);
					inFlightFence = VK_NULL_HANDLE;
				}
			}
		};

		struct FrameResource
		{
			SyncObjects syncObjects;

			void Init(const Device& device);
			void Destroy(const Device& device);
		};
		FrameResource m_FrameResources[MAX_FRAMES_IN_FLIGHT];
		
		virtual void InitFrameResources();
		virtual void DestroyFrameResources();

		virtual void OnInit();
		virtual void OnDestroy();
		virtual void OnUpdate();
		virtual void OnRender();
		virtual void OnResize();

		std::vector<VkCommandBuffer> m_ActiveCmds;

		uint64_t m_FrameIndex{ 0 };

		VkExtent2D m_ViewportSize{};
		VkExtent2D m_RenderExtent{};
		VkFormat m_ColorFormat{};
		VkFormat m_DepthFormat{};
		VkViewport m_MainViewport{};
		
		bool m_bResized{ false };
		bool m_bFlipViewport{ true };

	private:
		GLFWwindow* m_Window{ nullptr };
		VkInstance m_Instance{ VK_NULL_HANDLE };
		Device m_Device{};
		Swapchain m_Swapchain{};

#if defined(_DEBUG)
		VkDebugUtilsMessengerEXT m_DebugMessenger{ VK_NULL_HANDLE };
#endif

		// Triangle
		Shader m_TriVertShader{};
		Shader m_TriFragShader{};
		GraphicsPipeline m_TrianglePipeline{};
		const ManagedTexture* m_ToyTexture{ nullptr };
	};
}
