#ifndef RENDER_SCENE_H
#define RENDER_SCENE_H 1

#include <vulkan/vulkan.h>
#include "misc.h"
#include "common/maths.h"
#include "input/queuecmd.h"

typedef struct
{
	float pos[3];
	float normal[3];
	float texCoord[2];
} Vertex;

typedef struct
{   
	float tform[4][4];
} ModelUniforms;

typedef struct
{
	Mat4 view;
	Mat4 proj;
} ViewpointUniforms;

typedef struct
{
	VkBuffer uniformBuffers[MAX_FRAMES_IN_FLIGHT];
	VkDeviceMemory uboMemory[MAX_FRAMES_IN_FLIGHT];
	void* uboMapped[MAX_FRAMES_IN_FLIGHT];
	float fov;
} Viewpoint;

typedef struct
{
	VkBuffer vertexBuffer;
	VkDeviceMemory vertexBufferMemory;

	VkBuffer indexBuffer;
	VkDeviceMemory indexBufferMemory;
	unsigned int indexCount;
	VkIndexType indexType;

	/* The properties of a mesh are held in its descriptor sets. */ 
	VkDescriptorPool descriptorPool;
	VkDescriptorSet descriptorSets[MAX_FRAMES_IN_FLIGHT];

	VkBuffer uniformBuffers[MAX_FRAMES_IN_FLIGHT];
	VkDeviceMemory uboMemory[MAX_FRAMES_IN_FLIGHT];
	void* uboMapped[MAX_FRAMES_IN_FLIGHT];
	unsigned int size;
} Mesh;

typedef struct
{
	float x, y, z;
	float r, g, b;
	float intensity;
	float distance;
} PointLight;

typedef struct
{
	int width;
	int height;

	uint32_t mipLevels;
	VkImage img;
	VkDeviceMemory mem;
	VkImageView view;
	VkSampler sampler;
	VkDescriptorSet samplerDescSet;

	/* IDs of meshes bound to texture */
	/* I will definitely completely redo this server. */
	int* boundMeshes;
	int* boundPasses;
	unsigned int boundMeshLimit;
	unsigned int boundMeshCount;
} Texture;

/* IDs are in separate arrays to reduce cache misses. */
typedef struct
{
	Mesh* meshes;
	int* meshIds;
	unsigned int meshLimit;
	unsigned int meshCount;

	Texture* textures;
	int* textureIds;
	unsigned int texLimit;
	unsigned int texCount;

	PointLight* pointLights;
	int* pointLightIds;
	unsigned int ptLightLimit;
	unsigned int ptLightCount;

	CommandQueue uniformCommands;
	
	int fd;
	char version;
	char hasViewpoint;
} Scene;

typedef struct
{
	Scene* scenes;
	unsigned int sceneCount;
	unsigned int sceneLimit;
} SceneArray;

int createSceneArray(SceneArray* scenes);
int sceneArrayAddEntry(SceneArray* scenes, Scene scene);
int sceneArrayRemoveEntry(SceneArray* scenes, unsigned int idx, VkDevice device);
void destroySceneArray(VkDevice device, SceneArray scenes);

int createScene(Scene* scene, int fd);

int createTexture(Texture* tex, VkDevice device, VkPhysicalDevice physDev);
int writeTexture(
	Texture* tex,
	void* pixels,
	VkDevice device,
	VkPhysicalDevice physDev,
	VkCommandBuffer cmdBuf,
	VkQueue queue
);

void destroyScene(VkDevice device, Scene scene);
void destroyMesh(VkDevice device, Mesh mesh);
void destroyViewpoint(VkDevice device, Viewpoint viewpoint);
void destroyTexture(VkDevice device, Texture texture);

int createViewpoint(Viewpoint* pov, VkDevice dev, VkPhysicalDevice physDev);

int findId(int* ids, unsigned int idCount, int query);

#endif

