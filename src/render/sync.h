#ifndef RENDER_SYNC_H
#define RENDER_SYNC_H 1

#include <vulkan/vulkan.h>

typedef struct
{
	VkSemaphore imageAvailable;
	VkSemaphore renderDone;
	VkFence inFlight;
} FrameSync;

int createFrameSync(FrameSync* sync, VkDevice device);
void destroyFrameSync(VkDevice device, FrameSync sync);

#endif

