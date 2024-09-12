#include "queuecmd.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int createCommandQueue(CommandQueue* queue)
{
	queue->commandLimit = 1;
	queue->commandCount = 0;
	queue->commands = (QueueCommand*)malloc(sizeof(QueueCommand));

	if (!queue->commands) {
		printf("Failed to create command queue\n");
		return -1;
	}

	return 0;
}

void destroyCommandQueue(CommandQueue queue)
{
	for (int i = 0; i < queue.commandCount; i++) {
		free(queue.commands[i].data);
	}

	free(queue.commands);
}

int pushCommandToQueue(CommandQueue* queue, QueueCommand cmd)
{
	if (queue->commandCount >= queue->commandLimit) {
		queue->commandLimit *= 2;

		void* newQueueCommands = realloc(
			queue->commands,
			queue->commandLimit * sizeof(QueueCommand)
		);

		if (!newQueueCommands) {
			perror("Failed to reallocate command queue");
			return -1;
		}

		queue->commands = (QueueCommand*)newQueueCommands;
	}

	queue->commands[queue->commandCount] = cmd;
	++queue->commandCount;

	return 0;
}


int unqueueCommand(CommandQueue* queue, unsigned int index)
{
	--queue->commandCount;

	free(queue->commands[index].data);

	memcpy(
		&queue->commands[index],
		&queue->commands[index + 1],
		(queue->commandCount - index) * sizeof(QueueCommand)
	);

	if (queue->commandCount < queue->commandLimit / 2) {
		queue->commandLimit /= 2;

		queue->commands = (QueueCommand*)realloc(
			queue->commands,
			queue->commandLimit * sizeof(QueueCommand)
		);

		if (!queue->commands) {
			perror("Failed to reallocate command queue");
			return -1;
		}
	}

	return 0;
}

