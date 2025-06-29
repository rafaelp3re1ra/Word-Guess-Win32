#ifndef UTILS_H
#define UTILS_H

#include "includes.h"

//	Variáveis globais
extern Player players[MAX_PLAYERS];
extern int nPlayers;
extern HANDLE mutex;
extern SharedData* pSharedData;

typedef struct {
	HANDLE hShutdownEvent;
	HANDLE hGameStartEvent;
	HANDLE hConnectionThread;
	HANDLE hSharedMemory;
} managerHandles;

void sendMessageToPlayer(const TCHAR* userName, Message msg, HANDLE hPipe);
void handleCommand(HANDLE hShutdownEvent);
managerHandles initializeManager();
void cleanupManager(managerHandles h);
void shutdownManager(HANDLE hShutdownEvent);
void listPlayers();
HANDLE openEvent(const TCHAR* eventName);

#endif // UTILS_H
