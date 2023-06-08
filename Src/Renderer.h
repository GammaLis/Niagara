#pragma once

#include "pch.h"
#include "Device.h"
#include "Swapchain.h"
#include "Image.h"
#include "Buffer.h"
#include "Shaders.h"
#include "Pipeline.h"
#include "CommandManager.h"

#include <unordered_map>


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

		// Load store actions
		LoadStoreInfo loadStoreDefault;
		LoadStoreInfo lClearSStore;
		LoadStoreInfo lDontCareSStore;

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
		Buffer viewUniformBuffer{"ViewUniformBuffer"};

		// View dependent textures
		Image colorBuffer{"ColorBuffer"};
		Image depthBuffer{"DepthBuffer"};

		void InitViewDependentBuffers(const Renderer& renderer);
		void Cleanup(const Device& device);
	};
	extern BufferManager g_BufferMgr;

	struct AccessDetail 
	{
		AccessDetail(VkPipelineStageFlags2 inStageMask = VK_PIPELINE_STAGE_2_NONE, VkAccessFlags2 inAccessMask = VK_ACCESS_2_NONE, VkImageLayout inLayout = VK_IMAGE_LAYOUT_UNDEFINED);

		void Reset() 
		{
			pipelineStage = VK_PIPELINE_STAGE_2_NONE;
			access = VK_ACCESS_NONE;
			layout = VK_IMAGE_LAYOUT_UNDEFINED;
		}

		bool NearlyEqual(const AccessDetail& other) const
		{
			return (layout == other.layout) && (pipelineStage & other.pipelineStage) != 0 && (access & other.access) != 0;
		}

		bool Equal(const AccessDetail& other) const 
		{
			return (layout == other.layout) && (pipelineStage == other.pipelineStage) && (access == other.access);
		}

		VkPipelineStageFlags2 pipelineStage;
		VkAccessFlags2 access;
		VkImageLayout layout;
	};

	struct AccessManager
	{
	public:
		void AddResourceAccess(const std::string& name);
		void RemoveResourceAccess(const std::string& name);

		void Invalidate();
		void UpdateAccess(const std::string& name, const AccessDetail& access, bool bImmediate = false);
		void Flush();
		bool GetAccessDetail(const std::string& name, AccessDetail &access);
		AccessDetail GetAccessDetail(const std::string& name);

	private:
		std::unordered_map<std::string, AccessDetail> m_ResourceAccesses;

		static constexpr uint32_t s_MaxPoolSize = 16;
		mutable std::pair<std::string, AccessDetail> m_Pool[s_MaxPoolSize];
		mutable uint32_t m_PoolCount{ 0 };
	};
	extern AccessManager g_AccessMgr;


	class RGBuilder;
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

		void RegisterExternalResources();

		std::vector<VkCommandBuffer> m_ActiveCmds;

		uint64_t m_FrameIndex{ 0 };

		VkExtent2D m_ViewportSize{};
		VkExtent2D m_RenderExtent{};
		VkFormat m_ColorFormat{};
		VkFormat m_DepthFormat{};
		VkViewport m_MainViewport{};
		VkRect2D m_RenderArea{};
		
		bool m_bResized{ false };
		bool m_bFlipViewport{ true };

		std::unique_ptr<RGBuilder> m_GraphBuilder;

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
