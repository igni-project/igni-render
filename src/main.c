#include "main.h"
#include "input/socket.h"
#include "render/scene.h"
#include "render/display.h"
#include "render/misc.h"
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define FPS_CAP 60

int main(int argc, char* argv[], char* envp[])
{
	int srvFd = createSocket(getenv("IGNI_RENDER_SRV"));
	if (srvFd == -1) exit(EXIT_FAILURE);

	/* The socket is ready so it is now safe to run some processes. 
	 * It's best to do this step early to prevent fork() from breaking
	 * too much. */

	pid_t pid;

	/* argv[0] is the path to this program so that gets skipped. */
	for (int i = 1; i < argc; ++i) {
		pid = fork();

		if (!pid) break;

		if (pid == -1) {
			perror("Failed to fork process");
			exit(EXIT_FAILURE);
		}

		printf("Execute %s\n", argv[i]);
		if (execve(argv[i], 0, envp) == -1) {
			printf("Failed to execute ");
			perror(argv[i]);
			exit(EXIT_FAILURE);
		}
	}

	/* Create the display frame */
	Display display;
	if (createDisplay(&display)) exit(EXIT_FAILURE);

	/* I wound up separating the render passes and display on many occasions,
	 * so I am keeping it that way. */
	if (createRenderPasses(&display)) exit(EXIT_FAILURE);

	/* Scene Array */	
	SceneArray scenes;
	if (createSceneArray(&scenes) == -1) exit(EXIT_FAILURE);

	/* In windowed mode, the display server refreshes at a constant rate
	 * to detect window closing and resizing. */
	struct timeval idleFrameTime = {0, 500000};

	/* Main Loop */

	char shouldClose = 0;
	int activity;
	fd_set readFds;
	int maxFd;
	clock_t frameTime = clock();

	while (!shouldClose) {
		shouldClose = 0;

		/* In addition to limiting the framerate, this allows multiple
		 * ronds of commands to process between. */
		if (clock() - frameTime > CLOCKS_PER_SEC / FPS_CAP) {
			frameTime = clock();

			display.currentFrame = 
				++display.currentFrame % MAX_FRAMES_IN_FLIGHT;

			for (int i = scenes.sceneCount - 1; i != -1; --i) {
				execUniformCommands(&scenes.scenes[i], &display);
			}

			shouldClose = renderScenes(&display, scenes);
		}

		FD_ZERO(&readFds);
		FD_SET(srvFd, &readFds);

		maxFd = srvFd;
		for (int i = 0; i < scenes.sceneCount; i++) {
			FD_SET(scenes.scenes[i].fd, &readFds);

			if (scenes.scenes[i].fd > maxFd) {
				maxFd = scenes.scenes[i].fd;
			}
		}

		activity = select(maxFd + 1, &readFds, 0, 0, &idleFrameTime);
		if (activity == -1 && errno != EINTR) {
			perror("Socket select failed");
		}

		/* Activity on the server socket means a new connection */
		if (FD_ISSET(srvFd, &readFds)) {
			/* Throwaway variables for accept() */
			struct sockaddr_un tmpAddr = {0};
			socklen_t tmpAddrSz = 0;
			
			int newFd = accept(srvFd, (struct sockaddr*)&tmpAddr, &tmpAddrSz);
			if (newFd == -1) {
				perror("Failed to accept client");
			}
			else {
				Scene newScene;
				createScene(&newScene, newFd);
				sceneArrayAddEntry(&scenes, newScene);
			}
		}

		/* This loop iterates in reverse to let already iterated scenes get
		 * shifted around when a scene is called for deletion.
		 *
		 * In addition, it gives the early connections priority over the
		 * camera. */
		for (int i = scenes.sceneCount - 1; i != -1; --i) {
			if (FD_ISSET(scenes.scenes[i].fd, &readFds)) {
				executeCmd(&scenes, display, i);
			}
		}
	}

	vkDeviceWaitIdle(display.dev.device);
	destroySceneArray(display.dev.device, scenes);
	destroyRenderPasses(display);
	destroyDisplay(display);

	return 0;
}

