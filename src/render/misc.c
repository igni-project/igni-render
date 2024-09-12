#include "misc.h"
#include "common/maths.h"
#include "config.h"
#include <stdio.h> 
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

int generateMipmaps(
	VkPhysicalDevice physDev,
	VkCommandBuffer cmdBuf,
	VkQueue queue,
	VkImage image,
	VkFormat format,
	int32_t width,
	int32_t height,
	uint32_t mipLevels
)
{
	/* Generating mipmaps relies on linear filtering. If support for linear
	 * filtering is not found, no mipmap will be generated. */
	VkFormatProperties formatProperties; 
	vkGetPhysicalDeviceFormatProperties(physDev, format, &formatProperties);

	if (!(
		formatProperties.optimalTilingFeatures
		& VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT
	)) {
		printf("Failed to generate mipmaps\n");
		return -1;
	}

	beginSingleTimeCommands(cmdBuf);

	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.image = image;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.subresourceRange.levelCount = 1;

	int32_t mipWidth = width;
	int32_t mipHeight = height;

	for (uint32_t i = 1; i < mipLevels; i++) {
		barrier.subresourceRange.baseMipLevel = i - 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

		vkCmdPipelineBarrier(
			cmdBuf,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			0,
			0, 0,
			0, 0,
			1, &barrier
		);

		VkImageBlit blit = {0};
		blit.srcOffsets[1].x = mipWidth;
		blit.srcOffsets[1].y = mipHeight;
		blit.srcOffsets[1].z = 1;

		blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.srcSubresource.mipLevel = i - 1;
		blit.srcSubresource.baseArrayLayer = 0;
		blit.srcSubresource.layerCount = 1;

		blit.dstOffsets[1].x = mipWidth > 1 ? mipWidth / 2 : 1;
		blit.dstOffsets[1].y = mipHeight > 1 ? mipHeight / 2 : 1;
		blit.dstOffsets[1].z = 1;

		blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.dstSubresource.mipLevel = i;
		blit.dstSubresource.baseArrayLayer = 0;
		blit.dstSubresource.layerCount = 1;

		vkCmdBlitImage(
			cmdBuf,
			image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &blit,
			VK_FILTER_LINEAR
		);

		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(
			cmdBuf,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			0,
			0, NULL,
			0, NULL,
			1, &barrier
		);

		/* Each mip level is half the size of the previous one.
		 * An image with a width or height of zero breaks things, so that can't
		 * happen. */
		if (mipWidth > 1) mipWidth /= 2;
		if (mipHeight > 1) mipHeight /= 2;
	}

	barrier.subresourceRange.baseMipLevel = mipLevels - 1;
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	vkCmdPipelineBarrier(
		cmdBuf,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		0,
		0, 0,
		0, 0,
		1, &barrier
	);

	endSingleTimeCommands(cmdBuf, queue);

	return 0;
}

int createSampler(
	VkDevice device,
	VkPhysicalDevice physDev,
	VkSampler* sampler,
	float mipLevels
)
{
	VkPhysicalDeviceFeatures supportedFeatures;
	vkGetPhysicalDeviceFeatures(physDev, &supportedFeatures);

	VkPhysicalDeviceProperties properties = {};
	vkGetPhysicalDeviceProperties(physDev, &properties);

	VkSamplerCreateInfo samplerInfo = {};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.anisotropyEnable =
		supportedFeatures.samplerAnisotropy ? VK_TRUE : VK_FALSE;
	samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = mipLevels;

	if (vkCreateSampler(device, &samplerInfo, 0, sampler) != VK_SUCCESS) {
		printf("Failed to create sampler.\n");
		return -1;
	}

	return 0;
}

int copyBufferToImage(
	VkCommandBuffer cmdBuf,
	VkQueue queue,
	VkBuffer buffer,
	VkImage image,
	uint32_t width,  
	uint32_t height
)
{
	beginSingleTimeCommands(cmdBuf);

	VkBufferImageCopy region = {};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;
	region.imageOffset.x = 0;
	region.imageOffset.y = 0;
	region.imageOffset.z = 0;
	region.imageExtent.width = width;
	region.imageExtent.height = height;

	/* Not to be confused with depth as in "bits per pixel", this does
	 * something else. */
	region.imageExtent.depth = 1;

	vkCmdCopyBufferToImage(
		cmdBuf,
		buffer,
		image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1,
		&region
	);

	endSingleTimeCommands(cmdBuf, queue);

	return 0;
}

int transitionImageLayout(
	VkCommandBuffer cmdBuf,
	VkQueue queue,
	VkImage image,
	VkFormat format,
	VkImageLayout oldLayout,
	VkImageLayout newLayout,
	uint32_t mipLevels
)
{
	beginSingleTimeCommands(cmdBuf);

	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;

	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = mipLevels;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	VkPipelineStageFlags srcStage;
	VkPipelineStageFlags dstStage;

	/* subresourceRange access mask logic */

	if (
		oldLayout == VK_IMAGE_LAYOUT_UNDEFINED
		&& newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
	) {
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	} else if (
		oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
		&& newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	) {
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	} else {
		printf("Unsupported layout transition.\n");
		return -1;
	}

	vkCmdPipelineBarrier(
		cmdBuf,
		srcStage,
		dstStage,
		0, 0, 0, 0, 0, 1,
		&barrier
	);

	endSingleTimeCommands(cmdBuf, queue);

	return 0;
}

int createUniformBuffers(
	VkDevice device,
	VkPhysicalDevice physDev,
	VkBuffer* buffer,
	VkDeviceMemory* memory,
	void** mappedMemory,
	VkDeviceSize size
)
{
	if (createBuffer(
		device,
		physDev,
		size,
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
		| VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		buffer,
		memory
	)) {
		return -1;
	}

	if (vkMapMemory(device, *memory, 0, size, 0, mappedMemory) != VK_SUCCESS) {
		printf("createUniformBuffers() failed: Could not map memory\n");
		return -1;
	}

	return 0;
}

void beginSingleTimeCommands(VkCommandBuffer cmdBuf)
{
	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	
	vkBeginCommandBuffer(cmdBuf, &beginInfo);
}

void endSingleTimeCommands(VkCommandBuffer cmdBuf, VkQueue queue)
{
	vkEndCommandBuffer(cmdBuf);

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cmdBuf;

	vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(queue);
}

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
)
{
	VkBuffer stagingBuffer = 0;
	VkDeviceMemory stagingBufferMemory = 0;

	if (createBuffer(
		device,
		physDev,
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
		| VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&stagingBuffer,
		&stagingBufferMemory
	)) {
		printf("Failed to create staging buffer\n");
		return -1;
	}

	void* mappedData;
	vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &mappedData);

	memcpy(mappedData, data, bufferSize);
	vkUnmapMemory(device, stagingBufferMemory);

	createBuffer(
		device,
		physDev,
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		buffer,
		mem
	);

	copyBuffer(command, queue, stagingBuffer, *buffer, bufferSize);

	vkDestroyBuffer(device, stagingBuffer, 0);
	vkFreeMemory(device, stagingBufferMemory, 0);

	return 0;
}

int createCommandBuffers(
	VkDevice device,
	VkCommandPool pool,
	VkCommandBuffer* buffer
)
{
	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = pool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 1;

	if (vkAllocateCommandBuffers(device, &allocInfo, buffer) != VK_SUCCESS) {
		printf("Failed to allocate command buffers\n");
		return -1;
	}


	return 0;
}


int copyBuffer(
	VkCommandBuffer cmdBuf,
	VkQueue queue,
	VkBuffer src,
	VkBuffer dst,
	VkDeviceSize size
)
{
	beginSingleTimeCommands(cmdBuf);

	VkBufferCopy copyRegion = {};
	copyRegion.size = size;

	vkCmdCopyBuffer(cmdBuf, src, dst, 1, &copyRegion);

	endSingleTimeCommands(cmdBuf, queue);

	return 0;
}

int createVertexBuffer(
	VkCommandBuffer command,
	VkQueue queue,
	VkDevice device,
	VkPhysicalDevice physDev,
	const void* data,
	VkDeviceSize bufferSize,
	VkBuffer* buffer,
	VkDeviceMemory* mem
)
{
	VkBuffer stagingBuffer = 0;
	VkDeviceMemory stagingBufferMemory = 0;

	if (createBuffer(
		device,
		physDev,
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
		| VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&stagingBuffer,
		&stagingBufferMemory
	)) {
		printf("Failed to create staging buffer\n");
		return -1;
	}

	void* mappedData;
	vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &mappedData);

	memcpy(mappedData, data, bufferSize);
	vkUnmapMemory(device, stagingBufferMemory);

	createBuffer(
		device,
		physDev,
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		buffer,
		mem
	);

	copyBuffer(command, queue, stagingBuffer, *buffer, bufferSize);

	vkDestroyBuffer(device, stagingBuffer, 0);
	vkFreeMemory(device, stagingBufferMemory, 0);

	return 0;
}

int createBuffer(
	VkDevice device,
	VkPhysicalDevice physDev,
	VkDeviceSize size,
	VkBufferUsageFlags usage,
	VkMemoryPropertyFlags properties,
	VkBuffer* buffer,
	VkDeviceMemory* bufferMemory
)
{
	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateBuffer(device, &bufferInfo, 0, buffer) != VK_SUCCESS) {
		printf("Failed to create buffer\n");
		return -1;
	}

	VkMemoryRequirements memRequirements = {};
	vkGetBufferMemoryRequirements(device, *buffer, &memRequirements);

	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(
		physDev,
		memRequirements.memoryTypeBits,
		properties
	);

	if (vkAllocateMemory(device, &allocInfo, 0, bufferMemory) != VK_SUCCESS) {
		printf("Failed to allocate buffer memory\n");
		return -1;
	}

	vkBindBufferMemory(device, *buffer, *bufferMemory, 0);

	return 0;
}

int loadShaderModule(VkDevice device, VkShaderModule* mod, const char* path)
{
	FILE* file = fopen(path, "r");

	if (!file) {
		perror("Failed to open shader file");
		return -1;
	}

	struct stat filestat;

	if (stat(path, &filestat) == -1) {
		perror("Failed to retrieve file statistics");
		return -1;
	}

	char code[filestat.st_size] = {};

	if (!code) {
		perror("Failed to allocate memory for shader");
		return -1;
	}

	if (!fread(code, 1, filestat.st_size, file)) {
		/* A janky way of making perror print a formatted string. */
		printf("Failed to read shader file (%s)", path);
		perror("");

		return -1;
	}

	VkShaderModuleCreateInfo shaderModuleInfo = {};
	shaderModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderModuleInfo.codeSize = filestat.st_size;
	shaderModuleInfo.pCode = (const uint32_t*)code;

	VkResult result = vkCreateShaderModule(device, &shaderModuleInfo, 0, mod);

	fclose(file);

	if (result != VK_SUCCESS) {
		printf("Failed to create shader module.\n");
		return -1;
	}

	return 0;
}

int createImageView(
	VkDevice device,
	VkImage image,
	VkFormat format,
	VkImageAspectFlags aspectFlags,
	uint32_t mipLevels,
	VkImageView* imageView
)
{
	VkImageViewCreateInfo viewInfo = {};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = format;
	viewInfo.subresourceRange.aspectMask = aspectFlags;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = mipLevels;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	if (vkCreateImageView(device, &viewInfo, 0, imageView) != VK_SUCCESS) {
		return -1;
	}

	return 0;
}	

uint32_t findMemoryType(
	VkPhysicalDevice physDev,
	uint32_t typeFilter,
	VkMemoryPropertyFlags properties
)
{
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(physDev, &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
		if (
			(typeFilter & (1 << i))
			&& (memProperties.memoryTypes[i].propertyFlags & properties)
			== properties
		) {
			return i;
		}
	}

	printf("Failed to find a suitable memory type.\n");

	return -1;
}


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
)
{
	VkImageCreateInfo imageInfo = {};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = width;
	imageInfo.extent.height = height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = mipLevels;
	imageInfo.arrayLayers = 1;
	imageInfo.format = format;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = usage;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

	if (vkCreateImage(device, &imageInfo, 0, image) != VK_SUCCESS) {
		printf("Failed to create image\n");
		return -1;
	}

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(device, *image, &memRequirements);

	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(
		physDev,
		memRequirements.memoryTypeBits,
		properties
	);

	if (vkAllocateMemory(device, &allocInfo, 0, imageMemory) != VK_SUCCESS) {
		printf("Failed to allocate image memory\n");
		return -1;

	}

	vkBindImageMemory(device, *image, *imageMemory, 0);
	return 0;
}

int getAvailableInstanceLayers(
	const char*** result,
	const char** queries,
	unsigned int queryCount
)
{
	if (!queries) {
		return 0;
	}

	*result = (const char**)malloc(queryCount * sizeof(*queries));

	unsigned int layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, NULL);
	VkLayerProperties layers[layerCount];
	vkEnumerateInstanceLayerProperties(&layerCount, layers);

	unsigned int availableLayerCount = 0;
	for (int i = 0; i < queryCount; i++) {
		int layerFound = 0;

		for (int ii = 0; ii < layerCount; ii++) {
			layerFound |= !strcmp(queries[i], layers[ii].layerName);
		}

		if (!layerFound) {
			printf("Vulkan instance layer not found: %s\n", queries[i]);
		} else {
			/* Allocate and add layer name to list of available layers */

			(*result)[availableLayerCount] =
				(const char*)malloc(strlen(queries[i]));

			strcpy((char*)((*result)[availableLayerCount]), queries[i]);
			availableLayerCount++;
		}
	}

	return availableLayerCount;
}

