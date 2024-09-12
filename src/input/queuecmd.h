#ifndef INPUT_QUEUECMD_H
#define INPUT_QUEUECMD_H 1

/* Queued commands are necessary to support multiple frames in flight.
 * When the GPU is drawing, descriptor sets become off limits. Instead of
 * waiting around for a descriptor set to become available again, changes are
 * only made to descriptor sets that aren't in use. */

#include <libigni/render.h>
#include <vulkan/vulkan.h>

typedef unsigned char qCmdOpcode;
enum
{
	QUEUE_CMD_MESH_BIND_TEXTURE = 0,
	QUEUE_CMD_MESH_WRITE_UNIFORM
};

typedef struct
{
	uint8_t pass;
	IgniRndElementId meshId;
	IgniRndElementId meshIndex;
	VkImageView view;
	VkSampler sampler;
} QCmdMeshBindTexture;

typedef struct
{
	qCmdOpcode opcode;
	unsigned char repeats;
	void* data;
} QueueCommand;

typedef struct
{
	QueueCommand* commands;
	unsigned int commandCount;
	unsigned int commandLimit;
} CommandQueue;

int createCommandQueue(CommandQueue* queue);
void destroyCommandQueue(CommandQueue queue);

int pushCommandToQueue(CommandQueue* queue, QueueCommand cmd);
int unqueueCommand(CommandQueue* queue, unsigned int index);

#endif

