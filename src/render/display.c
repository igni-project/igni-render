#include "config.h"
#include "display.h"
#include "physdev.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

int beginRenderPass(
	VkCommandBuffer* cmdBuf,
	RenderPass pass,
	VkFramebuffer fb,
	VkExtent2D ext
)
{
	vkResetCommandBuffer(*cmdBuf, 0);

	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	if (vkBeginCommandBuffer(*cmdBuf, &beginInfo) != VK_SUCCESS) {
		printf("Failed to begin command buffer\n");
		return -1;
	}

	VkClearValue clearValues[4] = { {color: {{0.0f, 0.0f, 0.0f, 1.0f}}}, {depthStencil: {1.0f, 0}}, {color: {{0.0f, 0.0f, 0.0f, 1.0f}}}, {color: {{0.0f, 0.0f, 0.0f, 1.0f}}}
	};

	VkRenderPassBeginInfo passBeginInfo = defRenderPassBeginInfo;
	passBeginInfo.renderPass = pass.pass;
	passBeginInfo.framebuffer = fb;
	passBeginInfo.renderArea.offset.x = 0;
	passBeginInfo.renderArea.offset.y = 0;
	passBeginInfo.renderArea.extent = ext;
	passBeginInfo.clearValueCount = 4;
	passBeginInfo.pClearValues = clearValues;

	vkCmdBeginRenderPass(*cmdBuf, &passBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)ext.width;
	viewport.height = (float)ext.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(*cmdBuf, 0, 1, &viewport);

	VkRect2D scissor = {};
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent = ext;
	vkCmdSetScissor(*cmdBuf, 0, 1, &scissor);

	vkCmdBindPipeline(*cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pass.pipeline);

	return 0;
}

int iterateScenes(
	VkCommandBuffer* cmdBuf,
	SceneArray scenes,
	int frame,
	VkPipelineLayout pipelineLayout
)
{
	VkBuffer vertexBuffers[1];
	VkDeviceSize offsets[] = {0};

	for (int i = 0; i < scenes.sceneCount; i++) {
		for (int j = 0; j < scenes.scenes[i].meshCount; j++) {
			const Mesh mesh = scenes.scenes[i].meshes[j];

			vkCmdBindDescriptorSets(
				*cmdBuf,
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				pipelineLayout,
				0,
				1,
				&mesh.descriptorSets[frame],
				0,
				0	
			);

			vertexBuffers[0] = mesh.vertexBuffer;
			vkCmdBindVertexBuffers(*cmdBuf, 0, 1, vertexBuffers, offsets);

			vkCmdBindIndexBuffer(*cmdBuf, mesh.indexBuffer, 0, mesh.indexType);
			
			vkCmdDrawIndexed(*cmdBuf, mesh.indexCount, 1, 0, 0, 0);
		}
	}

	return 0;
}

int endRenderPass(VkCommandBuffer* cmdBuf, VkQueue queue, FrameSync sync)
{
	vkCmdEndRenderPass(*cmdBuf);

	if (vkEndCommandBuffer(*cmdBuf) != VK_SUCCESS) {
		printf("Failed to record command buffer.\n");
		return -1;
	}

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore waitSemaphores[] = {sync.imageAvailable};

	VkPipelineStageFlags waitStages =
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = &waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = cmdBuf;

	VkSemaphore signalSemaphores[] = {sync.renderDone};

	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	if (vkQueueSubmit(queue, 1, &submitInfo, sync.inFlight) != VK_SUCCESS) {
		printf("Failed to submit draw command buffer\n");
		return -1;
	}

	return 0;
}

int endRenderPassA(VkCommandBuffer* cmdBuf, VkQueue queue, FrameSync sync)
{
	vkCmdEndRenderPass(*cmdBuf);

	if (vkEndCommandBuffer(*cmdBuf) != VK_SUCCESS) {
		printf("Failed to record command buffer.\n");
		return -1;
	}

	VkSemaphore signalSemaphores[] = {sync.renderDone};

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	VkPipelineStageFlags waitStages =
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	submitInfo.pWaitDstStageMask = &waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = cmdBuf;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	if (vkQueueSubmit(queue, 1, &submitInfo, sync.inFlight) != VK_SUCCESS) {
		printf("Failed to submit draw command buffer\n");
		return -1;
	}

	return 0;
}

int renderScenes(Display* display, SceneArray scenes)
{
	/* The window needs regular polling to close when ordered to */
#if HAVE_LIBGLFW == 1 && WINDOWED
	glfwPollEvents();
	if (glfwWindowShouldClose(display->window)) return 1;
#endif

	/* Geometry Pass */

	vkWaitForFences(
		display->dev.device,
		1,
		&display->geomSync[display->currentFrame].inFlight,
		VK_TRUE,
		UINT64_MAX
	);

	vkResetFences(
		display->dev.device,
		1, 
		&display->geomSync[display->currentFrame].inFlight
	);

	if (beginRenderPass(
		&display->geom.commandBuffers[display->currentFrame],
		display->geom,
		display->geomFb[display->currentFrame],
		display->swapchain.extent
	)) {
		printf("Failed to begin render pass\n");
		return -1;
	}

	if (iterateScenes(
		&display->geom.commandBuffers[display->currentFrame],
		scenes,
		display->currentFrame,
		display->geom.pipelineLayout
	)) {
		return -1;
	}

	if (endRenderPassA(
		&display->geom.commandBuffers[display->currentFrame],
		display->dev.graphicsQueue,
		display->geomSync[display->currentFrame]
	)) {
		return -1;
	}

	/* Beauty Pass*/

	vkWaitForFences(
		display->dev.device,
		1,
		&display->beautySync[display->currentFrame].inFlight,
		VK_TRUE,
		UINT64_MAX
	);

	uint32_t imageIndex;
	VkResult result = vkAcquireNextImageKHR(
		display->dev.device,
		display->swapchain.swapchain,
		UINT64_MAX,
		display->beautySync[display->currentFrame].imageAvailable,
		VK_NULL_HANDLE,
		&imageIndex
	);

	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		if (recreateSwapchain(display)) return -1;
		if (recreateRenderPasses(display)) return -1;
		if (recreatePOV(display)) return -1;
		return 0;
	} else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		printf("Failed to acquire swapchain image.\n");
	}

	vkResetFences(
		display->dev.device,
		1, 
		&display->beautySync[display->currentFrame].inFlight
	);

	if (beginRenderPass(
		&display->beauty.commandBuffers[display->currentFrame],
		display->beauty,
		display->beautyFb[imageIndex],
		display->swapchain.extent
	)) {
		printf("Failed to begin render pass\n");
		return -1;
	}

	vkCmdBindDescriptorSets(
		display->beauty.commandBuffers[display->currentFrame],
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		display->beauty.pipelineLayout,
		0,
		1,
		&display->beautyDescSets[display->currentFrame],
		0,
		0	
	);

	vkCmdDraw(
		display->beauty.commandBuffers[display->currentFrame],
		3, 1, 0, 0
	);

	if (endRenderPass(
		&display->beauty.commandBuffers[display->currentFrame],
		display->dev.graphicsQueue,
		display->beautySync[display->currentFrame]
	)) {
		return -1;
	}

	/* Post-render */

	VkSemaphore waitSemaphores[] = {
		display->beautySync[display->currentFrame].renderDone,
		display->geomSync[display->currentFrame].renderDone
	};

	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 2;
	presentInfo.pWaitSemaphores = waitSemaphores;
	presentInfo.pImageIndices = &imageIndex;

	VkSwapchainKHR swapchains[] = {display->swapchain.swapchain};

	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapchains;
	presentInfo.pImageIndices = &imageIndex;

	result = vkQueuePresentKHR(display->dev.presentQueue, &presentInfo);
	
	/* Recreate the swapchain if it needs adjustment, such as resizing. */
	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
		if (recreateSwapchain(display)) return -1;
		if (recreateRenderPasses(display)) return -1;
		if (recreatePOV(display)) return -1;
	} else if (result != VK_SUCCESS) {
		printf("Failed to acquire swapchain image.\n");
		return -1;
	}
	return 0;
}

#if HAVE_LIBDRM == 1 && !WINDOWED
int createDisplaySurface(Display* display) {
	int drmFd;
	drmModeRes* drmRes = 0;

	/* A simple open() call can't determine if a card is usable.
	 * Instead, a loop iterates through each file in /dev/dri 
	 * until the correct card is found. */
	const char driPath[] = "/dev/dri/";

	/* the dirent structure's "d_name" entry has a size limit of
	 * PATH_MAX + 1. */
	const int cardNameLen = strlen(driPath) + PATH_MAX + 1;
	char cardName[cardNameLen] = {};

	DIR* driDir = opendir(driPath);
	struct dirent* cardEnt;

	while ((cardEnt = readdir(driDir))) {
		/* strcat finds the end of a string by reading the
		 * null ternimator, so this should work. */
		strcpy(cardName, driPath);
		strcat(cardName, cardEnt->d_name);

		drmFd = open(cardName, O_RDWR | O_NONBLOCK);

		drmRes = drmModeGetResources(drmFd);
		if (drmRes && drmRes->count_connectors) {
			printf ("DRM found %s\n", cardName);
			break;
		} else {
			printf("DRM not found %s\n", cardName);
			close(drmFd);
		}
	}

	closedir(driDir);

	if (!drmRes) {
		perror("Failed to get DRM resources");
		return -1;
	}

	uint32_t connectorId = 0;

	if (!drmRes->count_connectors) {
		printf("No device connectors available.\n");
		return -1;
	}

	VkDisplayModeParametersKHR displayModeParams = {};

	for (int i = 0; i < drmRes->count_connectors; ++i) {
		printf("connector %i\n", i);
		
		drmModeConnector* conn = 
			drmModeGetConnector(drmFd, drmRes->connectors[i]);

		printf("conn type %i\n", conn->connector_type);

		for (int j = 0; j < conn->count_modes; ++j) {
			printf("mode %i\n", j);
			printf("Refresh rate %i\n", conn->modes[j].vrefresh);
			printf("width %i\n", conn->modes[j].hdisplay);
			printf("height %i\n", conn->modes[j].vdisplay);

			if (!conn->modes[j].vrefresh) continue;
			if (!conn->modes[j].hdisplay) continue;
			if (!conn->modes[j].vdisplay) continue;

			displayModeParams.refreshRate = conn->modes[j].vrefresh * 1000;
			displayModeParams.visibleRegion.width = conn->modes[j].hdisplay;
			displayModeParams.visibleRegion.height = conn->modes[j].vdisplay;

			connectorId = conn->connector_id;
		}

		drmModeFreeConnector(conn);
	}

	drmModeFreeResources(drmRes);

	PFN_vkGetDrmDisplayEXT getDrmDisplay = (PFN_vkGetDrmDisplayEXT)
		vkGetInstanceProcAddr(display->instance, "vkGetDrmDisplayEXT");

	if (!getDrmDisplay) {
		printf("Failed to get get drm display proc addr\n");
		return -1;
	}
	
	VkResult result = getDrmDisplay(
		display->physicalDevice,
		drmFd,
		connectorId,
		&display->display
	);

	close(drmFd);
	
	if (result != VK_SUCCESS) {
		printf("Failed to get DRM display (%i)\n", result);
		return -1;
	}

	printf("displayproc %lp\n", getDrmDisplay);

	VkDisplayModeCreateInfoKHR displayModeInfo = {};
	displayModeInfo.sType = VK_STRUCTURE_TYPE_DISPLAY_MODE_CREATE_INFO_KHR;
	displayModeInfo.parameters = displayModeParams;

	result = vkCreateDisplayModeKHR(
		display->physicalDevice,
		display->display,
		&displayModeInfo,
		0,
		&display->displayMode
	);

	if (result != VK_SUCCESS) {
		printf("Failed to create display mode (%i)\n", result);
		return -1;
	}

	VkDisplaySurfaceCreateInfoKHR surfaceInfo = {};
	surfaceInfo.sType = VK_STRUCTURE_TYPE_DISPLAY_SURFACE_CREATE_INFO_KHR;
	surfaceInfo.displayMode = display->displayMode;
	surfaceInfo.globalAlpha = 1.0f;
	surfaceInfo.alphaMode = VK_DISPLAY_PLANE_ALPHA_OPAQUE_BIT_KHR;	
	surfaceInfo.imageExtent = displayModeParams.visibleRegion;
	surfaceInfo.transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;

	result = vkCreateDisplayPlaneSurfaceKHR(
		display->instance,
		&surfaceInfo,
		0,
		&display->surface
	);

	if (result != VK_SUCCESS) {
		printf("Failed to create display plane surface\n");
		return -1;
	}
	
	return 0;
}

#endif /* HAVE_LIBDRM == 1 && !WINDOWED */

int createDisplay(Display* display)
{

	unsigned int instLayerCount = 1; 
	const char* instLayers[] = {"VK_LAYER_KHRONOS_validation"}; 
	int width, height;

#if WINDOWED
#if HAVE_LIBGLFW == 1 

	unsigned int instExtCount = 0; 
	const char** instExtensions; 
	const unsigned int deviceExtensionCount = 1;
	const char* deviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
	display->window = glfwCreateWindow(800, 600, "Igni Render", 0, 0);
	if (display->window == NULL) {
		printf("Failed to create window\n");
		return -1;
	}

	instExtensions = glfwGetRequiredInstanceExtensions(&instExtCount);
	glfwGetFramebufferSize(display->window, &width, &height);

	if(createInstance(
		&display->instance,
		instExtCount,
		instExtensions,
		instLayerCount,
		instLayers
	)) {
		return -1;
	}

	if (glfwCreateWindowSurface(
		display->instance,
		display->window,
		0,
		&display->surface
	) != VK_SUCCESS) {
		printf("Failed to create window surface.\n");
		return -1;
	}
	
	if (selectPhysicalDevice(
		display->instance,
		&display->physicalDevice,
		deviceExtensions,
		deviceExtensionCount
	)) {
		return -1;
	}

	if (createLogicalDevice(
		&display->dev,
		display->physicalDevice,
		display->surface,
		deviceExtensions,
		deviceExtensionCount,
		instLayerCount,
		instLayers
	)) {
		return -1;
	}

#else /* HAVE_LIBGLFW != 1 */
	
	printf("Windowed mode unavailable. GLFW absent from build.\n");

#endif /* HAVE_LIBGLFW == 1 */
#else /* !WINDOWED */
#if HAVE_LIBDRM == 1

	const unsigned int instExtCount = 4;
	const char* instExtensions[] = {
		"VK_KHR_surface",
		"VK_KHR_display",
		VK_EXT_DIRECT_MODE_DISPLAY_EXTENSION_NAME,
		VK_EXT_ACQUIRE_DRM_DISPLAY_EXTENSION_NAME,
		"VK_KHR_get_physical_device_properties2",
		"VK_KHR_external_memory_capabilities"
	};

	const unsigned int deviceExtensionCount = 1;
	const char* deviceExtensions[] = {

		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

	if(createInstance(
		&display->instance,
		instExtCount,
		instExtensions,
		instLayerCount,
		instLayers
	)) {
		printf("Vulkan or GPU drivers may not be installed correctly.\n");
		return -1;
	}

	if (selectPhysicalDevice(
		display->instance,
		&display->physicalDevice,
		deviceExtensions,
		deviceExtensionCount
	)) {
		return -1;
	}

	printf("Create FUllScreen Display\n");

	if (createDisplaySurface(display)) {
		return -1;
	}

	if (createLogicalDevice(
		&display->dev,
		display->physicalDevice,
		display->surface,
		deviceExtensions,
		deviceExtensionCount,
		instLayerCount,
		instLayers
	)) {
		return -1;
	}

#else  /* HAVE_LIBDRM != 1 */

	printf("Fullscreen unavailable libdrm absent from build.\n");

#endif /* HAVE_LIBDRM == 1 */
#endif /* WINDOWED */


	if (createExtendedSwapchain(
		width,
		height,
		&display->swapchain,
		display->dev,
		display->physicalDevice,
		display->surface
	)) {
		printf("Failed to create extended swapchain\n");
		return -1;
	}

	if (createViewpoint(
		&display->pov,
		display->dev.device,
		display->physicalDevice
	)) {
		return -1;
	}

	/* Default viewpoint placement */
	ViewpointUniforms ubo = {};
	
	Vec3 eye = {0.0f}, centre = {0.0f}, up = {0.0f};
	eye.x = 2.0f;
	eye.y = 2.0f;
	eye.z = 2.0f;
	up.z = 1.0f;

	display->pov.fov = 5.0f / 57.296f;
	ubo.view = matLook(eye, centre, up);
	ubo.proj = matPersp(
		display->pov.fov,
		(float)display->swapchain.extent.width
		/ (float)display->swapchain.extent.height,
		0.1f,
	   	10.0f
	);

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		memcpy(display->pov.uboMapped[i], &ubo, sizeof(ViewpointUniforms));
	}

	if (createCommandPool(
		display->dev.device,
		display->physicalDevice,
		display->surface,
		&display->cmdPool
	)) {
		return -1;
	}

	if (createCommandBuffers(
		display->dev.device,
		display->cmdPool,
		&display->cmd
	)) {
		return -1;
	}

	/* Null Texture - a 1x1 magenta pixel */

	display->nulTexture.width = 1;
	display->nulTexture.height = 1;

	if (createTexture(
		&display->nulTexture, 
		display->dev.device,
		display->physicalDevice
	)) {
		return -1;
	}

	unsigned char magenta[4] = {255, 0, 255, 255};

	if (writeTexture(
		&display->nulTexture,
		magenta,
		display->dev.device,
		display->physicalDevice,
		display->cmd,
		display->dev.graphicsQueue
	)) {
		return -1;
	}

	return 0;
}

int createRenderPasses(Display* display)
{
	if (createGeomPass(display)) {
		printf("Failed to create geometry pass.\n");
		return -1;
	}

	if (createBeautyPass(display)) {
		printf("Failed to create beauty pass.\n");
		return -1;
	}

	return 0;
}

int createGeomPass(Display* display)
{
	/* Sync */
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		if (createFrameSync(&display->geomSync[i], display->dev.device))  {
			return -1;
		}
	}

	/* Framebuffer Attachments */
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		/* Depth Buffer */
		if (createFramebufferAttachment(
			&display->depth[i],
			findDepthFormat(display->physicalDevice),
			display->dev.device,
			display->physicalDevice,
			display->swapchain.extent,
			VK_IMAGE_ASPECT_DEPTH_BIT,
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
		)) {
			return -1;
		}

		/* Colour */
		if (createFramebufferAttachment(
			&display->colour[i],
			VK_FORMAT_R8G8B8A8_UNORM,
			display->dev.device,
			display->physicalDevice,
			display->swapchain.extent,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
			VK_IMAGE_USAGE_SAMPLED_BIT
		)) {
			return -1;
		}

		/* Normal */
		if (createFramebufferAttachment(
			&display->normal[i],
			VK_FORMAT_R8G8B8A8_UNORM,
			display->dev.device,
			display->physicalDevice,
			display->swapchain.extent,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
			VK_IMAGE_USAGE_SAMPLED_BIT
		)) {
			return -1;
		}

		/* Position */
		if (createFramebufferAttachment(
			&display->position[i],
			VK_FORMAT_R32G32B32A32_SFLOAT,
			display->dev.device,
			display->physicalDevice,
			display->swapchain.extent,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
			VK_IMAGE_USAGE_SAMPLED_BIT
		)) {
			return -1;
		}
	}


	/* Render Pass */

	VkAttachmentDescription colourAttachment = defAttachmentDescription;
	colourAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	colourAttachment.format = VK_FORMAT_R8G8B8A8_UNORM;

	VkAttachmentReference colourAttachmentRef = {};
	colourAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	colourAttachmentRef.attachment = 0;

	VkAttachmentDescription depthAttachment = defAttachmentDescription;
	depthAttachment.finalLayout = 
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	depthAttachment.format = findDepthFormat(display->physicalDevice);

	VkAttachmentReference depthAttachmentRef = {};
	depthAttachmentRef.layout = 
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	depthAttachmentRef.attachment = 1;

	VkAttachmentDescription normalAttachment = defAttachmentDescription;
	normalAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	normalAttachment.format = VK_FORMAT_R8G8B8A8_UNORM;

	VkAttachmentReference normalAttachmentRef = {};
	normalAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	normalAttachmentRef.attachment = 2;

	VkAttachmentDescription posAttachment = defAttachmentDescription;
	posAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	posAttachment.format = VK_FORMAT_R32G32B32A32_SFLOAT;

	VkAttachmentReference posAttachmentRef = {};
	posAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	posAttachmentRef.attachment = 3;

	const int attachmentCount = 4;
	VkAttachmentDescription attachments[] =  {
		colourAttachment, 
		depthAttachment,
		normalAttachment,
	   	posAttachment
	};
	VkAttachmentReference attachmentRefs[] =  {
		colourAttachmentRef,
		depthAttachmentRef,
		normalAttachmentRef,
		posAttachmentRef
	};
	const int colourAttachmentCount = 3;
	VkAttachmentReference colourAttachmentRefs[] = {
		colourAttachmentRef,
		normalAttachmentRef, 
		posAttachmentRef
	};

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = colourAttachmentCount;
	subpass.pColorAttachments = colourAttachmentRefs;
	subpass.pDepthStencilAttachment = &depthAttachmentRef;

	VkSubpassDependency dependency = {};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask =
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
		| VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask =
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
		| VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.dstAccessMask =
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
		| VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo passInfo = defRenderPassCreateInfo;
	passInfo.attachmentCount = attachmentCount;
	passInfo.pAttachments = attachments;
	passInfo.subpassCount = 1;
	passInfo.pSubpasses = &subpass;
	passInfo.dependencyCount = 1;
	passInfo.pDependencies = &dependency;

	if (vkCreateRenderPass(
		display->dev.device,
		&passInfo,
		0,
		&display->geom.pass
	) != VK_SUCCESS) {
		printf("Failed to create beauty render pass\n");
		return -1;
	}

	/* Framebuffers */

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {

		VkImageView imgViewAttachments[] = {
			display->colour[i].view, 
			display->depth[i].view,
			display->normal[i].view,
			display->position[i].view
		};

		VkFramebufferCreateInfo fbInfo = {};
		fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fbInfo.renderPass = display->geom.pass;
		fbInfo.attachmentCount = 4;
		fbInfo.pAttachments = imgViewAttachments;
		fbInfo.width = display->swapchain.extent.width;
		fbInfo.height = display->swapchain.extent.height;
		fbInfo.layers = 1;

		if (vkCreateFramebuffer(
			display->dev.device,
			&fbInfo,
			0,
			&display->geomFb[i]
		) != VK_SUCCESS) {
			printf("Failed to create framebuffer\n");
			return -1;
		}
	}

	/* Command Pool */

	QueueFamilyIndices queueFamilies = 
		findQueueFamilies(display->physicalDevice, display->surface);

	VkCommandPoolCreateInfo commandPoolInfo = {};
	commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	commandPoolInfo.queueFamilyIndex = queueFamilies.graphics;
	
	if (vkCreateCommandPool(
		display->dev.device,
		&commandPoolInfo,
		0,
		&display->geom.commandPool
	) != VK_SUCCESS) {
		printf("Failed to create command pool\n");
		return -1;
	}
	
	/* Command Buffers */

	VkCommandBufferAllocateInfo allocInfo = defCommandBufferAllocateInfo;
	allocInfo.commandPool = display->geom.commandPool;

	display->geom.commandBuffers = (VkCommandBuffer*)
		malloc(sizeof(VkCommandBuffer) * MAX_FRAMES_IN_FLIGHT);

	if (vkAllocateCommandBuffers(
		display->dev.device,
		&allocInfo,
		display->geom.commandBuffers
	) != VK_SUCCESS) {
		printf("Failed to create command buffers\n");
		return -1;
	}

	/* Descriptor Set Layout */

	VkDescriptorSetLayoutBinding uboLayoutBinding = {};
	uboLayoutBinding.binding = 0;
	uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboLayoutBinding.descriptorCount = 1;
	uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkDescriptorSetLayoutBinding colourSamplerLayoutBinding = {};
	colourSamplerLayoutBinding.binding = 1;
	colourSamplerLayoutBinding.descriptorCount = 1;
	colourSamplerLayoutBinding.descriptorType =
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	colourSamplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutBinding objectUniformLayoutBinding = {};
	objectUniformLayoutBinding.binding = 2;
	objectUniformLayoutBinding.descriptorType =
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	objectUniformLayoutBinding.descriptorCount = 1;
	objectUniformLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkDescriptorSetLayoutBinding descSetLayoutBindings[] = {
		uboLayoutBinding,
		colourSamplerLayoutBinding,
		objectUniformLayoutBinding
	};

	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 3;
	layoutInfo.pBindings = descSetLayoutBindings;

	if (vkCreateDescriptorSetLayout(
		display->dev.device,
		&layoutInfo,
		0,
		&display->geom.descSetLayout
	) != VK_SUCCESS) {
		printf("Failed to create descriptor set layout\n");
		return -1;
	}

	/* Descriptor Pool [none] */
	/* Descriptor Sets [none] */

	/* Pipeline Layout */

	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &display->geom.descSetLayout;

	if (vkCreatePipelineLayout(
		display->dev.device,
		&pipelineLayoutInfo,
		0,
		&display->geom.pipelineLayout
	) != VK_SUCCESS) {
		printf("Failed to create pipeline layout\n");
		return -1;
	}

	/* Pipeline */

	const int shaderStageCount = 2;

	const char* dataDir = getenv("IGNI_RENDER_DATA_DIR");

	/* +1 to account for the null terminator */
	int dataDirLen = strlen(dataDir) + 1;

	/* 20 chars is enough for the base filename */
	char* fragPath = malloc(dataDirLen + 20);
	memcpy(fragPath, dataDir, dataDirLen);
	strcat(fragPath, "/frag.spv");

	char* vertPath = malloc(dataDirLen + 20);
	memcpy(vertPath, dataDir, dataDirLen);
	strcat(vertPath, "/vert.spv");

	VkShaderModule basicFrag;

	if (loadShaderModule(
		display->dev.device,
		&basicFrag,
		fragPath
	)) {
		 return -1;
	}

	VkPipelineShaderStageCreateInfo fragShaderStageInfo = {}; 
	fragShaderStageInfo.sType =
	   VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.module = basicFrag;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.pName = "main";

	VkShaderModule basicVert;

	if (loadShaderModule(
		display->dev.device,
		&basicVert,
		vertPath
	)) {
		return -1;
	}

	VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
	vertShaderStageInfo.sType =
	   VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.module = basicVert;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.pName = "main";

	free(fragPath);
	free(vertPath);

	VkPipelineShaderStageCreateInfo shaderStages[] = {
		vertShaderStageInfo,
		fragShaderStageInfo
	};

	VkDynamicState dynamicStates[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};

	VkPipelineDynamicStateCreateInfo dynamicInfo = {};
	dynamicInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicInfo.dynamicStateCount = shaderStageCount;
	dynamicInfo.pDynamicStates = dynamicStates;

	/* Vertex Input Info */

	const int vertexAttributeCount = 3;
	VkVertexInputAttributeDescription vertexAttributeDescriptions[] = { {
			location: 0,
			binding: 0,
			format: VK_FORMAT_R32G32B32_SFLOAT,
			offset: offsetof(Vertex, pos)
		},   {
			location: 1,
			binding: 0,
			format: VK_FORMAT_R32G32B32_SFLOAT,
			offset: offsetof(Vertex, normal)
		}, {
			location: 2,
			binding: 0,
			format: VK_FORMAT_R32G32_SFLOAT,
			offset: offsetof(Vertex, texCoord)
		}
	};

	VkVertexInputBindingDescription vertexBindingDescription = {   
		binding: 0,
		stride: sizeof(Vertex),
		inputRate: VK_VERTEX_INPUT_RATE_VERTEX
	};

	VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
	vertexInputInfo.sType =
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.pVertexBindingDescriptions =
		&vertexBindingDescription;
	vertexInputInfo.vertexAttributeDescriptionCount =
		vertexAttributeCount;
	vertexInputInfo.pVertexAttributeDescriptions =
		vertexAttributeDescriptions;

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo = {};
	inputAssemblyInfo.sType =
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = display->swapchain.extent.width;
	viewport.height = display->swapchain.extent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	/* Scissor A.K.A Crop Rectangle */

	VkRect2D scissor = {};
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent = display->swapchain.extent;

	VkPipelineViewportStateCreateInfo viewportInfo = {};
	viewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportInfo.viewportCount = 1;
	viewportInfo.pViewports = &viewport;
	viewportInfo.scissorCount = 1;
	viewportInfo.pScissors = &scissor;

	VkPipelineRasterizationStateCreateInfo rasteriserInfo = {};
	rasteriserInfo.sType =
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasteriserInfo.depthClampEnable = VK_FALSE;
	rasteriserInfo.rasterizerDiscardEnable = VK_FALSE;
	rasteriserInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasteriserInfo.lineWidth = 1.0f;
	rasteriserInfo.cullMode = VK_CULL_MODE_NONE;
	rasteriserInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasteriserInfo.depthBiasEnable = VK_FALSE;

	VkPipelineMultisampleStateCreateInfo multisampleInfo = {};
	multisampleInfo.sType =
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleInfo.sampleShadingEnable = VK_FALSE;
	multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampleInfo.minSampleShading = 1.0f;

	/* Colour Blend Attachment
	 * The fragment shader's colours are applied to the framebuffer through
	 * blending. A colour recieved from the fragment shader is blended
	 * with the colour already on the framebuffer. */

	VkPipelineColorBlendAttachmentState colourBlendAttachment = {};
	colourBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT
		| VK_COLOR_COMPONENT_G_BIT
		| VK_COLOR_COMPONENT_B_BIT
		| VK_COLOR_COMPONENT_A_BIT;

	/* In this case, alpha blending is used.*/

	colourBlendAttachment.blendEnable = VK_TRUE;
	colourBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colourBlendAttachment.dstColorBlendFactor =
		VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colourBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colourBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colourBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colourBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	/* they're all colour channels so this will do for now */
	VkPipelineColorBlendAttachmentState colourBlendAttachments[3] =  {
		colourBlendAttachment, colourBlendAttachment, colourBlendAttachment
	};

	VkPipelineColorBlendStateCreateInfo colourBlendInfo = {};
	colourBlendInfo.sType =
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colourBlendInfo.logicOpEnable = VK_FALSE;
	colourBlendInfo.attachmentCount = 3;
	colourBlendInfo.pAttachments = colourBlendAttachments;

	VkPipelineDepthStencilStateCreateInfo depthStencil = {};
	depthStencil.sType =
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_TRUE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.stencilTestEnable = VK_FALSE;

	VkGraphicsPipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = shaderStageCount;
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssemblyInfo;
	pipelineInfo.pViewportState = &viewportInfo;
	pipelineInfo.pRasterizationState = &rasteriserInfo;
	pipelineInfo.pMultisampleState = &multisampleInfo;
	pipelineInfo.pColorBlendState = &colourBlendInfo;
	pipelineInfo.pDepthStencilState = &depthStencil;
	pipelineInfo.pDynamicState = &dynamicInfo;
	pipelineInfo.layout = display->geom.pipelineLayout;
	pipelineInfo.renderPass = display->geom.pass;
	pipelineInfo.subpass = 0;

	VkResult pipelineResult = vkCreateGraphicsPipelines(
		display->dev.device,
		VK_NULL_HANDLE,
		1,
		&pipelineInfo,
		0,
		&display->geom.pipeline
	);

	vkDestroyShaderModule(display->dev.device, basicVert, 0);   
	vkDestroyShaderModule(display->dev.device, basicFrag, 0); 

	if (pipelineResult != VK_SUCCESS) {
		printf("Failed to create graphics pipeline\n");
		return -1;
	}

	return 0;
}

int createBeautyPass(Display* display)
{
	/* Image View */
	
	display->swapchainImageView = (VkImageView*)malloc(
		display->swapchain.imageCount * sizeof(VkImageView)
	);

	VkImageViewCreateInfo gpImageView = defImageViewCreateInfo;
	for (int i = 0; i < display->swapchain.imageCount; i++) {
		gpImageView.image = display->swapchain.images[i];
		gpImageView.format = display->swapchain.imageFormat; 

		if (vkCreateImageView(
			display->dev.device,
			&gpImageView,
			0,
			&display->swapchainImageView[i]
		) != VK_SUCCESS) {
			printf("Failed to create colour image view\n");
			return -1;
		}
	}

	/* Sync */

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		if (createFrameSync(&display->beautySync[i], display->dev.device))  {
			return -1;
		}
	}

	/* Base render pass */

	VkAttachmentDescription beautyAttachment = defAttachmentDescription;
	beautyAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	beautyAttachment.format = display->swapchain.imageFormat;

	VkAttachmentReference beautyAttachmentRef;
	beautyAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	beautyAttachmentRef.attachment = 0;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &beautyAttachmentRef;

	VkSubpassDependency dependency = {};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask =
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
		| VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask =
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
		| VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.dstAccessMask =
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
		| VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo passInfo = defRenderPassCreateInfo;
	passInfo.attachmentCount = 1;
	passInfo.pAttachments = &beautyAttachment;
	passInfo.subpassCount = 1;
	passInfo.pSubpasses = &subpass;
	passInfo.dependencyCount = 1;
	passInfo.pDependencies = &dependency;

	if (vkCreateRenderPass(
		display->dev.device,
		&passInfo,
		0,
		&display->beauty.pass
	) != VK_SUCCESS) {
		printf("Failed to create beauty render pass\n");
		return -1;
	}

	/* Command Pool */
	QueueFamilyIndices queueFamilies = 
		findQueueFamilies(display->physicalDevice, display->surface);

	VkCommandPoolCreateInfo commandPoolInfo = defCommandPoolCreateInfo;
	commandPoolInfo.queueFamilyIndex = queueFamilies.graphics;
	
	if (vkCreateCommandPool(
		display->dev.device,
		&commandPoolInfo,
		0,
		&display->beauty.commandPool
	) != VK_SUCCESS) {
		printf("Failed to create command pool\n");
		return -1;
	}

	/* Command Buffers */

	VkCommandBufferAllocateInfo allocInfo = defCommandBufferAllocateInfo;
	allocInfo.commandPool = display->beauty.commandPool;

	display->beauty.commandBuffers = (VkCommandBuffer*)
		malloc(sizeof(VkCommandBuffer) * MAX_FRAMES_IN_FLIGHT);

	if (vkAllocateCommandBuffers(
		display->dev.device,
		&allocInfo,
		display->beauty.commandBuffers
	) != VK_SUCCESS) {
		printf("Failed to create command buffers\n");
		return -1;
	}

	/* Descriptor Set Layout */

	VkDescriptorSetLayoutBinding colourSamplerLayoutBinding = {};
	colourSamplerLayoutBinding.binding = 0;
	colourSamplerLayoutBinding.descriptorCount = 1;
	colourSamplerLayoutBinding.descriptorType =
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	colourSamplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutBinding normalSamplerLayoutBinding = {};
	normalSamplerLayoutBinding.binding = 1;
	normalSamplerLayoutBinding.descriptorCount = 1;
	normalSamplerLayoutBinding.descriptorType =
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	normalSamplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutBinding positionSamplerLayoutBinding = {};
	positionSamplerLayoutBinding.binding = 2;
	positionSamplerLayoutBinding.descriptorCount = 1;
	positionSamplerLayoutBinding.descriptorType =
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	positionSamplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutBinding descSetLayoutBindings[] = {
		colourSamplerLayoutBinding,
		normalSamplerLayoutBinding,
		positionSamplerLayoutBinding
	};

	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 3;
	layoutInfo.pBindings = descSetLayoutBindings;

	if (vkCreateDescriptorSetLayout(
		display->dev.device,
		&layoutInfo,
		0,
		&display->beauty.descSetLayout
	) != VK_SUCCESS) {
		printf("Failed to create descriptor set layout\n");
		return -1;
	}

	/* Descriptor Pool */

	VkDescriptorPoolSize poolSizes[1] = {};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[0].descriptorCount = MAX_FRAMES_IN_FLIGHT * 3;

	VkDescriptorPoolCreateInfo descPoolInfo = {};
	descPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descPoolInfo.poolSizeCount = 1;
	descPoolInfo.pPoolSizes = poolSizes;
	descPoolInfo.maxSets = MAX_FRAMES_IN_FLIGHT;

	if (vkCreateDescriptorPool(
		display->dev.device,
		&descPoolInfo,
		0,
		&display->beautyDescPool
	) != VK_SUCCESS) {
		printf("Failed to create descriptor pool.\n");
		return -1;
	}

	/* Descriptor Sets */

	VkDescriptorSetLayout descSetLayouts[MAX_FRAMES_IN_FLIGHT];
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		descSetLayouts[i] = display->beauty.descSetLayout;
	}

	VkDescriptorSetAllocateInfo gAllocInfo = {};
	gAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	gAllocInfo.descriptorPool = display->beautyDescPool;
	gAllocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
	gAllocInfo.pSetLayouts = descSetLayouts;

	if (vkAllocateDescriptorSets(
		display->dev.device,
		&gAllocInfo,
		display->beautyDescSets
	) != VK_SUCCESS) {
		printf("Failed to allocate descriptor sets.\n");
		return -1;
	}

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		/* Colour */
		VkDescriptorImageInfo colourImageInfo = {};
		colourImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		colourImageInfo.imageView = display->colour[i].view;
		colourImageInfo.sampler = display->colour[i].sampler;

		VkWriteDescriptorSet colourWriteDesc = {};
		colourWriteDesc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		colourWriteDesc.dstSet = display->beautyDescSets[i];
		colourWriteDesc.dstBinding = 0;
		colourWriteDesc.dstArrayElement = 0;
		colourWriteDesc.descriptorType =
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		colourWriteDesc.descriptorCount = 1;
		colourWriteDesc.pImageInfo = &colourImageInfo;

		vkUpdateDescriptorSets(
			display->dev.device,
			1,
			&colourWriteDesc,
			0,
			0
		);

		/* Normal */
		VkDescriptorImageInfo normalImageInfo = {};
		normalImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		normalImageInfo.imageView = display->normal[i].view;
		normalImageInfo.sampler = display->normal[i].sampler;

		VkWriteDescriptorSet normalWriteDesc = {};
		normalWriteDesc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		normalWriteDesc.dstSet = display->beautyDescSets[i];
		normalWriteDesc.dstBinding = 1;
		normalWriteDesc.dstArrayElement = 0;
		normalWriteDesc.descriptorType =
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		normalWriteDesc.descriptorCount = 1;
		normalWriteDesc.pImageInfo = &normalImageInfo;

		vkUpdateDescriptorSets(
			display->dev.device,
			1,
			&normalWriteDesc,
			0,
			0
		);

		/* Position */
		VkDescriptorImageInfo posImageInfo = {};
		posImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		posImageInfo.imageView = display->position[i].view;
		posImageInfo.sampler = display->position[i].sampler;

		VkWriteDescriptorSet posWriteDesc = {};
		posWriteDesc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		posWriteDesc.dstSet = display->beautyDescSets[i];
		posWriteDesc.dstBinding = 2;
		posWriteDesc.dstArrayElement = 0;
		posWriteDesc.descriptorType =
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		posWriteDesc.descriptorCount = 1;
		posWriteDesc.pImageInfo = &posImageInfo;

		vkUpdateDescriptorSets(
			display->dev.device,
			1,
			&posWriteDesc,
			0,
			0
		);
	}

	/* Pipeline Layout */

	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &display->beauty.descSetLayout;

	if (vkCreatePipelineLayout(
		display->dev.device,
		&pipelineLayoutInfo,
		0,
		&display->beauty.pipelineLayout
	) != VK_SUCCESS) {
		printf("Failed to create pipeline layout\n");
		return -1;
	}

	/* Pipeline */

	PipelineCreateInfos pipelineInfos = {};
	createPipelineInfos(&pipelineInfos);

	const int shaderStageCount = 2;

	VkShaderModule basicFrag;

	const char* dataDir = getenv("IGNI_RENDER_DATA_DIR");

	/* +1 to account for the null terminator */
	int dataDirLen = strlen(dataDir) + 1;

	/* 20 chars is enough for the base filename */
	char* beautyFragPath = malloc(dataDirLen + 20);
	memcpy(beautyFragPath, dataDir, dataDirLen);
	strcat(beautyFragPath, "/beautyfrag.spv");
	
	char* beautyVertPath = malloc(dataDirLen + 20);
	memcpy(beautyVertPath, dataDir, dataDirLen);
	strcat(beautyVertPath, "/beautyvert.spv");

	if (loadShaderModule(
		display->dev.device,
		&basicFrag,
		beautyFragPath
	)) {
		 return -1;
	}

	VkPipelineShaderStageCreateInfo fragShaderStageInfo = {}; 
	fragShaderStageInfo.sType =
	   VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.module = basicFrag;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.pName = "main";

	VkShaderModule basicVert;

	if (loadShaderModule(
		display->dev.device,
		&basicVert,
		beautyVertPath
	)) {
		return -1;
	}

	VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
	vertShaderStageInfo.sType =
	   VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.module = basicVert;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.pName = "main";

	free(beautyFragPath);
	free(beautyVertPath);

	VkPipelineShaderStageCreateInfo shaderStages[] = {
		vertShaderStageInfo,
		fragShaderStageInfo
	};

	VkDynamicState dynamicStates[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};

	pipelineInfos.dynamicState.dynamicStateCount = shaderStageCount;
	pipelineInfos.dynamicState.pDynamicStates = dynamicStates;

	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = display->swapchain.extent.width;
	viewport.height = display->swapchain.extent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = {};
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent = display->swapchain.extent;

	pipelineInfos.viewportState.viewportCount = 1;
	pipelineInfos.viewportState.pViewports = &viewport;
	pipelineInfos.viewportState.scissorCount = 1;
	pipelineInfos.viewportState.pScissors = &scissor;

	/* Colour Blend Attachment
	 * The fragment shader's colours are applied to the framebuffer through
	 * blending. A colour recieved from the fragment shader is blended
	 * with the colour already on the framebuffer. */

	VkPipelineColorBlendAttachmentState colourBlendAttachment = {};
	colourBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT
		| VK_COLOR_COMPONENT_G_BIT
		| VK_COLOR_COMPONENT_B_BIT
		| VK_COLOR_COMPONENT_A_BIT;

	/* In this case, alpha blending is used.*/

	colourBlendAttachment.blendEnable = VK_TRUE;
	colourBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colourBlendAttachment.dstColorBlendFactor =
		VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colourBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colourBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colourBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colourBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	pipelineInfos.colourBlendState.attachmentCount = 1;
	pipelineInfos.colourBlendState.pAttachments = &colourBlendAttachment;

	pipelineInfos.pipeline.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfos.pipeline.stageCount = shaderStageCount;
	pipelineInfos.pipeline.pStages = shaderStages;
	pipelineInfos.pipeline.layout = display->beauty.pipelineLayout;
	pipelineInfos.pipeline.renderPass = display->beauty.pass;
	pipelineInfos.pipeline.subpass = 0;

	VkResult pipelineResult = vkCreateGraphicsPipelines(
		display->dev.device,
		VK_NULL_HANDLE,
		1,
		&pipelineInfos.pipeline,
		0,
		&display->beauty.pipeline
	);

	vkDestroyShaderModule(display->dev.device, basicVert, 0);   
	vkDestroyShaderModule(display->dev.device, basicFrag, 0); 

	if (pipelineResult != VK_SUCCESS) {
		printf("Failed to create graphics pipeline\n");
		return -1;
	}

	/* Framebuffer */

	display->beautyFb = (VkFramebuffer*)malloc(
		display->swapchain.imageCount * sizeof(VkFramebuffer)
	);

	for (int i = 0; i < display->swapchain.imageCount; i++) {
		VkFramebufferCreateInfo fbInfo = {};
		fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fbInfo.renderPass = display->beauty.pass;
		fbInfo.attachmentCount = 1;
		fbInfo.pAttachments = &display->swapchainImageView[i];
		fbInfo.width = display->swapchain.extent.width;
		fbInfo.height = display->swapchain.extent.height;
		fbInfo.layers = 1;

		if (vkCreateFramebuffer(
			display->dev.device,
			&fbInfo,
			0,
			&display->beautyFb[i]
		) != VK_SUCCESS) {
			printf("Failed to create framebuffer\n");
			return -1;
		}
	}


	return 0;
}

void destroyRenderPasses(Display display)
{
	destroyRenderPass(display.dev.device, display.geom);
	destroyRenderPass(display.dev.device, display.beauty);
	vkDestroyDescriptorPool(display.dev.device, display.beautyDescPool, 0);

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		vkDestroyFramebuffer(display.dev.device, display.geomFb[i], 0);
		destroyFramebufferAttachment(display.dev.device, display.depth[i]);
		destroyFramebufferAttachment(display.dev.device, display.colour[i]);
		destroyFramebufferAttachment(display.dev.device, display.normal[i]);
		destroyFramebufferAttachment(display.dev.device, display.position[i]);
		destroyFrameSync(display.dev.device, display.geomSync[i]);
		destroyFrameSync(display.dev.device, display.beautySync[i]);
	}

	for (int i = 0; i < display.swapchain.imageCount; i++) {
		vkDestroyFramebuffer(display.dev.device, display.beautyFb[i], 0);
		vkDestroyImageView(display.dev.device, display.swapchainImageView[i], 0);
	}

	free(display.beautyFb);
	free(display.swapchainImageView);
}

void destroyDisplay(Display display)
{
	vkFreeCommandBuffers(display.dev.device, display.cmdPool, 1, &display.cmd);
	vkDestroyCommandPool(display.dev.device, display.cmdPool, 0);

	destroyExtendedSwapchain(display.dev.device, display.swapchain);
	destroyViewpoint(display.dev.device, display.pov);
	destroyTexture(display.dev.device, display.nulTexture);

	vkDestroyDevice(display.dev.device, 0);
	vkDestroySurfaceKHR(display.instance, display.surface, 0);

#if HAVE_LIBGLFW == 1 && WINDOWED
	glfwDestroyWindow(display.window);
	glfwTerminate();
#else

	PFN_vkReleaseDisplayEXT releaseDisplay = (PFN_vkReleaseDisplayEXT)vkGetInstanceProcAddr(display.instance, "vkReleaseDisplayEXT");

	if (releaseDisplay)  {
		releaseDisplay(display.physicalDevice, display.display);
	}

#endif

	vkDestroyInstance(display.instance, 0);
}

int recreateSwapchain(Display* display)
{
	int width = 0;
	int height = 0;

	/* All surfaces have their own ways of getting width and height. */
#if HAVE_LIBGLFW == 1 && WINDOWED
	while (!width || !height) {
		glfwGetFramebufferSize(display->window, &width, &height);
		glfwWaitEvents();
	}
#endif

	vkDeviceWaitIdle(display->dev.device);

	destroyExtendedSwapchain(display->dev.device, display->swapchain);

	if (createExtendedSwapchain(
		width,
		height,
		&display->swapchain,
		display->dev,
		display->physicalDevice,
		display->surface
	)) {
		printf("Failed to create swapchain\n");
		return -1;
	}


	return 0;
}

int recreateRenderPasses(Display* display)
{
	vkDeviceWaitIdle(display->dev.device);
	destroyRenderPasses(*display);

	if (createRenderPasses(display)) return -1;

	return 0;
}

int recreatePOV(Display* display)
{

	ViewpointUniforms ubo;

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		ubo = *(ViewpointUniforms*)display->pov.uboMapped[i];
		ubo.proj = matPersp(
			display->pov.fov,
			(float)display->swapchain.extent.width
			/ (float)display->swapchain.extent.height,
			0.1f,
			10.0f
		);

		*(ViewpointUniforms*)display->pov.uboMapped[i] = ubo;
	}
	return 0;
}


int createInstance(
	VkInstance* instance,
	unsigned int instExtCount,
	const char** instExtensions, 
	unsigned int instLayerCount,
	const char* instLayers[]
)
{
	VkApplicationInfo appInfo = {0};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = PACKAGE_NAME;
	appInfo.applicationVersion = VK_MAKE_VERSION(MAJ_V, MIN_V, PATCH_V);
	appInfo.pEngineName = "None";
	appInfo.engineVersion = VK_MAKE_VERSION(MAJ_V, MIN_V, PATCH_V);
	appInfo.apiVersion = VK_API_VERSION_1_0;

	VkInstanceCreateInfo instanceInfo = {0};
	instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceInfo.pApplicationInfo = &appInfo;
	instanceInfo.enabledExtensionCount = instExtCount;
	instanceInfo.ppEnabledExtensionNames = instExtensions;

	instanceInfo.enabledLayerCount = getAvailableInstanceLayers(
		(const char***)&instanceInfo.ppEnabledLayerNames,
		instLayers,
		instLayerCount
	);

	VkResult result = vkCreateInstance(&instanceInfo, NULL, instance);
	
	if (result != VK_SUCCESS) {
		printf("Failed to create Vulkan instance (%i)\n", result);
		return -1;
	}

	return 0;
}

int selectPhysicalDevice(
	VkInstance instance,
	VkPhysicalDevice* physDev,
	const char** deviceExtensions,
	unsigned int deviceExtensionCount
)
{
	/* Get a list of all GPUs */

	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &deviceCount, NULL);

	printf("Physical device count: %i\n", deviceCount);

	if (!deviceCount) {
		printf("No supported devices found.\n");
		return -1;
	}

	VkPhysicalDevice devices[deviceCount];
	vkEnumeratePhysicalDevices(instance, &deviceCount, devices);

	/* Select the most suitable GPU */

	uint32_t gpuScore = 0;
	uint32_t gpuHiScore = 0;

	for (int i = 0; i < deviceCount; i++) {
		gpuScore = deviceHasExtensions(
			devices[i],
			deviceExtensions,
			deviceExtensionCount
		);

		if (gpuScore > gpuHiScore) {
			*physDev = devices[i];
			gpuHiScore = gpuScore;
		}
	}

	if (!gpuHiScore) {
		printf("Failed to find a suitable physical device\n");
		return -1;
	}

	return 0;
}

int createLogicalDevice(
	LogicalDevice* logicalDevice,
	VkPhysicalDevice physDev,
	VkSurfaceKHR surface,
	const char** deviceExtensions,
	unsigned int deviceExtensionCount,
	unsigned int instLayerCount,
	const char* instLayers[]
)
{
	QueueFamilyIndices queueFamilies = findQueueFamilies(physDev, surface);

	float queuePriority = 1.0f;

	VkDeviceQueueCreateInfo queueInfo[queueFamilies.uniqueIndexCount] = {};

	/* Add queues to queue creation info */
	for (int i = 0; i < queueFamilies.uniqueIndexCount; i++) {
		queueInfo[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueInfo[i].queueCount = 1;
		queueInfo[i].pQueuePriorities = &queuePriority;
		queueInfo[i].queueFamilyIndex = queueFamilies.uniqueIndices[i];
	}
	VkPhysicalDeviceFeatures deviceFeatures = {};
	deviceFeatures.samplerAnisotropy = VK_TRUE;

	VkDeviceCreateInfo deviceInfo = {};
	deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceInfo.pQueueCreateInfos = queueInfo;
	deviceInfo.queueCreateInfoCount = queueFamilies.uniqueIndexCount;
	deviceInfo.pEnabledFeatures = &deviceFeatures;

	deviceInfo.enabledExtensionCount = deviceExtensionCount;
	deviceInfo.ppEnabledExtensionNames = deviceExtensions;

	deviceInfo.enabledLayerCount = instLayerCount;
	deviceInfo.ppEnabledLayerNames = instLayers;

	if (vkCreateDevice(
		physDev,
		&deviceInfo,
		0,
		&logicalDevice->device
	)!= VK_SUCCESS) {
		return -1;
	}

	vkGetDeviceQueue(
		logicalDevice->device,
		queueFamilies.graphics,
		0,
		&logicalDevice->graphicsQueue
	);

	vkGetDeviceQueue(
		logicalDevice->device,
		queueFamilies.present,
		0,
		&logicalDevice->presentQueue
	);

	return 0;
}



