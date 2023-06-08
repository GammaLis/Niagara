#include "RenderGraphBuilder.h"

namespace Niagara
{
	/// ResourceDesc

	RGBufferDesc RGBufferDesc::Create(uint32_t count, uint32_t stride, VkBufferUsageFlags usage)
	{
		RGBufferDesc desc{};
		desc.size = static_cast<VkDeviceSize>(stride * count);
		desc.usage = usage;
		return desc;
	}
	RGBufferDesc RGBufferDesc::Create(uint32_t size, VkBufferUsageFlags usage)
	{
		RGBufferDesc desc{};
		desc.size = static_cast<VkDeviceSize>(size);
		desc.usage = usage;
		return desc;
	}
	RGBufferDesc RGBufferDesc::Create(const Buffer& buffer)
	{
		RGBufferDesc desc{};
		desc.size = buffer.size;
		desc.usage = buffer.bufferUsage;
		return desc;
	}
	
	RGTextureDesc RGTextureDesc::Create2D(uint32_t w, uint32_t h, VkFormat format, uint32_t mips, uint32_t layers, VkImageUsageFlags usage)
	{
		RGTextureDesc desc{};
		desc.w = static_cast<float>(w);
		desc.h = static_cast<float>(h);
		desc.d = 1.0f;
		desc.format = format;
		desc.mipLevels = mips;
		desc.arrayLayers = layers;
		desc.sizeType = ESizeType::Absolute;
		desc.usage = usage;
		return desc;
	}

	RGTextureDesc RGTextureDesc::Create2D(float scale, VkFormat format, uint32_t mips, uint32_t layers, VkImageUsageFlags usage)
	{
		RGTextureDesc desc{};
		desc.w = scale;
		desc.h = scale;
		desc.d = 1.0f;
		desc.format = format;
		desc.mipLevels = mips;
		desc.arrayLayers = layers;
		desc.sizeType = ESizeType::Relative;
		desc.usage = usage;
		return desc;
	}

	RGTextureDesc RGTextureDesc::Create(const Image &image)
	{
		RGTextureDesc desc{};
		desc.w = static_cast<float>(image.extent.width);
		desc.h = static_cast<float>(image.extent.height);
		desc.d = static_cast<float>(image.extent.depth);
		desc.format = image.format;
		desc.mipLevels = image.subresource.mipLevel;
		desc.arrayLayers = image.subresource.arrayLayer;
		desc.sizeType = ESizeType::Absolute;
		desc.usage = image.usage;
		return desc;
	}

	/// RGResource

	void RGResource::Reset()
	{
		m_WritePasses.clear();
		m_ReadPasses.clear();
		m_bCacheValid = false;
	}

	/// RGResourcePool

	void RGResourcePool::Init(const Device& device, const VkExtent2D& viewport)
	{
		m_Device = &device;
		m_ViewportSize = viewport;
	}

	void RGResourcePool::Destroy()
	{
		for (auto& buffer : m_Buffers)
			buffer.Destroy(*m_Device);
		m_Buffers.clear();

		for (auto& texture : m_Textures)
			texture.Destroy(*m_Device);
		m_Textures.clear();
	}

	Buffer* RGResourcePool::CreateBuffer(const RGBufferDesc& desc, const std::string &name)
	{
		auto iter = m_BufferMap.find(name);
		if (iter == m_BufferMap.end())
		{
			uint32_t index = static_cast<uint32_t>(m_Buffers.size());
			m_BufferMap[name] = index;
			m_Buffers.emplace_back("");

			auto& buffer = m_Buffers.back();
			buffer.Init(*m_Device, desc.size, desc.usage);

			return &buffer;
		}
		else
		{
			auto& buffer = m_Buffers[iter->second];
			if (buffer.size != desc.size || buffer.bufferUsage != desc.usage)
				buffer.Init(*m_Device, desc.size, desc.usage);

			return &buffer;
		}
	}

	Image* RGResourcePool::CreateTexture(const RGTextureDesc& desc, const std::string &name)
	{
		uint32_t w = 1, h = 1, d = (uint32_t)desc.d;
		if (desc.sizeType == ESizeType::Absolute)
		{
			w = static_cast<uint32_t>(desc.w);
			h = static_cast<uint32_t>(desc.h);
		}
		else
		{
			w = static_cast<uint32_t>(desc.w * m_ViewportSize.width);
			h = static_cast<uint32_t>(desc.h * m_ViewportSize.height);
		}

		auto iter = m_TextureMap.find(name);
		if (iter == m_TextureMap.end())
		{
			uint32_t index = static_cast<uint32_t>(m_Textures.size());
			m_TextureMap[name] = index;
			m_Textures.emplace_back("");

			auto& texture = m_Textures.back();
			texture.Init(*m_Device, VkExtent3D{ w, h, d }, desc.format, desc.usage, 0, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				desc.mipLevels, desc.arrayLayers);

			return &texture;
		}
		else
		{
			auto& texture = m_Textures[iter->second];
			if (texture.extent.width != w || texture.extent.height != h || texture.extent.depth != d || texture.format != desc.format || 
				texture.usage != desc.usage || texture.subresource.mipLevel != desc.mipLevels || texture.subresource.arrayLayer != desc.arrayLayers)
				texture.Init(*m_Device, VkExtent3D{ w, h, d }, desc.format, desc.usage, 0, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
					desc.mipLevels, desc.arrayLayers);

			return &texture;
		}
	}

	/// RGPass

	RGPass::RGPass(const std::string& inName, PassFlags inFlags) : name{ inName }, m_PassFlags { inFlags }, m_Index{ g_InvalidHandle }
	{
		if (inFlags & (uint32_t)EPassFlags::Raster)
			m_DefaultStages = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT; // VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT
		else if (inFlags & ((uint32_t)EPassFlags::Compute | (uint32_t)EPassFlags::AsyncCompute))
			m_DefaultStages = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		else if (inFlags & (uint32_t)EPassFlags::Copy)
			m_DefaultStages = VK_PIPELINE_STAGE_2_TRANSFER_BIT | VK_PIPELINE_STAGE_2_BLIT_BIT | VK_PIPELINE_STAGE_2_COPY_BIT;
	}

	RGPass& RGPass::ReadBuffer(RGBufferRef buffer, const AccessInfo& access, VkBufferUsageFlags usage)
	{
		buffer->ReadIn(m_Index, m_QueueFlags);
		buffer->desc.usage = usage;
		m_InBuffers.emplace_back(buffer, access);
		return *this;
	}

	RGPass& RGPass::WriteBuffer(RGBufferRef buffer, const AccessInfo& access, VkBufferUsageFlags usage)
	{
		buffer->WrittenIn(m_Index, m_QueueFlags);
		buffer->desc.usage = usage;
		m_OutBuffers.emplace_back(buffer, access);
		return *this;
	}

	RGPass& RGPass::ReadTexture(RGTextureRef texture, const AccessInfo& access, VkImageLayout layout, VkImageUsageFlags usage)
	{
		texture->ReadIn(m_Index, m_QueueFlags);
		texture->desc.usage |= usage;

		auto iter = std::find_if(std::begin(m_InTextures), std::end(m_InTextures), [&](const AccessedTexture& tex) {
			return tex.texture == texture; });
		if (iter != m_InTextures.end())
			return *this;

		m_InTextures.emplace_back(texture, access, layout);

		return *this;
	}

	RGPass& RGPass::WriteTexture(RGTextureRef texture, const AccessInfo& access, VkImageLayout layout, VkImageUsageFlags usage)
	{
		texture->WrittenIn(m_Index, m_QueueFlags);
		texture->desc.usage |= usage;
		m_OutTextures.emplace_back(texture, access, layout);
		return *this;
	}

	RGPass& RGPass::AddInputAttachment(RGTextureRef attachment)
	{
		attachment->ReadIn(m_Index, m_QueueFlags);
		attachment->desc.usage |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

		VkPipelineStageFlags stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
		VkAccessFlags2 access = VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT;

		if (IsDepthStencilFormat(attachment->desc.format))
		{
			stage |= VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
			access |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
		}
		else
		{
			stage |= VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
			access |= VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT;
		}

		m_InAttachments.emplace_back(attachment, AccessInfo{stage, access}, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		return *this;
	}

	RGPass& RGPass::AddColorAttachment(RGTextureRef attachment, const LoadStoreInfo& loadStoreInfo)
	{
		attachment->WrittenIn(m_Index, m_QueueFlags);
		attachment->desc.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		m_ColorAttachments.emplace_back(attachment, 
			AccessInfo{VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT}, 
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, loadStoreInfo);
		return *this;
	}

	RGPass& RGPass::SetDepthAttachment(RGTextureRef attachment, const LoadStoreInfo& loadStoreInfo, VkAccessFlags2 access, VkImageLayout layout)
	{
		attachment->WrittenIn(m_Index, m_QueueFlags);
		attachment->desc.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

		m_DepthStencilAttachment.texture = attachment;
		m_DepthStencilAttachment.loadStoreInfo = loadStoreInfo;
		m_DepthStencilAttachment.layout = layout;
		m_DepthStencilAttachment.access.pipelineStage = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
		m_DepthStencilAttachment.access.access = access;

		return *this;
	}

	// FIXME: Provide LoadStoreInfo ???
	RGPass& RGPass::ReadDepthAttachment(RGTextureRef attachment)
	{
		return SetDepthAttachment(attachment, g_CommonStates.loadStoreDefault, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL);
	}

	RGPass& RGPass::WriteDepthAttachment(RGTextureRef attachment)
	{
		bool hasStencil = IsDepthStencilFormat(attachment->desc.format);
		return SetDepthAttachment(attachment, g_CommonStates.loadStoreDefault, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
			hasStencil ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
	}

	void RGPass::PreExecute(VkCommandBuffer cmd)
	{
		if (m_PassFlags & (uint32_t)EPassFlags::Raster)
		{			
			g_CommandContext.SetAttachments(m_ColorAttachments, m_DepthStencilAttachment);
			g_CommandContext.BeginRendering(cmd, m_RenderArea);
		}
	}

	void RGPass::PostExecute(VkCommandBuffer cmd)
	{ 
		if (m_PassFlags & (uint32_t)EPassFlags::Raster)
		{
			g_CommandContext.EndRendering(cmd);
		}
	}

	void RGPass::Reset()
	{
		m_InBuffers.clear();
		m_InTextures.clear();
		m_OutBuffers.clear();
		m_OutTextures.clear();

		m_InAttachments.clear();
		m_ColorAttachments.clear();

		bCacheValid = false;
	}


	/// RGBuilder

	RGBuilder::RGBuilder() : m_ResourcePool{ new RGResourcePool() }
	{

	}

	void RGBuilder::Init(Renderer* renderer)
	{
		m_Renderer = renderer;
		m_ResourcePool->Init(renderer->GetDevice(), renderer->ViewportExtent());
	}

	void RGBuilder::Destroy()
	{
		m_Buffers.clear();
		m_Textures.clear();
		m_BufferMap.clear();
		m_TextureMap.clear();

		m_ResourcePool->Destroy();
	}

	void RGBuilder::Resize(const VkExtent2D& viewportSize)
	{
		m_ResourcePool->Resize(m_Renderer->ViewportExtent());
	}

	RGBufferRef RGBuilder::CreateRGBuffer(const RGBufferDesc& desc, const std::string& name)
	{
		auto iter = m_BufferMap.find(name);
		if (iter == m_BufferMap.end() || iter->second >= m_Buffers.size())
		{
			uint32_t index = static_cast<uint32_t>(m_Buffers.size());
			m_BufferMap[name] = index;
			m_Buffers.emplace_back(new RGBuffer(name));

			auto *buffer = m_Buffers.back().get();
			buffer->desc = desc;
			return buffer;
		}
		else
		{
			printf("WARNING::Create a buffer with a name already exists!");

			auto *buffer = m_Buffers[iter->second].get();
			return buffer;
		}
	}

	RGTextureRef RGBuilder::CreateRGTexture(const RGTextureDesc& desc, const std::string& name)
	{
		auto iter = m_TextureMap.find(name);
		if (iter == m_TextureMap.end() || iter->second >= m_Textures.size())
		{
			uint32_t index = static_cast<uint32_t>(m_Textures.size());
			m_TextureMap[name] = index;
			m_Textures.emplace_back(new RGTexture(name));

			auto *texture = m_Textures.back().get();
			texture->desc = desc;
			return texture;
		}
		else
		{
			printf("WARNING::Create a texture with a name already exists!");

			auto *texture = m_Textures[iter->second].get();
			return texture;
		}
	}

	RGBufferRef RGBuilder::GetRGBuffer(const std::string& name)
	{
		auto iter = m_BufferMap.find(name);
		return iter != m_BufferMap.end() ? m_Buffers[iter->second].get() : nullptr;
	}

	RGTextureRef RGBuilder::GetRGTexture(const std::string& name)
	{
		auto iter = m_TextureMap.find(name);
		return iter != m_TextureMap.end() ? m_Textures[iter->second].get() : nullptr;
	}

	RGBufferRef RGBuilder::RegisterExternalBuffer(Buffer& buffer)
	{
		const std::string &name = buffer.name;

		auto iter = m_BufferMap.find(name);
		if (iter == m_BufferMap.end() || iter->second >= m_Buffers.size())
		{
			uint32_t index = static_cast<uint32_t>(m_Buffers.size());
			m_BufferMap[name] = index;
			m_Buffers.emplace_back(new RGBuffer(name));

			auto *buf = m_Buffers.back().get();
			buf->desc = RGBufferDesc::Create(buffer);
			buf->physicalIndex = m_ExternalResourceCount++;
			buf->SetPhysicalResource(&buffer);
			return buf;
		}
		else
		{
			printf("WARNING::Register a buffer with a name already exists!");

			auto *buf = m_Buffers[iter->second].get();
			return buf;
		}
	}

	RGTextureRef RGBuilder::RegisterExternalTexture(Image& image)
	{
		const std::string& name = image.name;

		auto iter = m_TextureMap.find(name);
		if (iter == m_TextureMap.end() || iter->second >= m_Textures.size())
		{
			uint32_t index = static_cast<uint32_t>(m_Textures.size());
			m_TextureMap[name] = index;
			m_Textures.emplace_back(new RGTexture(name));

			auto *texture = m_Textures.back().get();
			texture->desc = RGTextureDesc::Create(image);
			texture->physicalIndex = m_ExternalResourceCount++;
			texture->isExternal = true;
			texture->SetPhysicalResource(&image);
			return texture;
		}
		else
		{
			printf("WARNING::Register a texture with a name already exists!");

			auto *texture = m_Textures[iter->second].get();
			return texture;
		}
	}

	void RGBuilder::Reset()
	{
		m_Passes.clear();
		m_PassMap.clear();

		m_ExecutionList.clear();
		m_PassDependencies.clear();

		m_bCacheValid = false;
	}

	void RGBuilder::Compile()
	{
		m_bValid = true;

		if (m_Passes.empty())
		{
			m_bValid = false;
			printf("RG::Empty RenderPasses!");
			return;
		}

		if (m_Output == nullptr)
		{
			m_bValid = false;
			printf("Null Output!");
			return;
		}

		BuildExecutionList();
		BuildResources();
		BuildBarriers();

		m_bCacheValid = true;
	}

	void RGBuilder::Execute()
	{
		if (!m_bValid)
			return;

		VkCommandBuffer cmd = m_Renderer->GetCommandBuffer();
		g_CommandContext.BeginCommandBuffer(cmd);

		uint32_t i = 0;
		for (const auto& passIndex : m_ExecutionList) 
		{
			auto pass = m_Passes[passIndex].get();

			PipelineBarriers(cmd, passIndex);

			pass->PreExecute(cmd);

			pass->Execute(cmd);

			pass->PostExecute(cmd);
		}

		g_CommandContext.EndCommandBuffer(cmd);
	}

	void RGBuilder::BuildPass(uint32_t passIndex, uint32_t level)
	{
		auto* pass = m_Passes[passIndex].get();

		// All inputs

		auto& inAttachments = pass->GetInputAttachments();
		for (auto& attachment : inAttachments)
		{
			BuildDependencies(passIndex, attachment.texture->GetWritePasses(), level);
		}

		auto& inBuffers = pass->GetInBuffers();
		for (auto& buffer : inBuffers)
		{
			BuildDependencies(passIndex, buffer.buffer->GetWritePasses(), level);
		}
		auto& inTextures = pass->GetInTextures();
		for (auto& texture : inTextures)
		{
			BuildDependencies(passIndex, texture.texture->GetWritePasses(), level);
		}

		// color/depth attachments are inouts, not just ins
#if 0
		auto& colorAttachments = pass->GetColorAttachments();
		for (auto& colorAttachment : colorAttachments) {
			if (colorAttachment.loadStoreInfo.loadOp == VK_ATTACHMENT_LOAD_OP_LOAD) {
				// TODO...
			}
		}
		auto& depthAttachment = pass->GetDepthAttachment();
		if (depthAttachment.texture && depthAttachment.loadStoreInfo.loadOp == VK_ATTACHMENT_LOAD_OP_LOAD) {

		}
#endif
	}

	void RGBuilder::BuildDependencies(uint32_t passIndex, const std::set<uint32_t>& writePasses, uint32_t level)
	{
		assert(level < (uint32_t)m_Passes.size());

		for (auto& writePass : writePasses)
			m_PassDependencies[passIndex].insert(writePass);

		// Reverse 
		std::vector<uint32_t> tmpVec(writePasses.crbegin(), writePasses.crend());
		for (auto& writePass : tmpVec)
		{
			m_ExecutionList.push_back(writePass);
			BuildPass(writePass, level + 1);
		}
	}
	
	void RGBuilder::BuildExecutionList()
	{
		m_PassDependencies.resize(m_Passes.size());

		RGResourceRef output = m_Output;

		// Simply loop through the passes
#if 0
		m_ExecuteList.resize(m_Passes.size());
		int index = 0;
		std::for_each(m_ExecuteList.begin(), m_ExecuteList.end(), [&index](uint32_t& val) {val = index++; });
#endif

		const auto& writePasses = output->GetWritePasses();
		std::vector<uint32_t> tmpVec(writePasses.crbegin(), writePasses.crend());
		for (auto index : tmpVec)
		{
			m_ExecutionList.push_back(index);
			BuildPass(index);
		}
		std::reverse(m_ExecutionList.begin(), m_ExecutionList.end());

		// Remove duplicates
		{
			std::unordered_set<uint32_t> tmpSet;
			auto si = std::begin(m_ExecutionList);
			for (auto it = std::begin(m_ExecutionList); it != std::end(m_ExecutionList); ++it)
			{
				if (tmpSet.find(*it) == tmpSet.end())
				{
					if (si != it)
						*si = *it;
					tmpSet.insert(*it);
					++si;
				}
			}
			m_ExecutionList.erase(si, std::end(m_ExecutionList));
		}
	}

	void RGBuilder::BuildResources()
	{
		uint32_t physicalResourceCount = m_ExternalResourceCount;

		auto UpdateBufferResources = [&](const auto& buffers) {
			if (!buffers.empty())
			{
				for (auto& accessed : buffers)
				{
					auto bufferRef = accessed.buffer;
					if (!bufferRef->isExternal && bufferRef->GetPhysicalResource() == nullptr)
					{
						bufferRef->SetPhysicalResource(m_ResourcePool->CreateBuffer(bufferRef->desc, bufferRef->GetName()));
						bufferRef->physicalIndex = physicalResourceCount++;
					}
				}
			}
		};
		auto UpdateTextureResources = [&](const auto& textures) {
			if (!textures.empty())
			{
				for (auto& accessed : textures)
				{
					auto textureRef = accessed.texture;
					if (!textureRef->isExternal && textureRef->GetPhysicalResource() == nullptr)
					{
						textureRef->SetPhysicalResource(m_ResourcePool->CreateTexture(textureRef->desc, textureRef->GetName()));
						textureRef->physicalIndex = physicalResourceCount++;
					}
				}
			}
		};

		for (const auto& passIndex : m_ExecutionList)
		{
			auto pass = m_Passes[passIndex].get();

			auto& inBuffers = pass->GetInBuffers();
			UpdateBufferResources(inBuffers);

			auto& outBuffers = pass->GetOutBuffers();
			UpdateBufferResources(outBuffers);

			auto& inTextures = pass->GetInTextures();
			UpdateTextureResources(inTextures);

			auto& outTextures = pass->GetOutTextures();
			UpdateTextureResources(outTextures);

			auto& inAttachments = pass->GetInputAttachments();
			UpdateTextureResources(inAttachments);

			auto& colorAttachments = pass->GetColorAttachments();
			UpdateTextureResources(colorAttachments);

			auto& depthAttachment = pass->GetDepthAttachment();
			{
				auto textureRef = depthAttachment.texture;
				if (textureRef != nullptr && !textureRef->isExternal && textureRef->GetPhysicalResource() == nullptr)
				{
					textureRef->SetPhysicalResource(m_ResourcePool->CreateTexture(textureRef->desc, textureRef->GetName()));
					textureRef->physicalIndex = physicalResourceCount++;
				}
			}

		}

		m_PhysicalResourceCount = physicalResourceCount;
	}

	void RGBuilder::BuildBarriers()
	{
#if 0
		struct Barrier
		{
			VkPipelineStageFlags2 srcStageMask{ 0 };
			VkPipelineStageFlags2 dstStageMask{ 0 };
			VkAccessFlags2 srcAccessMask{ 0 };
			VkAccessFlags2 dstAccessMask{ 0 };
			VkImageLayout srcLayout{ VK_IMAGE_LAYOUT_UNDEFINED };
			VkImageLayout dstLayout{ VK_IMAGE_LAYOUT_UNDEFINED };
		};
#endif

		std::vector<Barrier> barriers(m_PhysicalResourceCount);

		auto& passBarrierList = m_PassBarriers;
		passBarrierList.resize(m_ExecutionList.size());

		auto UpdatePassBufferBarriers = [&](std::vector<Barrier>& passBarriers, const auto& buffers) {
			if (!buffers.empty()) {
				for (auto& accessed : buffers) {
					auto bufferRef = accessed.buffer;
					if (bufferRef != nullptr && bufferRef->GetPhysicalResource() != nullptr && bufferRef->physicalIndex != g_InvalidHandle) {
						auto& barrier = barriers[bufferRef->physicalIndex];
						if (!(barrier.dstAccessMask & accessed.access.access)) {
							passBarriers.emplace_back(Barrier{});
							auto& newBarrier = passBarriers.back();
							newBarrier.srcAccessMask = barrier.dstAccessMask;
							newBarrier.srcStageMask = barrier.dstStageMask;
							newBarrier.dstAccessMask = barrier.dstAccessMask = accessed.access.access;
							newBarrier.dstStageMask = barrier.dstStageMask = accessed.access.pipelineStage;
						}
					}
				}
			}
		};
		auto UpdatePassImageBarriers = [&](std::vector<Barrier>& passBarriers, const auto& textures) {
			if (!textures.empty()) {
				for (auto& accessed : textures) {
					auto textureRef = accessed.texture;
					if (textureRef != nullptr && textureRef->GetPhysicalResource() != nullptr && textureRef->physicalIndex != g_InvalidHandle) {
						auto& barrier = barriers[textureRef->physicalIndex];
						if (!(barrier.dstAccessMask & accessed.access.access) || !(barrier.dstLayout & accessed.layout)) {
							passBarriers.emplace_back(Barrier{});
							auto& newBarrier = passBarriers.back();
							newBarrier.srcAccessMask = barrier.dstAccessMask;
							newBarrier.srcStageMask = barrier.dstStageMask;
							newBarrier.srcLayout = barrier.dstLayout;
							newBarrier.dstAccessMask = barrier.dstAccessMask = accessed.access.access;
							newBarrier.dstStageMask = barrier.dstStageMask = accessed.access.pipelineStage;
							newBarrier.dstLayout = barrier.dstLayout = accessed.layout;
						}
					}
				}
			}
		};

		uint32_t i = 0;
		for (const auto& passIndex : m_ExecutionList)
		{
			auto pass = m_Passes[passIndex].get();
			auto& passBarriers = passBarrierList[i++];

			auto& inBuffers = pass->GetInBuffers();
			UpdatePassBufferBarriers(passBarriers, inBuffers);
			
			auto& outBuffers = pass->GetOutBuffers();
			UpdatePassBufferBarriers(passBarriers, outBuffers);
			
			auto& inTextures = pass->GetInTextures();
			UpdatePassImageBarriers(passBarriers, inTextures);
			
			auto& outTextures = pass->GetOutTextures();
			UpdatePassImageBarriers(passBarriers, outTextures);
			
			auto& inAttachments = pass->GetInputAttachments();
			UpdatePassImageBarriers(passBarriers, inAttachments);
			
			auto& colorAttachments = pass->GetColorAttachments();
			UpdatePassImageBarriers(passBarriers, colorAttachments);
			
			auto& depthAttachment = pass->GetDepthAttachment();
			{
				auto textureRef = depthAttachment.texture;
				if (textureRef != nullptr && textureRef->GetPhysicalResource() != nullptr && textureRef->physicalIndex != g_InvalidHandle) 
				{
					auto& barrier = barriers[textureRef->physicalIndex];
					if (!(barrier.dstAccessMask & depthAttachment.access.access) || !(barrier.dstLayout & depthAttachment.layout)) 
					{
						passBarriers.emplace_back(Barrier{});
						auto& newBarrier = passBarriers.back();
						newBarrier.srcAccessMask = barrier.dstAccessMask;
						newBarrier.srcStageMask = barrier.dstStageMask;
						newBarrier.srcLayout = barrier.dstLayout;
						newBarrier.dstAccessMask = barrier.dstAccessMask = depthAttachment.access.access;
						newBarrier.dstStageMask = barrier.dstStageMask = depthAttachment.access.pipelineStage;
						newBarrier.dstLayout = barrier.dstLayout = depthAttachment.layout;
					}
				}
			}

		}
	}

	void RGBuilder::PipelineBarriers(VkCommandBuffer cmd, uint32_t passIndex)
	{
		uint32_t barrierOffset = 0;
		auto BufferBarriers = [&barrierOffset](const auto& inBuffers, const std::vector<Barrier>& inBarriers) {
			if (!inBuffers.empty())
			{
				for (const auto &accessed : inBuffers)
				{
					if (accessed.buffer != nullptr)
					{
						const auto& barrier = inBarriers[barrierOffset++];

						auto bufferRef = accessed.buffer;
						auto buffer = bufferRef->GetPhysicalResource();
						g_CommandContext.BufferBarrier2(*buffer, 0, bufferRef->desc.size, 
							barrier.srcStageMask, barrier.dstStageMask, barrier.srcAccessMask, barrier.dstAccessMask);
					}
				}
			}
		};
		auto ImageBarriers = [&barrierOffset](const auto& inTextures, const std::vector<Barrier>& inBarriers)
		{
			if (!inTextures.empty())
			{
				for (const auto& accessed : inTextures)
				{
					if (accessed.texture != nullptr)
					{
						const auto& barrier = inBarriers[barrierOffset++];

						auto textureRef = accessed.texture;
						auto texture = textureRef->GetPhysicalResource();
						// FIXME: use different ImageView, not just views[0]
						g_CommandContext.ImageBarrier2(*texture, texture->views[0].subresourceRange,
							barrier.srcLayout, barrier.dstLayout, barrier.srcStageMask, barrier.dstStageMask, barrier.srcAccessMask, barrier.dstAccessMask);
					}
				}
			}
		};
		
		auto *pass = m_Passes[passIndex].get();
		auto &passBarriers = m_PassBarriers[passIndex];
		{
			auto& inBuffers = pass->GetInBuffers();
			BufferBarriers(inBuffers, passBarriers);

			auto& outBuffers = pass->GetOutBuffers();
			BufferBarriers(outBuffers, passBarriers);

			auto& inTextures = pass->GetInTextures();
			ImageBarriers(inTextures, passBarriers);

			auto& outTextures = pass->GetOutTextures();
			ImageBarriers(outTextures, passBarriers);

			auto& inAttachments = pass->GetInputAttachments();
			ImageBarriers(inAttachments, passBarriers);

			auto& colorAttachments = pass->GetColorAttachments();
			ImageBarriers(colorAttachments, passBarriers);

			auto& depthAttachment = pass->GetDepthAttachment();
			{
				auto textureRef = depthAttachment.texture;
				if (textureRef != nullptr && textureRef->GetPhysicalResource() != nullptr && textureRef->physicalIndex != g_InvalidHandle)
				{
					auto* texture = textureRef->GetPhysicalResource();
					auto& barrier = passBarriers[barrierOffset++];
					g_CommandContext.ImageBarrier2(*texture, texture->views[0].subresourceRange,
						barrier.srcLayout, barrier.dstLayout, barrier.srcStageMask, barrier.dstStageMask, barrier.srcAccessMask, barrier.dstAccessMask);
				}
			}

			g_CommandContext.PipelineBarriers2(cmd);
		}
	}
}
