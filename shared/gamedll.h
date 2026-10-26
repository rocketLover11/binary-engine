#pragma once

#include "interface.h"
#include "giface.h"

struct IServerGameDLL
{
	virtual bool Init(CreateInterfaceFn engineFactory) = 0;
	virtual void LevelInit(const char* mapName) = 0;
	virtual void GameFrame(bool simulating) = 0;
	virtual void Shutdown() = 0;
};

#define SERVER_GAMEDLL_VERSION "ServerGameDLL001"

struct IClientGameDLL
{
	virtual bool Init(CreateInterfaceFn engineFactory) = 0;
	virtual void Frame(float frameTime, float interpAlpha) = 0;
	virtual void DrawGui(IGui* gui) = 0;
	virtual void Shutdown() = 0;
};

#define CLIENT_GAMEDLL_VERSION "ClientGameDLL002"