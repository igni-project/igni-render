#include "swapchain.h"
#include "common/maths.h"
#include "physdev.h"
#include <stdio.h> 
#include <string.h>
#include <stdlib.h>

SwapchainSupportInfo getSwapchainSupport(
	VkSurfaceKHR surface,
	VkPhysicalDevice device
)
{
	SwapchainSupportInfo support = {0};
	/* Get surface capabilities */

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
		device,
		surface,
		&support.capabilities
	);

	/* Get available surface formats */

	vkGetPhysicalDeviceSurfaceFormatsKHR(
		device,
		surface,
		&support.formatCount,
		NULL
	);

	/* Get available presentation modes */

	vkGetPhysicalDeviceSurfacePresentModesKHR(
		device,
		surface,
		&support.presentModeCount,
		NULL
	);

	return support;
}

int createExtendedSwapchain(
	int width,
	int height,
	ExtendedSwapchain* extSwapchain,
	LogicalDevice logicalDevice,
	VkPhysicalDevice physDev,
	VkSurfaceKHR surface
)
{
	/* I'm writing it this way to avoid putting a malloc() in
	 * getSwapchainSupport() */
	int surfaceFmtCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(physDev, surface, &surfaceFmtCount, 0);

	VkSurfaceFormatKHR surfaceFormats[surfaceFmtCount] = {};
	vkGetPhysicalDeviceSurfaceFormatsKHR(
		physDev,
		surface,
		&surfaceFmtCount,
		surfaceFormats
	);

	VkSurfaceFormatKHR surfaceFormat;
	surfaceFormat = chooseSwapSurfaceFormat(
		surfaceFormats,
		surfaceFmtCount
	);

	extSwapchain->imageFormat = surfaceFormat.format;

	int presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(
		physDev,
		surface,
		&presentModeCount,
		0	
	);

	VkPresentModeKHR presentModes[presentModeCount] = {};
	vkGetPhysicalDeviceSurfacePresentModesKHR(
		physDev,
		surface,
		&presentModeCount,
		presentModes
	);

	VkPresentModeKHR presentMode = chooseSwapPresentMode(
		presentModes,
		presentModeCount
	);

	VkSurfaceCapabilitiesKHR capabilities;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physDev, surface, &capabilities);

	VkExtent2D extent = chooseSwapExtent(capabilities, width, height);
	extSwapchain->extent = extent;
	extSwapchain->imageCount = capabilities.minImageCount + 1;

	/* Don't request more images than allowed.
	 * A maximum image count of 0 means there is no upper limit. */
	if (
		capabilities.maxImageCount > 0
		&& extSwapchain->imageCount > capabilities.maxImageCount
	) {
		extSwapchain->imageCount = capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR swapchainInfo = {};
	swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchainInfo.surface = surface;
	swapchainInfo.minImageCount = extSwapchain->imageCount;
	swapchainInfo.imageFormat = surfaceFormat.format;
	swapchainInfo.imageColorSpace = surfaceFormat.colorSpace;
	swapchainInfo.imageExtent = extent;
	swapchainInfo.imageArrayLayers = 1;
	swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	QueueFamilyIndices queueFamilies = findQueueFamilies(physDev, surface);

	if (queueFamilies.graphics != queueFamilies.present) {
		swapchainInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		swapchainInfo.queueFamilyIndexCount = queueFamilies.uniqueIndexCount;
		swapchainInfo.pQueueFamilyIndices = queueFamilies.uniqueIndices;
	} else {
		swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	}

	swapchainInfo.preTransform = capabilities.currentTransform;
	swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchainInfo.presentMode = presentMode;
	swapchainInfo.clipped = VK_TRUE;
	swapchainInfo.oldSwapchain = VK_NULL_HANDLE;

	if (vkCreateSwapchainKHR(
		logicalDevice.device,
		&swapchainInfo,
		0,
		&extSwapchain->swapchain
	) != VK_SUCCESS) {
		return -1;
	}

	/**/

	vkGetSwapchainImagesKHR(
		logicalDevice.device,
		extSwapchain->swapchain,
		&extSwapchain->imageCount,
		NULL
	);

	extSwapchain->images = (VkImage*)malloc(
		sizeof(VkImage) * extSwapchain->imageCount
	);

	if (!extSwapchain->images) {
		printf("Failed to allocate memory for swapchain images\n");
		return -1;
	}

	/* Get the images (good) */
	if (vkGetSwapchainImagesKHR(
		logicalDevice.device,
		extSwapchain->swapchain,
		&extSwapchain->imageCount,
		extSwapchain->images
	) != VK_SUCCESS) {
		printf("Failed to get swapchain images\n");
		return -1;
	}

	return 0;
}

void destroyExtendedSwapchain(VkDevice device, ExtendedSwapchain extSwapchain)
{
	vkDestroySwapchainKHR(device, extSwapchain.swapchain, 0);
}

VkExtent2D chooseSwapExtent(
	VkSurfaceCapabilitiesKHR capabilities,
	int width,
	int height
)
{
	VkExtent2D extent = {};

	if (capabilities.currentExtent.width != (uint32_t)-1) {
		return capabilities.currentExtent;
	}

	extent.width = clamp(
		extent.width,
		capabilities.minImageExtent.width,
		capabilities.maxImageExtent.width
	);

	extent.height = clamp(
		extent.height,
		capabilities.minImageExtent.height,
		capabilities.maxImageExtent.height
	);
}

VkPresentModeKHR chooseSwapPresentMode(
	VkPresentModeKHR* presentModes,
	int presentModeCount
)
{
	for (int i = 0; i < presentModeCount; i++) {
		if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
			return presentModes[i];
		}
	}

	return VK_PRESENT_MODE_FIFO_KHR;
}

VkSurfaceFormatKHR chooseSwapSurfaceFormat(
	VkSurfaceFormatKHR* formats,
	int formatCount
)
{
	for (int i = 0; i < formatCount; i++) {   
		if (
			formats[i].format == VK_FORMAT_B8G8R8A8_SRGB
			&& formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
		) {
			return formats[i];
		}
	}

	return formats[0];

}

int createCommandPool(
	VkDevice device,
	VkPhysicalDevice physDev,
	VkSurfaceKHR surface,
	VkCommandPool* pool
)
{
	QueueFamilyIndices queueFamilies = findQueueFamilies(physDev, surface);

	VkCommandPoolCreateInfo cmdPoolInfo = {};
	cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	cmdPoolInfo.queueFamilyIndex = queueFamilies.graphics;

	if (vkCreateCommandPool(device, &cmdPoolInfo, 0, pool) != VK_SUCCESS) {
		printf("Failed to create command pool\n");
		return -1;
	}

	return 0;
}



