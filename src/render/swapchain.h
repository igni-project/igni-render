#ifndef RENDER_SWAPCHAIN_H
#define RENDER_SWAPCHAIN_H 1

#include <vulkan/vulkan.h>

typedef struct
{
	VkDevice device;
	VkQueue graphicsQueue;
	VkQueue presentQueue;
} LogicalDevice;

typedef struct
{
	VkSurfaceCapabilitiesKHR capabilities;
	uint32_t formatCount;
	VkSurfaceFormatKHR* formats;
	uint32_t presentModeCount;
	VkPresentModeKHR* presentModes;
} SwapchainSupportInfo;

typedef struct
{
	VkSwapchainKHR swapchain;
	VkFormat imageFormat;
	VkExtent2D extent;
	uint32_t imageCount;
	VkImage* images;
} ExtendedSwapchain;

SwapchainSupportInfo getSwapchainSupport(
	VkSurfaceKHR surface,
	VkPhysicalDevice device
);

int createExtendedSwapchain(
	int width,
	int height,
	ExtendedSwapchain* extSwapchain,
	LogicalDevice logicalDevice,
	VkPhysicalDevice physDev,
	VkSurfaceKHR surface
);

void destroyExtendedSwapchain(VkDevice device, ExtendedSwapchain extSwapchain);

VkSurfaceFormatKHR chooseSwapSurfaceFormat(
	VkSurfaceFormatKHR* formats,
	int formatCount
);
	
VkPresentModeKHR chooseSwapPresentMode(
	VkPresentModeKHR* presentModes,
	int presentModeCount
);

VkExtent2D chooseSwapExtent(
	VkSurfaceCapabilitiesKHR capabilities,
	int width,
	int height
);

int createCommandPool(
	VkDevice device,
	VkPhysicalDevice physDev,
	VkSurfaceKHR surface,
	VkCommandPool* pool
);


#endif

