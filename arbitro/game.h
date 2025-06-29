#ifndef LOGIC_H
#define LOGIC_H

#include "includes.h"

typedef struct {
	char letters[12];  // Max 12 letters visible
	int letterCount;
	char lastWord[BUFFER];
	int maxLetters;
	int rhythm;
	BOOL gameRunning;
	int leaderIndex;  // Index of the player with highest score
} GameState;

// Game state
void setGameRunning(BOOL running);
BOOL isGameRunning();

// Game logic
void setupTimer(HANDLE* hTimer, int seconds);
void refreshLatestLetter(int usedLetterIndex);
void genLetters();
void subLetters(BOOL usedLetters[]);
void processWord(int pIndex, TCHAR* palavra, HANDLE hpipe);
DWORD changeVelocity(TCHAR cadence);
void refreshClientLetters();
DWORD WINAPI gameThread(LPVOID lpParam);
void enviarDicionarioParaBot(HANDLE hPipe);

// Registry
void readFromRegistry(DWORD* maxLetras, DWORD* ritmo);
void writeToRegistry(DWORD maxLetras, DWORD ritmo);
void clearRegistry();

#endif // LOGIC_H
