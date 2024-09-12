#ifndef RENDER_ESSENTIAL_H
#define RENDER_ESSENTIAL_H 1

#define MAX_FRAMES_IN_FLIGHT 2

#include <stdint.h>
#include <vulkan/vulkan.h>

int generateMipmaps(
	VkPhysicalDevice physDev,
	VkCommandBuffer cmdBuf,
	VkQueue queue,
	VkImage image,
	VkFormat format,
	int32_t width,
	int32_t height,
	uint32_t mipLevels
);

int createSampler(
	VkDevice device,
	VkPhysicalDevice physDev,
	VkSampler* sampler,
	float mipLevels
);

int copyBufferToImage(
	VkCommandBuffer cmdBuf,
	VkQueue queue,
	VkBuffer buffer,
	VkImage image,
	uint32_t width,  
	uint32_t height
);

int transitionImageLayout(
	VkCommandBuffer cmdBuf,
	VkQueue queue,
	VkImage image,
	VkFormat format,
	VkImageLayout oldLayout,
	VkImageLayout newLayout,
	uint32_t mipLevels
);

int createUniformBuffers(
	VkDevice device,
	VkPhysicalDevice physDev,
	VkBuffer* buffer,
	VkDeviceMemory* memory,
	void** mappedMemory,
	VkDeviceSize size
);

void beginSingleTimeCommands(VkCommandBuffer cmdBuf);
void endSingleTimeCommands(VkCommandBuffer cmdBuf, VkQueue queue);

int createIndexBuffer(
	VkCommandBuffer command,
	VkQueue queue,
	VkDevice device,
	VkPhysicalDevice physDev,
	const void* data,
	VkDeviceSize bufferSize,
	int entrySz, 
	VkBuffer* buffer,
	VkDeviceMemory* mem
);

int createCommandBuffers(
	VkDevice device,
	VkCommandPool pool,
	VkCommandBuffer* buffer
);

int copyBuffer(
	VkCommandBuffer cmdBuf,
	VkQueue queue,
	VkBuffer src,
	VkBuffer dst,
	VkDeviceSize size
);

int createVertexBuffer(
	VkCommandBuffer command,
	VkQueue queue,
	VkDevice device,
	VkPhysicalDevice physDev,
	const void* data,
	VkDeviceSize bufferSize,
	VkBuffer* buffer,
	VkDeviceMemory* mem
);

int createBuffer(
	VkDevice device,
	VkPhysicalDevice physDev,
	VkDeviceSize size,
	VkBufferUsageFlags usage,
	VkMemoryPropertyFlags properties,
	VkBuffer* buffer,
	VkDeviceMemory* bufferMemory
);


int loadShaderModule(VkDevice device, VkShaderModule* mod, const char* path);

int createImageView(
	VkDevice device,
	VkImage image,
	VkFormat format,
	VkImageAspectFlags aspectFlags,
	uint32_t mipLevels,
	VkImageView* imageView
); 

uint32_t findMemoryType(
	VkPhysicalDevice physDev,
	uint32_t typeFilter,
	VkMemoryPropertyFlags properties
);
int createImage(
	VkDevice device,
	VkPhysicalDevice physDev,
	uint32_t width,
	uint32_t height,
	uint32_t mipLevels,
	VkFormat format,
	VkImageTiling tiling,
	VkImageUsageFlags usage,
	VkMemoryPropertyFlags properties,
	VkImage* image,
	VkDeviceMemory* imageMemory
);

int getAvailableInstanceLayers(
	const char*** result,
	const char** queries,
	unsigned int queryCount
);

#endif

