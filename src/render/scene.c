#include "scene.h"
#include "common/maths.h" 
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <errno.h>
#include <string.h> //memcpy
#include <unistd.h>
#include <math.h>

int createSceneArray(SceneArray* scenes)
{
	scenes->sceneLimit = sizeof(Scene);
	scenes->sceneCount = 0;
	scenes->scenes = (Scene*)malloc(sizeof(Scene));
	if (!scenes->scenes) {
		perror("Failed to allocate memory for scenes");
		return -1;
	}

	return 0;
}

int sceneArrayRemoveEntry(SceneArray* scenes, unsigned int idx, VkDevice device)
{
	destroyScene(device, scenes->scenes[idx]);

	close(scenes->scenes[idx].fd);

	scenes->sceneCount--;

	memcpy(
		&scenes->scenes[idx],
		&scenes->scenes[idx + 1],
		(scenes->sceneCount - idx) * sizeof(Scene)
	);

	/* Allocate less space if too much is allocated */
	while (scenes->sceneCount < scenes->sceneLimit / 2) {
		scenes->sceneLimit /= 2;
		scenes->scenes = (Scene*)
			realloc(scenes->scenes, sizeof(Scene) * scenes->sceneLimit);
		if (!scenes->scenes) {
			perror("Failed to reallocate scenes");
			return -1;
		}
	}

	return 0;
}

int sceneArrayAddEntry(SceneArray* scenes, Scene scene)
{
	/* Allocate more space if not enough is available */
	while (scenes->sceneCount >= scenes->sceneLimit) {
		scenes->sceneLimit *= 2;
		scenes->scenes = (Scene*)
			realloc(scenes->scenes, sizeof(Scene) * scenes->sceneLimit);
		if (!scenes->scenes) {
			perror("Failed to reallocate scenes");
			return -1;
		}
	}

	scenes->scenes[scenes->sceneCount] = scene;

	++scenes->sceneCount;
}

void destroySceneArray(VkDevice device, SceneArray scenes)
{
	for (int i = 0; i < scenes.sceneCount; i++) {
		destroyScene(device, scenes.scenes[i]);
	}

	free(scenes.scenes);
}

int createScene(Scene* scene, int fd)
{
	scene->fd = fd;
	scene->version = 0;

	scene->meshes = (Mesh*)malloc(sizeof(Mesh));
	scene->meshIds = (int*)malloc(sizeof(int));
	scene->meshCount = 0;
	scene->meshLimit = 1;

	scene->textures = (Texture*)malloc(sizeof(Texture));
	scene->textureIds = (int*)malloc(sizeof(int));
	scene->texCount = 0;
	scene->texLimit = 1;

	scene->pointLights = (PointLight*)malloc(sizeof(PointLight));
	scene->pointLightIds = (int*)malloc(sizeof(int));
	scene->ptLightCount = 0;
	scene->ptLightLimit = 1;

	if (createCommandQueue(&scene->uniformCommands)) {
		return -1;
	}

	return 0;
}


int createTexture(Texture* tex, VkDevice device, VkPhysicalDevice physDev)
{
	const uint32_t mipLevels = 1;
	tex->mipLevels = floor(log2(max(tex->width, tex->height)));

	/* The number of mip levels cannot be less or equal to the minimum. The
	 * minimum is 0 right now. */
	if (!tex->mipLevels) tex->mipLevels = 1;

	if (createImage(
		device,
		physDev,
		tex->width,
		tex->height,
		tex->mipLevels,
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT
		| VK_IMAGE_USAGE_TRANSFER_DST_BIT
		| VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		&tex->img,
		&tex->mem
	)) {
		return -1;
	}

	/* the image view */

	if (createImageView(
		device,
		tex->img,
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_IMAGE_ASPECT_COLOR_BIT,
		tex->mipLevels,
		&tex->view
	)) {
		return -1;
	}

	if (createSampler(device, physDev, &tex->sampler, tex->mipLevels)) {
		return -1;
	}

	return 0;
}


int writeTexture(
	Texture* tex,
	void* pixels,
	VkDevice device,
	VkPhysicalDevice physDev,
	VkCommandBuffer cmdBuf,
	VkQueue queue
)
{
	const int pixDepth = 4;

	VkDeviceSize imageSize = tex->width * tex->height * pixDepth;

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	if (createBuffer(
		device,
		physDev,
		imageSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
		| VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&stagingBuffer,
		&stagingBufferMemory
	)) {
		return -1;
	}

	void* data;
	vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
	memcpy(data, pixels, (size_t)imageSize);
	vkUnmapMemory(device, stagingBufferMemory);

	if (transitionImageLayout(
		cmdBuf,
		queue,
		tex->img,
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		tex->mipLevels
	)) {
		return -1;
	}
	
	if (copyBufferToImage(
		cmdBuf,
		queue,
		stagingBuffer,
		tex->img,
		tex->width,
		tex->height
	)) {
		return -1;
	}

	/* Generating mipmaps transitions the image layout to 
	 * VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL*/

	if (generateMipmaps(
		physDev,
		cmdBuf,
		queue,
		tex->img,
		VK_FORMAT_R8G8B8A8_SRGB,
		tex->width,
		tex->height,
		tex->mipLevels
	)) {
		printf("Failed to generate mipmaps.\n");
		tex->mipLevels = 1;
	}

	vkDestroyBuffer(device, stagingBuffer, 0);
	vkFreeMemory(device, stagingBufferMemory, 0);

	return 0;
}

void destroyScene(VkDevice device, Scene scene)
{
	vkDeviceWaitIdle(device);

	for (int i = 0; i < scene.meshCount; i++) {
		destroyMesh(device, scene.meshes[i]);
	}

	for (int i = 0; i < scene.texCount; i++) {
		destroyTexture(device, scene.textures[i]);
	}


	free(scene.meshes);
	free(scene.meshIds);

	free(scene.textures);
	free(scene.textureIds);

	free(scene.pointLights);
	free(scene.pointLightIds);

	destroyCommandQueue(scene.uniformCommands);
}

void destroyMesh(VkDevice device, Mesh mesh)
{   
	vkDestroyBuffer(device, mesh.vertexBuffer, 0);
	vkFreeMemory(device, mesh.vertexBufferMemory, 0);
	
	vkDestroyBuffer(device, mesh.indexBuffer, 0);
	vkFreeMemory(device, mesh.indexBufferMemory, 0);
		
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		vkDestroyBuffer(device, mesh.uniformBuffers[i], 0);
		vkFreeMemory(device, mesh.uboMemory[i], 0);
	}

	vkDestroyDescriptorPool(device, mesh.descriptorPool, 0);
}

void destroyViewpoint(VkDevice device, Viewpoint viewpoint)
{
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		vkDestroyBuffer(device, viewpoint.uniformBuffers[i], 0);
		vkFreeMemory(device, viewpoint.uboMemory[i], 0);
	}
}

void destroyTexture(VkDevice device, Texture texture)
{
	vkDeviceWaitIdle(device);

	vkDestroySampler(device, texture.sampler, 0);
	vkDestroyImageView(device, texture.view, 0);

	vkDestroyImage(device, texture.img, 0);
	vkFreeMemory(device, texture.mem, 0);

	free(texture.boundMeshes);
	free(texture.boundPasses);
}

int createViewpoint(Viewpoint* pov, VkDevice dev, VkPhysicalDevice physDev)
{
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		/* POV */

		if (createUniformBuffers(
			dev,
			physDev,
			&pov->uniformBuffers[i],
			&pov->uboMemory[i],
			&pov->uboMapped[i],
			sizeof(ViewpointUniforms)
		)) {
			printf("Failed to create viewpoint\n");
			return -1;
		}
	}

	return 0;
}

int findId(int* ids, unsigned int idCount, int query)
{
	for (int i = 0; i < idCount; i++) {
		if (ids[i] == query) return i;
	}

	return -1;
}


