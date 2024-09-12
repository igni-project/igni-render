#include "physdev.h"
#include <stdio.h> 
#include <string.h>

QueueFamilyIndices findQueueFamilies(
	VkPhysicalDevice device,
	VkSurfaceKHR surface
)
{
	QueueFamilyIndices result = {};

	unsigned int nFamilies;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &nFamilies, NULL);

	VkQueueFamilyProperties families[nFamilies];
	vkGetPhysicalDeviceQueueFamilyProperties(device, &nFamilies, families);

	for (int i = 0; i < nFamilies; i++) {
		uint8_t idxHasSomething = 1;

		/* Find the graphics queue */

		if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			result.graphics = i;
			idxHasSomething = 0;
		}

		/* Find the presentation queue */

		VkBool32 presentSupport;
		vkGetPhysicalDeviceSurfaceSupportKHR(
			device,
			i,
			surface,
			&presentSupport
		);

		if (presentSupport) {
			result.present = i;
			idxHasSomething = 0;
		}

		/* Extend list of unique queue indices */
		if (!idxHasSomething) {
			result.uniqueIndices[result.uniqueIndexCount] = i;
			result.uniqueIndexCount++;
		}
	}

	return result;
}

int deviceHasExtensions(
	VkPhysicalDevice device,
	const char** deviceExtensions,
	unsigned int deviceExtensionCount
)
{
	unsigned int nAllExtensions = 0;

	vkEnumerateDeviceExtensionProperties(device, NULL, &nAllExtensions, NULL);

	VkExtensionProperties allExtensions[nAllExtensions];
	vkEnumerateDeviceExtensionProperties(
		device,
		NULL,
		&nAllExtensions,
		allExtensions
	);

	unsigned int hasExtension = 0;

	for (int i = 0; i < deviceExtensionCount; i++) {
		hasExtension = 0;

		for (int ii = 0; ii < nAllExtensions; ii++) {
			if (!strcmp(allExtensions[ii].extensionName, deviceExtensions[i])) {
				hasExtension = 1;
			}
		}

		if (!hasExtension) {
			return 0;
		}
	}

	return 1;
}

VkFormat findDepthFormat(VkPhysicalDevice physicalDevice)
{   
	VkFormat fmtCandidates[] = {
		VK_FORMAT_D32_SFLOAT,
		VK_FORMAT_D32_SFLOAT_S8_UINT,
		VK_FORMAT_D24_UNORM_S8_UINT
	};
	
	return findSupportedFormat(
		fmtCandidates,
		sizeof(fmtCandidates) / sizeof(*fmtCandidates),
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT,
		physicalDevice
	);
}

VkFormat findSupportedFormat(
	VkFormat* candidates,
	int nCandidates,
	VkImageTiling tiling,
	VkFormatFeatureFlags features,
	VkPhysicalDevice physDev
)
{
	for (int i = 0; i < nCandidates; i++) {
		VkFormatProperties properties;
		vkGetPhysicalDeviceFormatProperties(
			physDev,
			candidates[i],
			&properties
		);

		if (
			tiling == VK_IMAGE_TILING_LINEAR
			&& (properties.linearTilingFeatures & features)
		) {
			return candidates[i];
		} else if (
			tiling == VK_IMAGE_TILING_OPTIMAL
			&& (properties.optimalTilingFeatures & features)
		) {
			return candidates[i];
		}
	}

	printf("Failed to find a supported format.\n");
	return VK_FORMAT_UNDEFINED;
}

