#ifndef PLAYERS_H
#define PLAYERS_H

#include "includes.h"

int addPlayer(HANDLE handlePipe, TCHAR* username, BOOL isBot);
void broadcastMessage(Message msg);
int findUser(const TCHAR* userName);
BOOL isUsernameAvailable(const TCHAR* username);
float getPlayerScore(int pIndex);
void removePlayer(int pIndex);
void handlePlayerMessage(Message msg, int pIndex);
void increaseScore(int pIndex, float score);

#endif // PLAYERS_H
