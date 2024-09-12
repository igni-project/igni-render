#include "sync.h"
#include <stdio.h> 

/* Sync objects signal when a frame is in use. */
int createFrameSync(FrameSync* sync, VkDevice device)
{
	VkSemaphoreCreateInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	/* image available */
	if (vkCreateSemaphore(device, &semaphoreInfo, 0, &sync->imageAvailable)
		!= VK_SUCCESS
	) {
		printf("Failed to create 'image available' semaphore\n");
		return -1;
	}

	/* render done */
	if (vkCreateSemaphore(device, &semaphoreInfo, 0, &sync->renderDone)
		!= VK_SUCCESS) {
		printf("Failed to create 'render done' semaphore\n");
		return -1;
	}

	/* in flight */
	if (vkCreateFence(device, &fenceInfo, 0, &sync->inFlight) != VK_SUCCESS) {
		printf("Failed to create in flight fence.\n");
		return -1;
	}

	return 0;
}

void destroyFrameSync(VkDevice device, FrameSync sync)
{
	vkDestroySemaphore(device, sync.imageAvailable, 0);
	vkDestroySemaphore(device, sync.renderDone, 0); 
	vkDestroyFence(device, sync.inFlight, 0);
}

