#include "Image.h"
#include "Device.h"
#include "VkCommon.h"
#include "Buffer.h"
#include "CommandManager.h"
#include "Utilities.h"
#include "Renderer.h"

#include <iostream>

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"


namespace Niagara
{
	/// ImageView

	void ImageView::Init(const Device &device, Image& image, VkImageViewType viewType, uint32_t baseMipLevel, uint32_t baseArrayLayer, uint32_t mipLevels, uint32_t arrayLayers)
	{
		Destroy(device);

		if (IsDepthStencilFormat(image.format))
			subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		else 
			subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.baseMipLevel = baseMipLevel;
		subresourceRange.levelCount = mipLevels;
		subresourceRange.baseArrayLayer = baseArrayLayer;
		subresourceRange.layerCount = arrayLayers;

		VkImageViewCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.format = image.format;
		createInfo.image = image.image;
		createInfo.subresourceRange = subresourceRange;
		createInfo.viewType = viewType;
		
		VK_CHECK(vkCreateImageView(device, &createInfo, nullptr, &view));
		assert(view);

		this->image = &image;
	}

	void ImageView::Destroy(const Device &device)
	{
		if (view != VK_NULL_HANDLE)
		{
			vkDestroyImageView(device, view, nullptr);
			view = VK_NULL_HANDLE;
		}
	}


	/// Sampler

	void Sampler::Init(const Device& device, VkFilter filter, VkSamplerMipmapMode mipmapMode, VkSamplerAddressMode addressMode, float maxAnisotropy, VkCompareOp compareOp, VkSamplerReductionMode reductionMode)
	{
		Destroy(device);

		VkSamplerCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		createInfo.magFilter = filter;
		createInfo.minFilter = filter;
		createInfo.mipmapMode = mipmapMode;
		createInfo.addressModeU = addressMode;
		createInfo.addressModeV = addressMode;
		createInfo.addressModeW = addressMode;
		createInfo.mipLodBias = 0;
		createInfo.anisotropyEnable = maxAnisotropy > 0 ? VK_TRUE : VK_FALSE;
		createInfo.maxAnisotropy = maxAnisotropy;
		createInfo.compareEnable = compareOp != VK_COMPARE_OP_NEVER ? VK_TRUE : VK_FALSE;
		createInfo.compareOp = compareOp;
		createInfo.minLod = 0.0f;
		createInfo.maxLod = 16.0f;

		// Reduction mode, eg. min depth pyramid
		VkSamplerReductionModeCreateInfo reductionModeCreateInfo{};
		if (reductionMode != VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE)
		{
			reductionModeCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO;
			reductionModeCreateInfo.reductionMode = reductionMode;
			createInfo.pNext = &reductionModeCreateInfo;
		}
		
		VK_CHECK(vkCreateSampler(device, &createInfo, nullptr, &sampler));
		assert(sampler);
	}

	void Sampler::Destroy(const Device& device)
	{
		if (sampler != VK_NULL_HANDLE)
		{
			vkDestroySampler(device, sampler, nullptr);
			sampler = VK_NULL_HANDLE;
		}
	}


	/// Image

	VkClearValue Image::s_ClearBlack = { 0, 0, 0,0 };
	VkClearValue Image::s_ClearDepth = { 0.0f, 0 };

	static VkImageType GetImageType(VkExtent3D extent)
	{
		VkImageType type{};

		uint32_t dimNum = 0;

		if (extent.width > 1)
			dimNum++;
		if (extent.height > 1)
			dimNum++;
		if (extent.depth > 1)
			dimNum++;

		// Default to 2 (w == 1, h == 1)
		if (dimNum == 0)
			dimNum = 2;

		switch (dimNum)
		{
		case 1:
			type = VK_IMAGE_TYPE_1D;
			break;
		case 2:
			type = VK_IMAGE_TYPE_2D;
			break;
		case 3:
			type = VK_IMAGE_TYPE_3D;
			break;
		default:
			throw std::runtime_error("No image type found.");
			break;
		}

		return type;
	}

	static VkImageViewType GetImageViewType(VkImageType imageType, uint32_t arrayLayers = 1, bool cubeImage = false)
	{
		switch (imageType)
		{
		case VK_IMAGE_TYPE_1D:
			return arrayLayers > 1 ? VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_IMAGE_VIEW_TYPE_1D;
		case VK_IMAGE_TYPE_2D:
			return arrayLayers > 1 ? 
				cubeImage && arrayLayers == 6 ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D_ARRAY : 
				VK_IMAGE_VIEW_TYPE_2D;
		case VK_IMAGE_TYPE_3D:
			return VK_IMAGE_VIEW_TYPE_3D;
		default:
			throw std::runtime_error("No image view type found.");
			return VK_IMAGE_VIEW_TYPE_2D;
		}
	}

	/**
	* Tiling
	* * VK_IMAGE_TILING_LINEAR: Texels are laid out in row-major order 
	* * VK_IMAGE_TILING_OPTIMAL: Texels are laid out in an implementation defined order for optimal access
	*/
	void Image::Init(const Device &device, const VkExtent3D& extent, VkFormat format, 
		VkImageUsageFlags imageUsage, VkImageCreateFlags flags, VkMemoryPropertyFlags memPropertyFlags,
		uint32_t mipLevels, uint32_t arrayLayers, VkClearValue clearValue,
		VkSampleCountFlagBits sampleCount, VkImageTiling tiling,
		const uint32_t* queueFamilies, uint32_t queueFamilyCount)
	{
		Destroy(device);

		if (mipLevels == 0)
			mipLevels = GetMipLevels(extent.width, extent.height);

		this->type = GetImageType(extent);
		this->extent = extent;
		this->format = format;
		this->sampleCount = sampleCount;
		this->usage = imageUsage;
		this->arrayLayers = arrayLayers;
		this->clearValue = clearValue;
		this->tiling = tiling;

		subresource.arrayLayer = arrayLayers;
		subresource.mipLevel = mipLevels;
		subresource.aspectMask = IsDepthStencilFormat(format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

		VkImageCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		createInfo.flags = flags;
		createInfo.imageType = type;
		createInfo.format = format;
		createInfo.extent = extent;
		createInfo.mipLevels = mipLevels;
		createInfo.arrayLayers = arrayLayers;
		createInfo.samples = sampleCount;
		createInfo.tiling = tiling;
		createInfo.usage = imageUsage;

		if (queueFamilies != nullptr)
		{
			createInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = queueFamilyCount;
			createInfo.pQueueFamilyIndices = queueFamilies;
		}

		VK_CHECK(vkCreateImage(device, &createInfo, nullptr, &image));
		assert(image);

		VkMemoryRequirements memRequirements{};
		vkGetImageMemoryRequirements(device, image, &memRequirements);

		if (usage & VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT)
			memPropertyFlags = VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;

		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		VkBool32 memTypeFound = VK_FALSE;
		allocInfo.memoryTypeIndex = device.GetMemoryType(memRequirements.memoryTypeBits, memPropertyFlags, &memTypeFound);
		assert(memTypeFound);

		VK_CHECK(vkAllocateMemory(device, &allocInfo, nullptr, &memory));
		assert(memory);

		vkBindImageMemory(device, image, memory, 0);

		// Create default view
		CreateImageView(device, GetImageViewType(type, arrayLayers), 0, 0, mipLevels, arrayLayers);

		layout = VK_IMAGE_LAYOUT_UNDEFINED;
		if (name != "")
			g_AccessMgr.AddResourceAccess(name);
	}

	void Image::Destroy(const Device& device)
	{
		if (!views.empty())
		{
			for (auto& view : views)
				view.Destroy(device);

			views.clear();
		}

		if (memory != VK_NULL_HANDLE)
		{
			// Unmap(device);
			vkFreeMemory(device, memory, nullptr);
		}
		if (image != VK_NULL_HANDLE)
			vkDestroyImage(device, image, nullptr);
	}

	const ImageView& Image::CreateImageView(const Device &device, VkImageViewType viewType, uint32_t baseMipLevel, uint32_t baseArrayLayer, uint32_t mipLevels, uint32_t arrayLayers)
	{
		views.push_back({});

		auto &view = views.back();
		view.Init(device, *this, viewType, baseMipLevel, baseArrayLayer, mipLevels > 0 ? mipLevels : subresource.mipLevel, arrayLayers > 0 ? arrayLayers : subresource.arrayLayer);

		return view;
	}

	uint8_t* Image::Map(const Device &device)
	{
		if (!mappedData)
		{
			if (tiling != VK_IMAGE_TILING_LINEAR)
				std::cerr << "Mapping image memory that is not linear.\n";
			
			// vkMapMemory(device, memory, VkDeviceSize(0), )
			isMapped = true;
		}

		return mappedData;
	}

	void Image::Unmap(const Device &device)
	{
		vkUnmapMemory(device, memory);
		mappedData = nullptr;
		isMapped = false;
	}


	/// Texture

	void Texture::Create2D(const Device& device, uint32_t rowPitchBytes, uint32_t width, uint32_t height, VkFormat format, const void* pInitData)
	{
		VkExtent3D extent{ width, height, 1 };
		Init(device, extent, format, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 0, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 1, 1);

		uint32_t size = rowPitchBytes * height;
		InitializeTexture(device, *this, pInitData, size);

		layout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
	}

	void Texture::Create2D(const Device& device, uint32_t width, uint32_t height, VkFormat format, const void* pInitData)
	{
		// TODO...
#if 0
		VkExtent3D extent{ width, height, 1 };
		Init(device, extent, format, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 0, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			0, 1, Image::s_ClearBlack);

		uint32_t size = 1 * width * height;

		InitializeTexture(device, *this, pInitData, size);
#endif

		layout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
	}

	void Texture::CreateCube(const  Device& device, uint32_t rowPitchBytes, uint32_t width, uint32_t  height, VkFormat format, const void* pInitData)
	{
		// TODO...
	}

	void ManagedTexture::WaitForLoad() 
	{
		while (m_IsLoading) 
			std::this_thread::yield();
	}

	void ManagedTexture::Unload() 
	{
		// TODO...
	}

	void ManagedTexture::SetDefault(EDefaultTexture defaultTex)
	{
		if (!views.empty())
			views.resize(1);

		views[0] = TextureManager::GetDefaultTexture(defaultTex).views[0];
	}

	void ManagedTexture::SetToInvalidTexture()
	{
		if (views.empty())
			views.resize(1);

		views[0] = TextureManager::GetMagentaTex2D().views[0];
		m_IsValid = false;
	}


	/// TextureManager
	
	TextureManager g_TextureMgr{};
	Texture TextureManager::s_DefaultTexture[(int)EDefaultTexture::kNumDefaultTextures];

	void TextureManager::Init(const Device &device, const std::string& textureRootPath)
	{
		m_RootPath = textureRootPath;
		InitDefaultTextures(device);
	}

	void TextureManager::Cleanup(const Device& device)
	{
		ReleaseCache(device);
		DestroyDefaultTextures(device);
	}

	std::pair<ManagedTexture*, bool> TextureManager::FindOrLoadTexture(const std::string& fileName, bool sRGB)
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);

		std::string key = fileName;
		if (sRGB)
			key += "_SRGB";

		// Searching for an existing managed texture
		auto iter = m_TextureCache.find(key);
		// If it's found, it has already been loaded or the load process has begun
		if (iter != m_TextureCache.end())
			return std::make_pair(iter->second.get(), false);

		ManagedTexture* newTexture = new ManagedTexture(key);
		m_TextureCache[key].reset(newTexture);

		// This was the first time it was requested, so indicate that the caller must read the file
		return std::make_pair(newTexture, true);
	}

	const ManagedTexture* TextureManager::LoadFromFile(const Device &device, const std::string& fileName, bool sRGB)
	{
		auto managedTexPair = FindOrLoadTexture(fileName, sRGB);

		auto tex = managedTexPair.first;
		const bool requestLoad = managedTexPair.second;

		if (!requestLoad)
		{
			tex->WaitForLoad();
			return tex;
		}

		// Load
		{
			const bool forceRGBA = true;
			int x, y, comp;
			auto rawData = stbi_load((m_RootPath +fileName).data(), &x, &y, &comp, forceRGBA ? STBI_rgb_alpha : 0); // 0
			if (rawData)
			{
				comp = forceRGBA ? 4 : comp;

				// This format matches exactly with the pixels loaded from the lib
				VkFormat format;
				switch (comp)
				{
				case 1:
					format = VK_FORMAT_R8_UNORM; break;
				case 2:
					format = VK_FORMAT_R8G8_UNORM; break;
					// ??? 
					// Format VK_FORMAT_R8G8B8_UNORM is not supported for this combination of parameters
				case 3:
					format = sRGB ? VK_FORMAT_R8G8B8_SRGB : VK_FORMAT_R8G8B8_UNORM; break;
				default:
				case 4:
					format = sRGB ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM; break;
				}

				VkDeviceSize size = x * y * comp;
				uint32_t rowPitchBytes = x * comp;
				tex->Create2D(device, rowPitchBytes, x, y, format, rawData);

				stbi_image_free(rawData);
			}
			else
			{
				tex->SetToInvalidTexture();
			}

			tex->m_IsLoading = false;
		}

		return tex;
	}

	void TextureManager::ReleaseCache(const Device& device)
	{
		for (const auto& c : m_TextureCache)
		{
			c.second->Destroy(device);
		}

		m_TextureCache.clear();
	}

	const Texture& TextureManager::GetBlackTex2D()
	{
		auto managedTexPair = g_TextureMgr.FindOrLoadTexture("DefaultBlackTexture");

		auto tex = managedTexPair.first;
		const bool requestLoad = managedTexPair.second;

		if (!requestLoad)
		{
			tex->WaitForLoad();
			return *tex;
		}

		uint32_t pixel = 0;
		tex->Create2D(*g_Device, 1, 1, VK_FORMAT_R8G8B8A8_UNORM, &pixel);
		tex->m_IsLoading = false;

		return *tex;
	}
	const Texture& TextureManager::GetWhiteTex2D()
	{
		auto managedTexPair = g_TextureMgr.FindOrLoadTexture("DefaultWhiteTexture");

		auto tex = managedTexPair.first;
		const bool requestLoad = managedTexPair.second;

		if (!requestLoad)
		{
			tex->WaitForLoad();
			return *tex;
		}

		uint32_t pixel = 0xFFFFFFFFul;
		tex->Create2D(*g_Device, 1, 1, VK_FORMAT_R8G8B8A8_UNORM, &pixel);
		tex->m_IsLoading = false;

		return *tex;
	}
	const Texture& TextureManager::GetMagentaTex2D()
	{
		auto managedTexPair = g_TextureMgr.FindOrLoadTexture("DefaultMagentaTexture");

		auto tex = managedTexPair.first;
		const bool requestLoad = managedTexPair.second;

		if (!requestLoad)
		{
			tex->WaitForLoad();
			return *tex;
		}

		uint32_t pixel = 0x00FF00FF;
		tex->Create2D(*g_Device, 1, 1, VK_FORMAT_R8G8B8A8_UNORM, &pixel);
		tex->m_IsLoading = false;

		return *tex;
	}

	Texture& TextureManager::GetDefaultTexture(EDefaultTexture texId)
	{
		assert(texId < EDefaultTexture::kNumDefaultTextures);
		return s_DefaultTexture[(int)texId];
	}

	void TextureManager::InitDefaultTextures(const Device& device)
	{
		const VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;

		uint32_t MagentPixel = 0xFFFF00FF;
		s_DefaultTexture[(int)EDefaultTexture::kMagenta2D].Create2D(device, 4, 1, 1, format, &MagentPixel);
		uint32_t BlackOpaqueTexel = 0xFF000000;
		s_DefaultTexture[(int)EDefaultTexture::kBlackOpaque2D].Create2D(device, 4, 1, 1, format, &BlackOpaqueTexel);
		uint32_t BlackTransparentTexel = 0x00000000;
		s_DefaultTexture[(int)EDefaultTexture::kBlackTransparent2D].Create2D(device, 4, 1, 1, format, &BlackTransparentTexel);
		uint32_t WhiteOpaqueTexel = 0xFFFFFFFF;
		s_DefaultTexture[(int)EDefaultTexture::kWhiteOpaque2D].Create2D(device, 4, 1, 1, format, &WhiteOpaqueTexel);
		uint32_t WhiteTransparentTexel = 0x00FFFFFF;
		s_DefaultTexture[(int)EDefaultTexture::kWhiteTransparent2D].Create2D(device, 4, 1, 1, format, &WhiteTransparentTexel);
		uint32_t FlatNormalTexel = 0x00FF8080;
		s_DefaultTexture[(int)EDefaultTexture::kDefaultNormalMap].Create2D(device, 4, 1, 1, format, &FlatNormalTexel);
		// TODO:
		// uint32_t BlackCubeTexels[6] = {};
		// s_DefaultTexture[(int)EDefaultTexture::kBlackCubeMap].CreateCube(device, 4, 1, 1, format, BlackCubeTexels);
	}

	void TextureManager::DestroyDefaultTextures(const Device& device)
	{
		for (int i = 0, imax = (int)EDefaultTexture::kNumDefaultTextures; i < imax; ++i)
			s_DefaultTexture[i].Destroy(device);
	}
}
