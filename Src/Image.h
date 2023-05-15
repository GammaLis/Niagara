// Ref: Vulkan-Samples

#pragma once

#include "pch.h"
#include "Utilities.h"
#include <unordered_set>
#include <unordered_map>
#include <mutex>


namespace Niagara
{
	class Device;
	class Image;

	class ImageView
	{
	public:
		ImageView() = default;

		void Init(const Device &device, Image& image, VkImageViewType viewType,
			uint32_t baseMipLevel = 0, uint32_t baseArrayLayer = 0, uint32_t mipLevels = 1, uint32_t arrayLayers = 1);
		void Destroy(const Device &device);

		operator VkImageView() const { return view; }
		
		VkImageView view{ VK_NULL_HANDLE };
		VkImageSubresourceRange subresourceRange{};
		Image* image{ nullptr };
	};

	class Sampler
	{
	public:
		Sampler() = default;

		void Init(const Device &device, VkFilter filter = VK_FILTER_LINEAR, VkSamplerMipmapMode mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR, 
			VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, float maxAnisotropy = 0, 
			VkCompareOp compareOp = VK_COMPARE_OP_NEVER, VkSamplerReductionMode reductionMode = VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE);
		void Destroy(const Device& device);

		operator VkSampler() const { return sampler; }

		VkSampler sampler{ VK_NULL_HANDLE };
	};

	class Image
	{
	public:
		static VkClearValue s_ClearBlack;
		static VkClearValue s_ClearDepth;

		VkDeviceMemory memory{ VK_NULL_HANDLE };
		VkImage image{ VK_NULL_HANDLE };
		VkImageType type;
		VkExtent3D extent;
		VkFormat format;
		VkImageUsageFlags usage;
		VkSampleCountFlags sampleCount;
		VkImageTiling tiling;
		VkImageSubresource subresource;
		uint32_t arrayLayers = 0;
		VkClearValue clearValue{};
		std::vector<ImageView> views;

		VkImageLayout layout{ VK_IMAGE_LAYOUT_UNDEFINED };

		uint8_t* mappedData{ nullptr };
		bool isMapped{ false };

		Image() = default;
		NON_COPYABLE(Image);

		void Init(const Device &device, const VkExtent3D& extent, VkFormat format,
			VkImageUsageFlags imageUsage, VkImageCreateFlags flags, VkMemoryPropertyFlags memPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			uint32_t mipLevels = 1, uint32_t arrayLayers = 1, VkClearValue clearValue = s_ClearBlack,
			VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT, VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL,
			const uint32_t* queueFamilies = nullptr, uint32_t queueFamilyCount = 0);
		void Destroy(const Device &device);

		operator VkImage() const { return image; }

		const ImageView& CreateImageView(const Device &device, VkImageViewType viewType, uint32_t baseMipLevel = 0, uint32_t baseArrayLayer = 0, uint32_t mipLevels = 1, uint32_t arrayLayers = 1);

		uint8_t* Map(const Device &device);
		void Unmap(const Device &device);
	};


	class TextureManager;

	enum class EDefaultTexture
	{
		kMagenta2D,
		kBlackOpaque2D,
		kBlackTransparent2D,
		kWhiteOpaque2D,
		kWhiteTransparent2D,
		kDefaultNormalMap,
		kBlackCubeMap,

		kNumDefaultTextures
	};

	class Texture : public Image 
	{
	public:
		void Create2D(const Device& device, uint32_t rowPitchBytes, uint32_t width, uint32_t height, VkFormat format, const void* pInitData);
		void Create2D(const Device& device, uint32_t width, uint32_t height, VkFormat format, const void* pInitData);
		void CreateCube(const  Device& device, uint32_t rowPitchBytes, uint32_t width, uint32_t  height, VkFormat format, const void* pInitData);
	};

	class ManagedTexture : public Texture
	{
		friend class TextureManager;
	public:
		ManagedTexture() = default;
		explicit ManagedTexture(const std::string& name) : m_Name{ name } { }

		void WaitForLoad();
		void Unload();

		void SetDefault(EDefaultTexture defaultTex = EDefaultTexture::kMagenta2D);
		void SetToInvalidTexture();
		bool IsValid() const { return m_IsValid; }

	private:
		std::string m_Name;
		uint32_t m_RefCount{ 0 };
		bool m_IsValid{ false };
		bool m_IsLoading{ true };
	};

	void CreateTextureImage(const Device& device, const char* fileName, bool sRGB = false);

	class TextureManager
	{
	public:
		void Init(const Device &device, const std::string& textureRootPath);
		void Cleanup(const Device& device);

		std::pair<ManagedTexture*, bool> FindOrLoadTexture(const std::string& fileName, bool sRGB = false);
		const ManagedTexture* LoadFromFile(const Device &device, const std::string& fileName, bool sRGB = false);

		void ReleaseCache(const Device &device);

		// Static members
		static const Texture& GetBlackTex2D();
		static const Texture& GetWhiteTex2D();
		static const Texture& GetMagentaTex2D();

		static Texture s_DefaultTexture[(int)EDefaultTexture::kNumDefaultTextures];
		static Texture& GetDefaultTexture(EDefaultTexture texId);

		static void InitDefaultTextures(const Device& device);
		static void DestroyDefaultTextures(const Device &device);

	private:
		std::string m_RootPath;
		std::unordered_map<std::string, std::unique_ptr<ManagedTexture>> m_TextureCache;
		std::mutex m_Mutex;
	};
	extern TextureManager g_TextureMgr;
}
