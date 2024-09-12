#ifndef RENDER_PHYSDEV_H
#define RENDER_PHYSDEV_H 1

#include <stdint.h>
#include <vulkan/vulkan.h>

#define MAX_QUEUE_FAMILY_INDICES 2

typedef struct
{
	uint32_t uniqueIndices[MAX_QUEUE_FAMILY_INDICES];
	uint8_t uniqueIndexCount;
	uint32_t graphics;
	uint32_t present;
} QueueFamilyIndices;

QueueFamilyIndices findQueueFamilies(
	VkPhysicalDevice device,
	VkSurfaceKHR surface
);

int deviceHasExtensions(
	VkPhysicalDevice device,
	const char** deviceExtensions,
	unsigned int deviceExtensionCount
);

VkFormat findDepthFormat(VkPhysicalDevice physicalDevice);

VkFormat findSupportedFormat(
	VkFormat* candidates,
	int nCandidates,
	VkImageTiling tiling,
	VkFormatFeatureFlags features,
	VkPhysicalDevice physDev
);

#endif

