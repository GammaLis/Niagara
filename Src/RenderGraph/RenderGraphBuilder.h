#pragma once

#include "pch.h"
#include "VkCommon.h"
#include "Renderer.h"
#include <set>
#include <unordered_set>
#include <unordered_map>


namespace Niagara
{
	// Resources

	class Buffer;
	class Image;

	constexpr uint32_t g_InvalidHandle{ ~0u };

	enum RGQueueFlagBits
	{
		RG_QUEUE_GRAPHICS_BIT		= 1 << 0,
		RG_QUEUE_COMPUTE_BIT		= 1 << 1,
		RG_QUEUE_TRANSFER_BIT		= 1 << 2,
		RG_QUEUE_ASYNC_COMPUTE_BIT	= 1 << 3,
		RG_QUEUE_ASYNC_GRAPHICS_	= 1 << 4
	};
	using RGQueueFlags = uint32_t;

	enum class ESizeType
	{
		Absolute,
		Relative,
	};

	enum class ERGResourceType
	{
		Buffer,
		Texture,
		Count
	};

	struct AccessInfo
	{
		VkPipelineStageFlags2 pipelineStage{ VK_PIPELINE_STAGE_2_NONE };
		VkAccessFlags2 access{ VK_ACCESS_2_NONE };
	};

	struct RGBufferDesc
	{
		static RGBufferDesc Create(uint32_t count, uint32_t stride, VkBufferUsageFlags usage = 0);
		static RGBufferDesc Create(uint32_t size, VkBufferUsageFlags usage = 0);
		static RGBufferDesc Create(const Buffer& buffer);

		VkDeviceSize size{ 0 };
		VkBufferUsageFlags usage{ 0 };

		bool operator== (const RGBufferDesc& other) const
		{
			return size == other.size && usage == other.usage;
		}
		bool operator!= (const RGBufferDesc& other) const
		{
			return !(*this == other);
		}
	};

	struct RGTextureDesc
	{
		static RGTextureDesc Create2D(uint32_t w, uint32_t h, VkFormat format, uint32_t mips = 1, uint32_t layers = 1, VkImageUsageFlags usage = 0);
		static RGTextureDesc Create2D(float scale, VkFormat format, uint32_t mips = 1, uint32_t layers = 1, VkImageUsageFlags usage = 0);
		static RGTextureDesc Create(const Image& image);

		ESizeType sizeType{ ESizeType::Relative };
		float w = 1.0f;
		float h = 1.0f;
		float d = 1.0f;
		VkFormat format{ VK_FORMAT_UNDEFINED };
		uint32_t samples{ 1 };
		uint32_t mipLevels{ 1 };
		uint32_t arrayLayers{ 1 };
		VkImageUsageFlags usage{ 0 };
	};	

	class RGResource
	{
	public:
		RGResource(const std::string &name = "") : m_Name{ name } {  }

		void UpdatePassAccess(uint32_t pass, const AccessInfo& access, RGQueueFlags queueFlags)
		{
			m_AccessInfos.push_back(std::make_pair(pass, access));
		}	

		void ReadIn(uint32_t pass, RGQueueFlags queueFlags = 0)
		{
			m_ReadPasses.insert(pass);
			m_QueueFlags |= queueFlags;
		}

		void WrittenIn(uint32_t pass, RGQueueFlags queueFlags = 0)
		{
			m_WritePasses.insert(pass);
			m_QueueFlags |= queueFlags;
		}
		
		const auto& GetReadPasses() const { return m_ReadPasses; }
		auto& GetReadPasses() { return m_ReadPasses; }
		const auto& GetWritePasses() const { return m_WritePasses; }
		auto& GetWritePasses() { return m_WritePasses; }

		const std::string& GetName() const { return m_Name; }
		void SetName(const std::string& name) { m_Name = name; }

		void Reset();
		bool IsCacheValid() const { return m_bCacheValid; }

		bool isExternal{ false };
		std::uint32_t physicalIndex{ g_InvalidHandle };

	private:
		std::string m_Name;
		RGQueueFlags m_QueueFlags{ 0 };
		bool m_bCacheValid{ false };
		std::set<uint32_t> m_WritePasses; // unordered_set
		std::set<uint32_t> m_ReadPasses;
		// FIXME: unused, to be deleted
		std::vector<std::pair<uint32_t, AccessInfo>> m_AccessInfos;
	};

	class RGBuffer : public RGResource
	{
	public:
		RGBuffer(const std::string &name = "") : RGResource(name) {  }

		auto* GetPhysicalResource() { return m_Buffer; }
		const auto* GetPhysicalResource() const { return m_Buffer; }

		void SetPhysicalResource(Buffer* buffer) { m_Buffer = buffer; }

		RGBufferDesc desc;
		
	private:
		Buffer* m_Buffer{ nullptr };
	};

	class RGTexture : public RGResource
	{
	public:
		RGTexture(const std::string &name = "") : RGResource(name) {  }

		auto* GetPhysicalResource() { return m_Image; }
		const auto& GetPhysicalResource() const { return m_Image; }

		void SetPhysicalResource(Image* image) { m_Image = image; }

		RGTextureDesc desc;

	private:
		Image* m_Image{ nullptr };
	};

	using RGResourceRef = RGResource*;
	using RGBufferRef = RGBuffer*;
	using RGTextureRef = RGTexture*;

	struct RGResourceHandle 
	{
		int index{ -1 };
		ERGResourceType type;
	};

	struct AccessedResource
	{
		AccessedResource(const AccessInfo& inAccess = {}) : access(inAccess) {  }
		AccessInfo access;
	};

	struct AccessedBuffer : AccessedResource
	{
		AccessedBuffer(RGBufferRef inBuffer = nullptr, const  AccessInfo& inAccess = {})
			: AccessedResource{ inAccess }, buffer{ inBuffer } {  }
		RGBufferRef buffer;
	};

	struct AccessedTexture : AccessedResource
	{
		AccessedTexture(RGTextureRef inTexture = nullptr, const AccessInfo& inAccess = {}, VkImageLayout inLayout = VK_IMAGE_LAYOUT_UNDEFINED)
			: AccessedResource{ inAccess }, texture{ inTexture }, layout{ inLayout } {  }
		RGTextureRef texture;
		VkImageLayout layout;
	};

	struct AccessedAttachment : AccessedTexture
	{
		AccessedAttachment(RGTextureRef inTexture = nullptr, const AccessInfo& inAccessInfo = {}, VkImageLayout inLayout = VK_IMAGE_LAYOUT_UNDEFINED, const LoadStoreInfo& inLoadStoreInfo = {})
			: AccessedTexture(inTexture, inAccessInfo, inLayout), loadStoreInfo{ inLoadStoreInfo } {  }
		LoadStoreInfo loadStoreInfo;
		bool bDepthStencil{ false };
	};
	

	class RGResourcePool 
	{
	public:
		void Init(const Device& device, const VkExtent2D& viewport);
		void Destroy();
		void Resize(const VkExtent2D& viewport) { m_ViewportSize = viewport; }
		
		Buffer* CreateBuffer(const RGBufferDesc &desc, const std::string &name);
		Image* CreateTexture(const RGTextureDesc &desc, const std::string &name);

	private:
		const Device* m_Device{ nullptr };
		VkExtent2D m_ViewportSize{ 1,1 };

		std::unordered_map<std::string, uint32_t> m_BufferMap;
		std::unordered_map<std::string, uint32_t> m_TextureMap;
		std::vector<Buffer> m_Buffers;
		std::vector<Image> m_Textures;
	};


	/// Passes

	// Ref: UE - ERDGPassFlags
	enum class EPassFlags
	{
		None = 0,
		// Pass uses rasterization on the graphics pipe
		Raster = 1 << 0,
		// Pass uses compute on the graphics pipe
		Compute = 1 << 1,
		// Pass uses compute on the async compute pipe
		AsyncCompute = 1 << 2,
		// Pass uses copy commands on the graphics pipe
		Copy = 1 << 3
	};
	using PassFlags = uint32_t;


	class RGBuilder;
	class RGPass 
	{
		friend class RGBuilder;
	public:
		RGPass(const std::string& name, PassFlags flags = 1);

		virtual void Execute(VkCommandBuffer cmd) { }
		virtual void PreExecute(VkCommandBuffer cmd);
		virtual void PostExecute(VkCommandBuffer cmd);

		void Reset();
		
		RGResourceHandle& Read(const RGResourceHandle& resource);
		RGResourceHandle& Write(const RGResourceHandle& resource);
		RGResourceHandle& Readwrite(const RGResourceHandle& resource);

		const auto& GetInBuffers() const { return m_InBuffers; }
		const auto& GetOutBuffers() const { return m_OutBuffers; }
		const auto& GetInTextures() const { return m_InTextures; }
		const auto& GetOutTextures() const { return m_OutTextures; }
		const auto& GetInputAttachments() const { return m_InAttachments; }
		const auto& GetColorAttachments() const { return m_ColorAttachments; };
		const auto& GetDepthAttachment () const { return m_DepthStencilAttachment; }

		auto& GetInBuffers() { return m_InBuffers; }
		auto& GetOutBuffers() { return m_OutBuffers; }
		auto& GetInTextures() { return m_InTextures; }
		auto& GetOutTextures() { return m_OutTextures; }
		auto& GetInputAttachments() { return m_InAttachments; }
		auto& GetColorAttachments() { return m_ColorAttachments; };
		auto& GetDepthAttachment() { return m_DepthStencilAttachment; }

		uint32_t GetIndex() const { return m_Index; }
		void RenderArea(const VkRect2D& renderArea) { m_RenderArea = renderArea; }

		/// Buffers

		RGPass& ReadBuffer(RGBufferRef buffer, const AccessInfo& access, VkBufferUsageFlags usage);
		RGPass& ReadVertexBuffer(RGBufferRef buffer)
		{
			return ReadBuffer(buffer, { VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT, VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT }, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
		}
		RGPass& ReadIndexBuffer(RGBufferRef buffer)
		{
			return ReadBuffer(buffer, { VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT, VK_ACCESS_2_INDEX_READ_BIT }, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
		}
		RGPass& ReadIndirectBuffer(RGBufferRef buffer)
		{
			return ReadBuffer(buffer, { VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT }, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
		}
		RGPass& ReadUniformBuffer(RGBufferRef buffer)
		{
			return ReadBuffer(buffer, { m_DefaultStages, VK_ACCESS_2_UNIFORM_READ_BIT }, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
		}
		RGPass& ReadTransferBuffer(RGBufferRef buffer)
		{
			ReadBuffer(buffer, { VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT }, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
		}

		RGPass& WriteBuffer(RGBufferRef buffer, const AccessInfo& access, VkBufferUsageFlags usage);
		RGPass& WriteStorageBuffer(RGBufferRef buffer, const AccessInfo& access)
		{
			// FIXME: VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT ??? 
			WriteBuffer(buffer, { m_DefaultStages, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT | VK_ACCESS_2_SHADER_STORAGE_READ_BIT }, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
		}
		RGPass& WriteTransferBuffer(RGBufferRef buffer)
		{
			WriteBuffer(buffer, { VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT }, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
		}
		
		/// Textures

		RGPass& ReadTexture(RGTextureRef texture, const AccessInfo& access, VkImageLayout layout, VkImageUsageFlags usage);
		RGPass& ReadSampledTexture(RGTextureRef texture)
		{
			return ReadTexture(texture, { m_DefaultStages, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT }, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT);
		}
		RGPass& ReadBlitTexture(RGTextureRef texture)
		{
			return ReadTexture(texture, { VK_PIPELINE_STAGE_2_BLIT_BIT | VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT}, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
		}

		RGPass& WriteTexture(RGTextureRef texture, const AccessInfo& access, VkImageLayout layout, VkImageUsageFlags usage);
		RGPass& WriteStorageTexture(RGTextureRef texture)
		{
			return WriteTexture(texture, { m_DefaultStages, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT | VK_ACCESS_2_SHADER_STORAGE_READ_BIT}, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_USAGE_STORAGE_BIT);
		}
		RGPass& WriteBlitTexture(RGTextureRef texture)
		{
			return WriteTexture(texture, { VK_PIPELINE_STAGE_2_BLIT_BIT | VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT }, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT);
		}

		/// Attachments

		RGPass& AddInputAttachment(RGTextureRef attachment);
		RGPass& AddColorAttachment(RGTextureRef attachment, const LoadStoreInfo& loadStoreInfo);
		RGPass& SetDepthAttachment(RGTextureRef attachment, const LoadStoreInfo& loadStoreInfo, VkAccessFlags2 access, VkImageLayout layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
		RGPass& ReadDepthAttachment(RGTextureRef attachment);
		RGPass& WriteDepthAttachment(RGTextureRef attachment);
		
		std::string name;
		bool enablePassCulling = true;
		bool enableAsyncCompute = false;
		bool bCacheValid = false;

	protected:
		RGBuilder* m_Builder{ nullptr };

		PassFlags m_PassFlags;
		RGQueueFlags m_QueueFlags{ 0 };
		VkPipelineStageFlags2 m_DefaultStages{ 0 };
		uint32_t m_Index{ g_InvalidHandle };

		std::vector<AccessedBuffer> m_InBuffers;
		std::vector<AccessedTexture> m_InTextures;
		std::vector<AccessedBuffer> m_OutBuffers;
		std::vector<AccessedTexture> m_OutTextures;

		// Attachments
		// FIXME: InputAttachments ???
		std::vector<AccessedAttachment> m_InAttachments;
		std::vector<AccessedAttachment> m_ColorAttachments;
		AccessedAttachment m_DepthStencilAttachment;

		// Raster area
		VkRect2D m_RenderArea{};
	};

	template <typename TPassData, typename TLambda>
	class TRGLambdaPass : public RGPass
	{
	public:
		TRGLambdaPass(const std::string &name, PassFlags flags, TPassData &&params, TLambda &&lambda)
			: RGPass(name, flags), m_PassData{std::move(params)}, m_ExecuteLambda{std::move(lambda)} 
		{  }

		virtual void Execute(VkCommandBuffer cmd) override;

		TPassData& GetParameters() { return m_PassData; }
		const TPassData& GetParameters() const { return m_PassData; }

	private:
		TPassData m_PassData;
		TLambda m_ExecuteLambda;
	};

	// FIXME: ???
	template <typename TPassData, typename TLambda>
	void TRGLambdaPass<TPassData, TLambda>::Execute(VkCommandBuffer cmd)
	{
		m_ExecuteLambda(cmd);
	}


	// Builder

	class RGBuilder
	{
	public:
		RGBuilder();

		void Init(Renderer* renderer);
		void Destroy();
		void Resize(const VkExtent2D& viewportSize);

		// ??? FIXME: Use Handle ???
#if 0
		RGResourceHandle CreateTexture(const RGTextureDesc &desc);
		RGResourceHandle CreateBuffer(const RGBufferDesc &desc);

		RGResourceHandle RegisterExternalTexture(const Image &image);
		RGResourceHandle RegisterExternalBuffer(const Buffer& buffer);

		RGBufferRef GetRGBuffer(const RGResourceHandle& handle);
		RGTextureRef GetRGTexture(const RGResourceHandle& handle);
#endif

		// TODO:
		// void ExportTexture();
		// void ExportBuffer();

		RGBufferRef CreateRGBuffer(const RGBufferDesc& desc, const std::string &name);
		RGTextureRef CreateRGTexture(const RGTextureDesc& desc, const std::string &name);

		RGBufferRef GetRGBuffer(const std::string& name);
		RGTextureRef GetRGTexture(const std::string& name);

		RGBufferRef RegisterExternalBuffer(Buffer& buffer);
		RGTextureRef RegisterExternalTexture(Image& image);

		void SetOutputTexture(RGTextureRef outTexture) { m_Output = outTexture; }

		template <typename TPassData, typename TLambda>
		TRGLambdaPass<TPassData, TLambda>& AddPass(const std::string& name, PassFlags flags, TPassData&& params, TLambda&& func)
		{
			auto iter = m_PassMap.find(name);
			if (iter != m_PassMap.end())
			{
				return dynamic_cast<TRGLambdaPass<TPassData, TLambda>&>(*m_Passes[iter->second]);
			}
			else
			{
				auto* newPass = new TRGLambdaPass<TPassData, TLambda>(name, flags, std::move(params), std::move(func));

				uint32_t passCount = static_cast<uint32_t>(m_Passes.size());
				newPass->m_Index = passCount;

				m_Passes.emplace_back(newPass);
				m_PassMap[name] = passCount;

				return *newPass;
			}
		}

		void Reset();
		void Compile();
		void Execute();

		bool IsCacheValid() const { return m_bCacheValid; }

	private:
		void BuildExecutionList();
		void BuildPass(uint32_t passIndex, uint32_t level = 0);
		void BuildDependencies(uint32_t passIndex, const std::set<uint32_t> &dependentPasses, uint32_t level = 0);
		void BuildResources();
		void BuildBarriers();

		void PipelineBarriers(VkCommandBuffer cmd, uint32_t passIndex);

		Renderer* m_Renderer{ nullptr };

		bool m_bValid{ true };
		bool m_bCacheValid{ false };
		
		std::vector<std::unique_ptr<RGPass>> m_Passes;
		std::unordered_map<std::string, uint32_t> m_PassMap; // name - pass index

		std::vector<std::unique_ptr<RGBuffer>> m_Buffers;
		std::vector<std::unique_ptr<RGTexture>> m_Textures;
		std::unordered_map<std::string, uint32_t> m_BufferMap;
		std::unordered_map<std::string, uint32_t> m_TextureMap;

		RGResourceRef m_Output{ nullptr };
		std::vector<uint32_t> m_ExecutionList;
		std::vector<std::unordered_set<uint32_t>> m_PassDependencies;

		// 
		std::uint32_t m_ExternalResourceCount{ 0 };
		std::uint32_t m_PhysicalResourceCount{ 0 };

		struct Barrier
		{
			VkPipelineStageFlags2 srcStageMask{ 0 };
			VkPipelineStageFlags2 dstStageMask{ 0 };
			VkAccessFlags2 srcAccessMask{ 0 };
			VkAccessFlags2 dstAccessMask{ 0 };
			VkImageLayout srcLayout{ VK_IMAGE_LAYOUT_UNDEFINED };
			VkImageLayout dstLayout{ VK_IMAGE_LAYOUT_UNDEFINED };
		};
		std::vector<std::vector<Barrier>> m_PassBarriers;

		// A resource pool -> a viewport
		std::unique_ptr<RGResourcePool> m_ResourcePool;
	};

	// extern RGBuilder g_GraphBuilder;
}
