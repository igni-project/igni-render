#ifndef RENDER_DISPLAY_H
#define RENDER_DISPLAY_H 1

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_screen.h>
#include <vulkan/vulkan_core.h>

#include "swapchain.h"
#include "misc.h"
#include "scene.h"
#include "pass.h"
#include "sync.h"

/* This program uses GLFW to create windows. */
#if HAVE_LIBGLFW == 1 && WINDOWED 
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#endif

/* Direct Rendering Manager */
#if HAVE_LIBDRM == 1 && !WINDOWED
#include <xf86drm.h>
#include <xf86drmMode.h>
#endif

typedef struct
{
	VkDisplayKHR display;
	VkDisplayModeKHR displayMode;

	unsigned int currentFrame;
	void* window;
	VkInstance instance;
	VkSurfaceKHR surface;

	VkPhysicalDevice physicalDevice;
	LogicalDevice dev;
	ExtendedSwapchain swapchain;

	VkCommandPool cmdPool;
	VkCommandBuffer cmd;

	Texture nulTexture;
	Viewpoint pov;

	RenderPass beauty;
	VkFramebuffer* beautyFb;
	VkImageView* swapchainImageView;
	FrameSync beautySync[MAX_FRAMES_IN_FLIGHT];	
	VkDescriptorSet beautyDescSets[MAX_FRAMES_IN_FLIGHT];
	VkDescriptorPool beautyDescPool;

	RenderPass geom;
	FrameSync geomSync[MAX_FRAMES_IN_FLIGHT];
	VkFramebuffer geomFb[MAX_FRAMES_IN_FLIGHT];

	FramebufferAttachment colour[MAX_FRAMES_IN_FLIGHT];
	FramebufferAttachment depth[MAX_FRAMES_IN_FLIGHT];
	FramebufferAttachment normal[MAX_FRAMES_IN_FLIGHT];
	FramebufferAttachment position[MAX_FRAMES_IN_FLIGHT];
} Display;

int beginRenderPass(
	VkCommandBuffer* cmdBuf,
	RenderPass pass,
	VkFramebuffer fb,
	VkExtent2D ext
);

int iterateScenes(
	VkCommandBuffer* cmdBuf,
	SceneArray scenes,
	int frame,
	VkPipelineLayout pipelineLayout
);

int endRenderPass(VkCommandBuffer* cmdBuf, VkQueue queue, FrameSync sync);
int endRenderPassA(VkCommandBuffer* cmdBuf, VkQueue queue, FrameSync sync);
int renderScenes(Display* display, SceneArray scenes);

int createRenderPasses(Display* display);
int createGeomPass(Display* display);
int createBeautyPass(Display* display);
int recreateRenderPasses(Display* display);
void destroyRenderPasses(Display display);

int recreatePOV(Display* display);

#if HAVE_LIBDRM == 1 && !WINDOWED
int createDisplaySurface(Display* display);
#endif

int createDisplay(Display* display);
void destroyDisplay(Display display);

int recreateSwapchain(Display* display);

int createInstance(
	VkInstance* instance,
	unsigned int instExtCount,
	const char** instExtensions, 
	unsigned int instLayerCount,
	const char* instLayers[]
);

int selectPhysicalDevice(
	VkInstance instance,
	VkPhysicalDevice* physDev,
	const char** deviceExtensions,
	unsigned int deviceExtensionCount
);

int createLogicalDevice(
	LogicalDevice* logicalDevice,
	VkPhysicalDevice physDev,
	VkSurfaceKHR surface,
	const char** deviceExtensions,
	unsigned int deviceExtensionCount,
	unsigned int instLayerCount,
	const char* instLayers[]
);

#endif

