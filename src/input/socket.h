#ifndef INPUT_SOCKET_H
#define INPUT_SOCKET_H 1

#include "render/scene.h"
#include "render/display.h"

int createSocket(const char* path);

int executeCmd(SceneArray* scenes, Display display, unsigned int idx);

int cmdConfigure(Scene* scene, Display display);

int cmdMeshCreate(Scene* scene, Display display);
int cmdMeshSetShader(Scene* scene, Display display);
int cmdMeshBindTexture(Scene* scene, Display display);
int cmdMeshTransform(Scene* scene, Display display);
int cmdMeshDelete(Scene* scene, Display display);

int cmdPointLightCreate(Scene* scene, Display display);
int cmdPointLightTransform(Scene* scene, Display display);
int cmdPointLightSetColour(Scene* scene, Display display);
int cmdPointLightDelete(Scene* scene, Display display);
int cmdTextureCreate(Scene* scene, Display display);
int cmdTextureDelete(Scene* scene, Display display);
int cmdViewpointTransform(Scene* scene, Display display);

int execUniformCommands(Scene* scene, Display* display);
int execUboCommand(Display* display, Scene* scene, QueueCommand* cmd);
int qMeshBindTexture(
	Display* display,
	Scene* scene,
	QCmdMeshBindTexture* cmd
);

#endif

