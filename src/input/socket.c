#include "socket.h"
#include "queuecmd.h"
#include "common/maths.h"

/* stb_image supports most of the classic image formats: JPG, PNG, BMP etc. */
#define STB_IMAGE_IMPLEMENTATION
#include "ext/stb_image.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <libigni/render.h>
#include <pthread.h>

/* At some point I should do my own asset importing. Converting mesh data twice
 * over isn't optimal. It will do for now. */
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

int createSocket(const char* path)
{
	int fd;

	if (!(fd = socket(AF_UNIX, SOCK_STREAM, 0))) {
		perror("Cannot create socket");
		return -1;
	}

	struct sockaddr_un sv_addr;
	sv_addr.sun_family = AF_UNIX;
	strncpy(sv_addr.sun_path, path, sizeof(sv_addr.sun_path));

	if (bind(fd, (struct sockaddr*)&sv_addr, sizeof(sv_addr)) == -1) {
		perror("Cannot bind socket");
		return -1;
	}

	if (listen(fd, 10) == -1) {
		perror("Cannot open socket for listening");
		return -1;
	}

	return fd;
}

int executeCmd(SceneArray* scenes, Display display, unsigned int idx)
{
	int result = -1;
	IgniRndOpcode opcode = IGNI_RENDER_OP_NUL;
	int recvResult = recv(scenes->scenes[idx].fd, &opcode, sizeof(opcode), 0);

	switch (opcode) {
	case IGNI_RENDER_OP_NUL:
		if (recvResult == -1) {
			/* EAGAIN is triggered when there is no data left to recieve. If
			 * this happens, look to other sockets for commands until new data
			 * arrives. */
			if (errno == EAGAIN) {
				errno = 0;
				return -1;
			}
			/* If it's a different error it really is a problem. */
			else {
				perror("recv() failed\n");
				result = -1;
				break;
			}
		}

		/* A null opcode without an error means the client
		 * disconnected. */

		break;

	case IGNI_RENDER_OP_CONFIGURE:
		result = cmdConfigure(&scenes->scenes[idx], display);
		break;

	case IGNI_RENDER_OP_MESH_CREATE:
		result = cmdMeshCreate(&scenes->scenes[idx], display);
		break;

	case IGNI_RENDER_OP_MESH_SET_SHADER:
		result = cmdMeshSetShader(&scenes->scenes[idx], display);
		break;

	case IGNI_RENDER_OP_MESH_BIND_TEXTURE:
		result = cmdMeshBindTexture(&scenes->scenes[idx], display);
		break;

	case IGNI_RENDER_OP_MESH_TRANSFORM:
		result = cmdMeshTransform(&scenes->scenes[idx], display);
		break;

	case IGNI_RENDER_OP_MESH_DELETE:
		result = cmdMeshDelete(&scenes->scenes[idx], display);
		break;

	case IGNI_RENDER_OP_POINT_LIGHT_CREATE:
		result = cmdPointLightCreate(&scenes->scenes[idx], display);
		break;

	case IGNI_RENDER_OP_POINT_LIGHT_TRANSFORM:
		result = cmdPointLightTransform(&scenes->scenes[idx], display);
		break;

	case IGNI_RENDER_OP_POINT_LIGHT_SET_COLOUR:
		result = cmdPointLightSetColour(&scenes->scenes[idx], display);
		break;

	case IGNI_RENDER_OP_POINT_LIGHT_DELETE:
		result = cmdPointLightDelete(&scenes->scenes[idx], display);
		break;

	case IGNI_RENDER_OP_TEXTURE_CREATE:
		result = cmdTextureCreate(&scenes->scenes[idx], display);
		break;

	case IGNI_RENDER_OP_TEXTURE_DELETE:
		result = cmdTextureDelete(&scenes->scenes[idx], display);
		break;

	case IGNI_RENDER_OP_VIEWPOINT_TRANSFORM:
		result = cmdViewpointTransform(&scenes->scenes[idx], display);
		break;

	default:
		printf("unknown opcode: %i\n", opcode);
		break;
	}

	if (result) {
		sceneArrayRemoveEntry(scenes, idx, display.dev.device);
		return -1;
	}

	return result;
}

/* This command exists to add compatibility between versions. */
int cmdConfigure(Scene* scene, Display display)
{
	IgniRndCmdConfigure cmd;
	recv(scene->fd, &cmd, sizeof(cmd), 0);

	scene->version = cmd.majVersion;

	return 0;
}

/* After some consideration, I have decided to make file loading the method of
 * mesh creation. The only alternative (as I'm aware) is directly sending raw
 * vertex and index data over network.
 *
 * 1. It adds flexibility on the server side.
 * 2. The protocol is subject to change, especially the structure of vertices. 
 * 	Changes to vertex structure can render a mesh unrecognisable.
 * 3. The size of the instruction gets reduced from chunks of data to a single
 * 	file path plus some small variables.
 *
 * CONS:
 * - Cannot easily load embedded resources
 * - May not support all file formats */
int cmdMeshCreate(Scene* scene, Display display)
{
	Mesh newMesh = {};

	IgniRndCmdMeshCreate cmd;
	recv(scene->fd, &cmd, sizeof(cmd), 0);

	/* The filename gets a null terminator to stop undefined behaviour. */
	char* path = malloc(cmd.pathLen + 1);
	recv(scene->fd, path, cmd.pathLen, 0);
	path[cmd.pathLen] = 0;

	const struct aiScene* impScene = 
		aiImportFile(path, aiProcessPreset_TargetRealtime_MaxQuality);

	free(path);

	if (!impScene) {
		printf("Failed to import mesh file. Make sure file exists.\n");
		return -1;
	}

	/* Don't create a mesh with an already existing ID. */
	if (findId(scene->meshIds, scene->meshCount, cmd.meshId) != -1) {
		printf("Mesh ID %i already exists.\n", cmd.meshId);
		return -1;
	}

	/* Allocate enough space before writing up the mesh */
	size_t vertexBufferSz = 0;

	/* At first, the vertex and index buffer sizes are simply the number of
	 * vertices and indices. */
	for (int i = 0; i < impScene->mNumMeshes; i++) {
		vertexBufferSz += impScene->mMeshes[i]->mNumVertices;

		/* Assuming the mesh got triangulated, as instructed to assimp */
		newMesh.indexCount += impScene->mMeshes[i]->mNumFaces * 3;
	}

	/* If there are too many vertices, 16-bit indices are upgraded to
	 * 32-bit indices and the buffer size is changed again. */

	char indexSize = 2;
	newMesh.indexType = VK_INDEX_TYPE_UINT16;
	
	if (vertexBufferSz > 32767) {
		newMesh.indexType = VK_INDEX_TYPE_UINT32;
		indexSize = 4;
	}

	vertexBufferSz *= sizeof(Vertex);	

	Vertex* vertexData = malloc(vertexBufferSz);
	if (!vertexData) {
		printf("Failed to allocate space for vertex buffer.\n");
		aiReleaseImport(impScene);
		return -1;
	}

	/* indexData is void because index size varies from mesh to mesh. */
	void* indexData = malloc(newMesh.indexCount * indexSize);
	if (!indexData) {
		printf("Failed to allocate space for index buffer.\n");
		aiReleaseImport(impScene);
		return -1;
	}

	uint16_t* indexPtr = indexData;

	/* Assimp's mesh data must be reformatted for the vertex and index buffers.
	 *
	 * All meshes in the imported scene get merged into one because the 'Create
	 * Mesh' command only provides one mesh ID. */

	struct aiMesh currentMesh;

	for (int i = 0; i < impScene->mNumMeshes; i++) {
		currentMesh = *impScene->mMeshes[i];

		/* Add vertices to mesh */
		for (int j = 0; j < currentMesh.mNumVertices; j++) {
			vertexData[j].pos[X] = currentMesh.mVertices[j].x;
			vertexData[j].pos[Y] = currentMesh.mVertices[j].y;
			vertexData[j].pos[Z] = currentMesh.mVertices[j].z;

			vertexData[j].texCoord[X] = currentMesh.mTextureCoords[0][j].x;
			vertexData[j].texCoord[Y] = currentMesh.mTextureCoords[0][j].y;

			vertexData[j].normal[X] = currentMesh.mNormals[j].x;
			vertexData[j].normal[Y] = currentMesh.mNormals[j].y;
			vertexData[j].normal[Z] = currentMesh.mNormals[j].z;
		}

		/* Add indices to mesh */
		for (int j = 0; j < currentMesh.mNumFaces; j++) {
			for (int k = 0; k < 3; k++) {
				*indexPtr = currentMesh.mFaces[j].mIndices[k];
				*(uintptr_t*)&indexPtr += indexSize;
			}
		}
	}

	if (createVertexBuffer(
		display.cmd,
		display.dev.graphicsQueue,
		display.dev.device,
		display.physicalDevice,
		vertexData,
		vertexBufferSz,
		&newMesh.vertexBuffer,
		&newMesh.vertexBufferMemory
	)) {
		aiReleaseImport(impScene);
		destroyMesh(display.dev.device, newMesh);
		return -1;
	}

	free(vertexData);

	if (createIndexBuffer(
		display.cmd,
		display.dev.graphicsQueue,
		display.dev.device,
		display.physicalDevice,
		indexData,
		newMesh.indexCount * indexSize,
		indexSize,
		&newMesh.indexBuffer, 
		&newMesh.indexBufferMemory
	)) {
		aiReleaseImport(impScene);
		destroyMesh(display.dev.device, newMesh);
		return -1;
	}

	free(indexData);

	aiReleaseImport(impScene);

	/* Create uniform buffers */

	const VkDeviceSize bufferSize = sizeof(ModelUniforms);
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		if (createBuffer(
			display.dev.device,
			display.physicalDevice,
			bufferSize,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
			| VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&newMesh.uniformBuffers[i],
			&newMesh.uboMemory[i]
		)) {
			printf("Failed to create uniform buffers.\n");
			destroyMesh(display.dev.device, newMesh);
			return -1;
		}

		if (vkMapMemory(
			display.dev.device,
			newMesh.uboMemory[i],
			0,
			bufferSize,
			0,
			&newMesh.uboMapped[i]
		) != VK_SUCCESS) {
			printf("Failed to map uniform buffer memory.\n");
			destroyMesh(display.dev.device, newMesh);
			return -1;
		}
	}

	/* Create descriptor pool */

	VkDescriptorPoolSize poolSizes[3] = {};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[0].descriptorCount = MAX_FRAMES_IN_FLIGHT;

	poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[1].descriptorCount = MAX_FRAMES_IN_FLIGHT;

	poolSizes[2].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[2].descriptorCount = MAX_FRAMES_IN_FLIGHT;

	VkDescriptorPoolCreateInfo descPoolInfo = {};
	descPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descPoolInfo.poolSizeCount = 3;
	descPoolInfo.pPoolSizes = poolSizes;
	descPoolInfo.maxSets = MAX_FRAMES_IN_FLIGHT;

	if (vkCreateDescriptorPool(
		display.dev.device,
		&descPoolInfo,
		0,
		&newMesh.descriptorPool
	) != VK_SUCCESS) {
		printf("Failed to create descriptor pool.\n");
		destroyMesh(display.dev.device, newMesh);
		return -1;
	}

	/* Create descriptor sets */

	VkDescriptorSetLayout descSetLayouts[MAX_FRAMES_IN_FLIGHT];
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		descSetLayouts[i] = display.geom.descSetLayout;
	}

	VkDescriptorSetAllocateInfo gAllocInfo = {};
	gAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	gAllocInfo.descriptorPool = newMesh.descriptorPool;
	gAllocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
	gAllocInfo.pSetLayouts = descSetLayouts;

	if (vkAllocateDescriptorSets(
		display.dev.device,
		&gAllocInfo,
		newMesh.descriptorSets
	) != VK_SUCCESS) {
		printf("Failed to allocate descriptor sets.\n");
		destroyMesh(display.dev.device, newMesh);
		return -1;
	}

	ModelUniforms meshUBO = {0.0f};

	/* Default transform matrix - no transformation applied to mesh */
	meshUBO.tform[X][X] = 1.0f;
	meshUBO.tform[Y][Y] = 1.0f;
	meshUBO.tform[Z][Z] = 1.0f;
	meshUBO.tform[W][W] = 1.0f;

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		/* Viewpoint Uniform Buffer */
		VkDescriptorBufferInfo povBufferInfo = {};
		povBufferInfo.buffer = display.pov.uniformBuffers[i];
		povBufferInfo.offset = 0;
		povBufferInfo.range = sizeof(ViewpointUniforms);

		VkWriteDescriptorSet povWrite = {};
		povWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		povWrite.dstSet = newMesh.descriptorSets[i];
		povWrite.dstBinding = 0;
		povWrite.dstArrayElement = 0;
		povWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		povWrite.descriptorCount = 1;
		povWrite.pBufferInfo = &povBufferInfo;

		vkUpdateDescriptorSets(display.dev.device, 1, &povWrite, 0, 0);

		/* The default texture is a single magenta pixel */
		VkDescriptorBufferInfo nullBuffer = {};
		nullBuffer.buffer = VK_NULL_HANDLE;
		nullBuffer.offset = 0;
		nullBuffer.range = 0;

		VkDescriptorImageInfo nullImage = {};
		nullImage.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		nullImage.imageView = display.nulTexture.view;
		nullImage.sampler = display.nulTexture.sampler;

		VkWriteDescriptorSet nulTexWriteDesc = {};
		nulTexWriteDesc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		nulTexWriteDesc.dstSet = newMesh.descriptorSets[i];
		nulTexWriteDesc.dstBinding = 1;
		nulTexWriteDesc.dstArrayElement = 0;
		nulTexWriteDesc.descriptorType =
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		nulTexWriteDesc.descriptorCount = 1;
		nulTexWriteDesc.pBufferInfo = &nullBuffer;
		nulTexWriteDesc.pImageInfo = &nullImage;
		vkUpdateDescriptorSets(display.dev.device, 1, &nulTexWriteDesc, 0, 0);

		/* Mesh Uniform Buffer */
		VkDescriptorBufferInfo meshBufferInfo = {};
		meshBufferInfo.buffer = newMesh.uniformBuffers[i];
		meshBufferInfo.offset = 0;
		meshBufferInfo.range = sizeof(ModelUniforms);

		VkWriteDescriptorSet meshWriteDesc = {};
		meshWriteDesc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		meshWriteDesc.dstSet = newMesh.descriptorSets[i];
		meshWriteDesc.dstBinding = 2;
		meshWriteDesc.dstArrayElement = 0;
		meshWriteDesc.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		meshWriteDesc.descriptorCount = 1;
		meshWriteDesc.pBufferInfo = &meshBufferInfo;

		vkUpdateDescriptorSets(display.dev.device, 1, &meshWriteDesc, 0, 0);

		/* Start the mesh out with its default transforms */
		memcpy(newMesh.uboMapped[i], &meshUBO, sizeof(ModelUniforms));
	}

	/* Once the mesh is successfully set up, it is ready for the scene. */ 

	if (scene->meshCount >= scene->meshLimit)  { 
		scene->meshLimit *= 2; 
		scene->meshes = (Mesh*)realloc( 
			scene->meshes, 
			sizeof(Mesh) * scene->meshLimit 
		); 
		scene->meshIds = (int*)realloc( 
			scene->meshIds, 
			sizeof(int) * scene->meshLimit 
		); 
	}

	scene->meshes[scene->meshCount] = newMesh;
	scene->meshIds[scene->meshCount] = cmd.meshId;
	++scene->meshCount;

	return 0;
}

int cmdMeshSetShader(Scene* scene, Display display)
{
	IgniRndCmdMeshSetShader cmd;
	recv(scene->fd, &cmd, sizeof(cmd), 0);

	return 0;
}

int cmdMeshBindTexture(Scene* scene, Display display)
{
	IgniRndCmdMeshBindTexture cmd;
	recv(scene->fd, &cmd, sizeof(cmd), 0);

	if (cmd.target > 0) {
		printf("Invalid texture target\n");
		return -1;
	};

	int texIdx = findId(scene->textureIds, scene->texCount, cmd.textureId);

	if (texIdx == -1) {
		printf("Failed to find texture.\n");
		return -1;
	}

	QueueCommand qCmd;
	qCmd.opcode = QUEUE_CMD_MESH_BIND_TEXTURE;
	qCmd.repeats = MAX_FRAMES_IN_FLIGHT;
	qCmd.data = malloc(sizeof(QCmdMeshBindTexture));

	if (!qCmd.data) {
		printf("Failed to allocate space for command qMeshBindTexture\n");
		return -1;
	}

	QCmdMeshBindTexture qCmdData = {0};
	qCmdData.meshId = cmd.meshId;
	qCmdData.view = scene->textures[texIdx].view;
	qCmdData.sampler = scene->textures[texIdx].sampler;
	qCmdData.pass = cmd.target;
	*(QCmdMeshBindTexture*)qCmd.data = qCmdData;

	pushCommandToQueue(&scene->uniformCommands, qCmd);

	int meshIdx = findId(scene->meshIds, scene->meshCount, cmd.meshId);

	if (meshIdx == -1) {
		printf("mesh not found.\n");
		return -1;
	}

	/* Resize the texture's array of bound meshes */
	Texture* selTex = &scene->textures[texIdx];

	selTex->boundMeshes[selTex->boundMeshCount] = cmd.meshId;
	selTex->boundPasses[selTex->boundMeshCount] = cmd.target;
	++selTex->boundMeshCount;

	if (selTex->boundMeshCount >= selTex->boundMeshLimit) {
		selTex->boundMeshLimit *= 2;
		selTex->boundMeshes = (int*)realloc(
			selTex->boundMeshes,
			sizeof(int) * selTex->boundMeshLimit
		);
		selTex->boundPasses = (int*)realloc(
			selTex->boundPasses,
			sizeof(int) * selTex->boundMeshLimit
		);
	}

	return 0;
}

int cmdMeshTransform(Scene* scene, Display display)
{
	IgniRndCmdMeshTransform cmd;
	recv(scene->fd, &cmd, sizeof(cmd), 0);

	int meshIdx = findId(scene->meshIds, scene->meshCount, cmd.meshId);

	if (meshIdx == -1) {
		printf("mesh not found.\n");
		return -1;
	}

	float transform[4][4] = FILL_MAT4(1.0f);
	transform[3][0] = 0;
	transform[3][1] = 0;
	transform[3][2] = 0;

	/* Scale, then rotate, then transform (T*R*S) 
	 * Yeah, matrix multiplication is done in reverse. I don't know why. */
	transform3d(transform, cmd.xLoc, cmd.yLoc, cmd.zLoc);
	rotate3d(transform, cmd.xRot, cmd.yRot, cmd.zRot);
	scale3d(transform, cmd.xScale, cmd.yScale, cmd.zScale);
	
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		memcpy(
			scene->meshes[meshIdx].uboMapped[i],
			transform,
			sizeof(transform)
		);
	}

	return 0;
}

int cmdMeshDelete(Scene* scene, Display display)
{
	IgniRndCmdMeshDelete cmd;
	recv(scene->fd, &cmd, sizeof(cmd), 0);

	int meshIdx = findId(scene->meshIds, scene->meshCount, cmd.meshId);

	if (meshIdx == -1) {
		printf("mesh not found.\n");
		return -1;
	}

	destroyMesh(display.dev.device, scene->meshes[meshIdx]);

	scene->meshCount--;
	
	memcpy(
		&scene->meshes[meshIdx],
		&scene->meshes[meshIdx + 1],
		(scene->meshCount - meshIdx) * sizeof(Scene)
	);

	/* Allocate less space if too much is allocated */
	while (scene->meshCount < scene->meshLimit / 2) {
		scene->meshLimit /= 2;
		scene->meshes = 
			(Mesh*)realloc(scene->meshes, sizeof(Mesh) * scene->meshLimit);

		if (!scene->meshes) {
			perror("Failed to reallocate meshes");
			return -1;
		}
	}

	return 0;
}

int cmdPointLightCreate(Scene* scene, Display display)
{
	IgniRndCmdPointLightCreate cmd;
	recv(scene->fd, &cmd, sizeof(cmd), 0);

	return 0;
}

int cmdPointLightTransform(Scene* scene, Display display)
{
	IgniRndCmdPointLightTransform cmd;
	recv(scene->fd, &cmd, sizeof(cmd), 0);

	return 0;
}

int cmdPointLightSetColour(Scene* scene, Display display)
{
	IgniRndCmdPointLightSetColour cmd;
	recv(scene->fd, &cmd, sizeof(cmd), 0);

	return 0;
}

int cmdPointLightDelete(Scene* scene, Display display)
{
	IgniRndCmdPointLightDelete cmd;
	recv(scene->fd, &cmd, sizeof(cmd), 0);

	return 0;
}

int cmdTextureCreate(Scene* scene, Display display)
{
	IgniRndCmdTextureCreate cmd;
	recv(scene->fd, &cmd, sizeof(cmd), 0);

	char* path = malloc(cmd.pathLen + 1);
	recv(scene->fd, path, cmd.pathLen, 0);

	if (!path) {
		printf("Failed to allocate memory for filename\n");
		return -1;
	}

	path[cmd.pathLen] = 0;

	/* Read image data */

	Texture newTexture;
	stbi_uc* pixels;
	int texDepth;

	/* RGB+Alpha is by far the most common pixel format supported by GPUs.
	 * RGB sometimes isn't even available. It's all because 4 colour channels 
	 * are easier to align than 3. */
	pixels = stbi_load(path,
		&newTexture.width,
		&newTexture.height,
		&texDepth,
		STBI_rgb_alpha
	);

	free(path);

	if (!pixels) {
		printf("Failed to load image.\n");
		return -1; 
	}

	/* Don't create a texture with an already existing ID. */
	if (findId(scene->textureIds, scene->texCount, cmd.textureId) != -1) {
		printf("Texture ID %i already exists.\n", cmd.textureId);
		return -1;
	}

	if (createTexture(
		&newTexture, 
		display.dev.device,
		display.physicalDevice
	)) {
		return -1;
	}

	if (writeTexture(
		&newTexture,
		pixels,
		display.dev.device,
		display.physicalDevice,
		display.cmd,
		display.dev.graphicsQueue
	)) {
		stbi_image_free(pixels);
		return -1;
	}

	stbi_image_free(pixels);

	/* Once the texture is successfully set up, it is ready for the scene. */

	if (scene->texCount >= scene->texLimit) {
		scene->texLimit *= 2;
		scene->textures = (Texture*)
			realloc(scene->textures, sizeof(Texture) * scene->texLimit);

		scene->textureIds = (int*)
			realloc(scene->textureIds,sizeof(int) * scene->texLimit);
	}

	scene->textures[scene->texCount] = newTexture;
	scene->textureIds[scene->texCount] = cmd.textureId;

	++scene->texCount;

	return 0;
}

int cmdTextureDelete(Scene* scene, Display display)
{
	IgniRndCmdTextureDelete cmd;
	recv(scene->fd, &cmd, sizeof(cmd), 0);

	int texIdx = findId(scene->textureIds, scene->texCount, cmd.textureId);

	if (texIdx == -1) {
		printf("texture not found.\n");
		return -1;
	}

	/* If a mesh has this texture bound, the GPU will freeze up mid render. */

	QueueCommand qCmd = {};
	qCmd.opcode = QUEUE_CMD_MESH_BIND_TEXTURE;
	qCmd.repeats = MAX_FRAMES_IN_FLIGHT;

	QCmdMeshBindTexture qCmdData = {0};

	for (int i = 0; i < scene->textures[texIdx].boundMeshCount; ++i) {
		qCmd.data = malloc(sizeof(QCmdMeshBindTexture));

		if (!qCmd.data) {
			printf("Failed to allocate space for command qMeshBindTexture\n\
	reverse causality\n");
			return -1;
		}

		qCmdData.meshId = scene->textures[texIdx].boundMeshes[i];
		qCmdData.view = display.nulTexture.view;
		qCmdData.sampler = display.nulTexture.sampler;
		qCmdData.pass = scene->textures[texIdx].boundPasses[i];
		*(QCmdMeshBindTexture*)qCmd.data = qCmdData;

		pushCommandToQueue(&scene->uniformCommands, qCmd);
	}

	
	destroyTexture(display.dev.device, scene->textures[texIdx]);

	scene->texCount--;
	
	memcpy(
		&scene->textures[texIdx],
		&scene->textures[texIdx + 1],
		(scene->texCount - texIdx) * sizeof(Scene)
	);

	/* Allocate less space if too much is allocated */
	while (scene->texCount < scene->texLimit / 2) {
		scene->texLimit /= 2;
		scene->textures = (Texture*)
			realloc(scene->textures, sizeof(Texture) * scene->texLimit);

		if (!scene->textures) {
			perror("Failed to reallocate textures");
			return -1;
		}
	}

	return 0;

}

int cmdViewpointTransform(Scene* scene, Display display)
{
	IgniRndCmdViewpointTransform cmd;
	recv(scene->fd, &cmd, sizeof(cmd), 0);

	ViewpointUniforms ubo = {};
	
	Vec3 eye = {0.0f}, centre = {0.0f}, up = {0.0f};
	eye.x = cmd.xLoc;
	eye.y = cmd.yLoc;
	eye.z = cmd.zLoc;
	centre.x = cmd.xLook;
	centre.y = cmd.yLook;
	centre.z = cmd.zLook;
	up.z = 1.0f;

	display.pov.fov = cmd.fov;
	ubo.view = matLook(eye, centre, up);
	ubo.proj = matPersp(
		display.pov.fov,
		(float)display.swapchain.extent.width
		/ (float)display.swapchain.extent.height,
		0.1f,
		10.0f
	);

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		memcpy(
			display.pov.uboMapped[i],
			&ubo,
			sizeof(ViewpointUniforms)
		);
	}

	return 0;
}

int execUniformCommands(Scene* scene, Display* display)
{
	for (int i = 0; i < scene->uniformCommands.commandCount; i++) {
		/* A command with 0 repeats is removed from the command queue 
		 * and skipped. */
		if (!scene->uniformCommands.commands[i].repeats) {
			unqueueCommand(&scene->uniformCommands, i);
			continue;
		}

		scene->uniformCommands.commands[i].repeats--;
	
		execUboCommand(display, scene, &scene->uniformCommands.commands[i]);
	}

	return 0;
}

int execUboCommand(Display* display, Scene* scene, QueueCommand* cmd)
{
	switch (cmd->opcode) {
	case QUEUE_CMD_MESH_BIND_TEXTURE:
		return qMeshBindTexture(display, scene, cmd->data);
	default:
		break;
	}

	return 0;
}

int qMeshBindTexture(
	Display* display,
	Scene* scene,
	QCmdMeshBindTexture* cmd
)
{
	/* Wait for frame */
	vkWaitForFences(
		display->dev.device,
		1,
		&display->geomSync[display->currentFrame].inFlight,
		VK_TRUE,
		UINT64_MAX
	);

	int meshIdx = findId(scene->meshIds, scene->meshCount, cmd->meshId);

	if (meshIdx == -1) {
		printf("Failed to find mesh.\n");
		return -1;
	}

	VkDescriptorImageInfo imageInfo = {};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfo.imageView = cmd->view;
	imageInfo.sampler = cmd->sampler;

	VkWriteDescriptorSet writeDesc = {};
	writeDesc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeDesc.dstSet =
		scene->meshes[meshIdx].descriptorSets[display->currentFrame];
	writeDesc.dstBinding = 1;
	writeDesc.dstArrayElement = 0;
	writeDesc.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writeDesc.descriptorCount = 1;
	writeDesc.pImageInfo = &imageInfo;

	vkUpdateDescriptorSets(display->dev.device, 1, &writeDesc, 0, 0);

	return 0;
}

