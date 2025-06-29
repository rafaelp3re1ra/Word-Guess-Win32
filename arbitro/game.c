#include "game.h"
#include "utils.h"
#include "players.h"

//Game state

GameState game;
extern SharedData* pSharedData;
TCHAR* dicionario[] = { _T("carro"), _T("cão"), _T("planta"), _T("sol"),_T("sofa"), _T("lua"), };
int nPalavras = 6;

void setGameRunning(BOOL running) {
	HANDLE hGameStarted = openEvent(GAMESTART_EV);
	game.gameRunning = running;

	if (running) {
		SetEvent(hGameStarted);     // Coloca como sinalizado (true)
	}
	else {
		ResetEvent(hGameStarted);   // Coloca como não sinalizado (false)
	}
}

BOOL isGameRunning() {
	if (game.gameRunning) {
		return TRUE;
	}
	return FALSE;
}

//Game logic

void enviarDicionarioParaBot(HANDLE hPipe) {
	TCHAR buffer[BUFFER] = { 0 };

	for (int i = 0; i < nPalavras; i++) {
		_tcscat_s(buffer, BUFFER, dicionario[i]);
		_tcscat_s(buffer, BUFFER, _T("\n"));
	}

	Message msg;
	msg.type = DICTIONARY;
	_tcscpy_s(msg.content, BUFFER, buffer);

	sendMessageToPlayer(NULL, msg, hPipe);
}

void refreshClientLetters() {
	Message msg;
	msg.type = REFRESH_LETTERS;
	broadcastMessage(msg);
}

void setupTimer(HANDLE* hTimer, int seconds) {
	if (hTimer != NULL) {
		*hTimer = CreateWaitableTimer(NULL, TRUE, NULL);
	}
	LARGE_INTEGER li;
	if (seconds < 1)
	{
		return;
	}
	li.QuadPart = -1 * seconds * 10000000;
	SetWaitableTimer(*hTimer, &li, 0, NULL, NULL, TRUE);
}

void refreshLatestLetter(int usedLetterIndex) {
	int maxLetters, temp;
	readFromRegistry(&maxLetters, NULL);
	if (pSharedData->letterCount == maxLetters) {
		for (int i = 0; i < pSharedData->letterCount; i++) {
			if (pSharedData->orderLetters[i] == usedLetterIndex) {
				int temp = pSharedData->orderLetters[i];
				for (int j = i; j < pSharedData->letterCount; j++) {
					pSharedData->orderLetters[j] = pSharedData->orderLetters[j + 1];
				}
				pSharedData->orderLetters[pSharedData->letterCount - 1] = temp;
				return;
			}
		}
	}
}

void genLetters() {
	srand(time(0));
	int randomWord, randomLetter;
	int tamPalavra;

	randomWord = rand() % (nPalavras);
	tamPalavra = _tcslen(dicionario[randomWord]);
	randomLetter = rand() % (tamPalavra);
	if (pSharedData->letterCount == MAX_LETRAS) {
		pSharedData->letters[pSharedData->orderLetters[0]] = dicionario[randomWord][randomLetter]; // No array de orderLetter a numero 0 deve ser sempre a mais antiga
		refreshLatestLetter(pSharedData->orderLetters[0]);
		return;
	}
	pSharedData->orderLetters[pSharedData->letterCount] = pSharedData->letterCount;
	pSharedData->letters[(pSharedData->letterCount)++] = dicionario[randomWord][randomLetter];
}

void subLetters(BOOL usedLetters[]) {
	srand(time(0));
	int randomWord, randomLetter;
	int tamPalavra;
	for (int i = 0; i < pSharedData->letterCount; i++) {
		if (usedLetters[i]) { // Se a letras esteve na palavra escolhida muda-a
			randomWord = rand() % (nPalavras - 1);
			tamPalavra = _tcslen(dicionario[randomWord]);
			randomLetter = rand() % (tamPalavra - 1);
			pSharedData->letters[i] = dicionario[randomWord][randomLetter]; // EScolhe uma letra random de uma palavra random
			refreshLatestLetter(i);
		}
	}
}

boolean isInVocab(TCHAR* palavra) {
	for (int i = 0; i < nPalavras; i++) {
		if (!_tcscmp(dicionario[i], palavra)) {
			return TRUE;
		}
	}
	return FALSE;
}

void processWord(int pIndex, TCHAR* palavra, HANDLE hpipe) {
	int tamPalavra = _tcslen(palavra);
	float pointsToAdd = 0;
	TCHAR* userName = pSharedData->players[pIndex].userName;
	BOOL* usedLetters = (BOOL*)malloc(pSharedData->letterCount * sizeof(BOOL));
	if (usedLetters == NULL) {
		_tprintf(_T("Erro na alocação de memoria"));
		exit(-1);
	}
	if (!isInVocab(palavra)) {
		pointsToAdd = _tcslen(palavra) * (-0.5);
	}
	else {
		for (int j = 0; j < pSharedData->letterCount; j++) { // Inicializa o used letters falso
			usedLetters[j] = FALSE;
		}
		for (int j = 0; j < pSharedData->letterCount; j++)
		{
			for (int i = 0; i < tamPalavra; i++) {

				if (pSharedData->letters[j] == palavra[i]) {
					refreshLatestLetter(j);
					pointsToAdd++;
					usedLetters[j] = TRUE; // como a letra foi usada vai ser mudada por outra (acontece depois)

					Message msg;
					msg.type = NORMAL_MSG;
					_stprintf_s(msg.content, BUFFER, _T("O jogador %s acertou uma palavra!\n"),
						players[pIndex].userName, nPlayers, MAX_PLAYERS);
					broadcastMessage(msg);
					break;
				}
			}
		}
		if (pointsToAdd == 0) {
			pointsToAdd = -(0.5 * _tcslen(palavra));
		}
	}
	increaseScore(pIndex, pointsToAdd);
	pSharedData->players[pIndex].score += pointsToAdd; // Atualizar o score do jogador na sharedMem
	subLetters(usedLetters);
	refreshClientLetters();
	free(usedLetters);
}

DWORD changeVelocity(TCHAR cadence) {
	DWORD velocity;
	readFromRegistry(NULL, &velocity);
	_tprintf(_T("Velocidade: %d\n"), velocity);
	if (cadence == REGISTRY_FASTER) {
		DWORD value = velocity - 1 < 1 ? 1 : velocity - 1;
		writeToRegistry(REGISTRY_NO_MAX, value);
		return value;
	}
	else if (cadence == REGISTRY_SLOWER) {
		writeToRegistry(REGISTRY_NO_MAX, velocity + 1);
		return velocity + 1;
	}
}

DWORD WINAPI gameThread(LPVOID lpParam) {
	HANDLE hGameStart = (HANDLE)lpParam;
	WaitForSingleObject(hGameStart, 0);
	HANDLE hTimer;
	DWORD velocity, maxLetters;
	//setupTimer(&hTimer, 10);

	DWORD result = WaitForSingleObject(hGameStart, INFINITE);
	for (int i = 0; i < MAX_LETRAS; i++) {
		pSharedData->letters[i] = _T('_');
	}
	if (result == WAIT_OBJECT_0) {
		_tprintf(_T("Tá set\n"));
	}
	else if (result == WAIT_TIMEOUT) {
		_tprintf(_T("Não tá set\n"));
	}
	else {
		_tprintf(_T("Erro: %d\n"), GetLastError());
	}

	while (!result) {
		if (nPlayers < 2) {
			setGameRunning(FALSE);
			WaitForSingleObject(hGameStart, INFINITE);
		}
		readFromRegistry(&maxLetters, &velocity);
		genLetters();
		_tprintf(_T("\n\rLetras: "));
		_tprintf(_T("%s"), pSharedData->letters);
		_tprintf(_T("\n"));
		refreshClientLetters();
		Sleep(velocity * 1000);
		//WaitForSingleObject(hTimer, INFINITE);
	}
	//CloseHandle(hTimer);
}

//Registry

void readFromRegistry(DWORD* maxLetras, DWORD* ritmo) {
	HKEY hKey = NULL;
	LONG result;

	result = RegOpenKeyEx(HKEY_CURRENT_USER, REGISTRY_KEY, 0, KEY_READ, &hKey);

	if (result == ERROR_SUCCESS) {

		if (maxLetras != NULL) {
			DWORD size = sizeof(DWORD);
			if (RegQueryValueEx(hKey, REGISTRY_MAX_LETTERS, NULL, NULL,
				(LPBYTE)maxLetras, &size) != ERROR_SUCCESS) {
				*maxLetras = 6;
			}
		}

		if (ritmo != NULL) {
			DWORD size = sizeof(DWORD);
			if (RegQueryValueEx(hKey, REGISTRY_RHYTHM, NULL, NULL,
				(LPBYTE)ritmo, &size) != ERROR_SUCCESS) {
				*ritmo = 3;
			}
		}
		RegCloseKey(hKey);
	}
	else {
		if (maxLetras != NULL) *maxLetras = MAX_LETRAS;
		if (ritmo != NULL) *ritmo = 3;

		writeToRegistry(*maxLetras, *ritmo);
	}
}

void writeToRegistry(DWORD maxLetras, DWORD ritmo) {
	HKEY hKey = NULL;
	LONG result;

	result = RegCreateKeyEx(HKEY_CURRENT_USER,
		REGISTRY_KEY,
		0,
		NULL,
		REG_OPTION_NON_VOLATILE,
		KEY_WRITE,
		NULL,
		&hKey,
		NULL);

	if (result != ERROR_SUCCESS) {
		return;
	}

	if (maxLetras == REGISTRY_NO_MAX) {
		RegSetValueEx(hKey, REGISTRY_RHYTHM, 0, REG_DWORD,
			(const BYTE*)&ritmo, sizeof(ritmo));
	}
	else {
		RegSetValueEx(hKey, REGISTRY_MAX_LETTERS, 0, REG_DWORD,
			(const BYTE*)&maxLetras, sizeof(maxLetras));
		RegSetValueEx(hKey, REGISTRY_RHYTHM, 0, REG_DWORD,
			(const BYTE*)&ritmo, sizeof(ritmo));
	}

	RegCloseKey(hKey);
}

void clearRegistry() {
	HKEY hKey = NULL;
	if (hKey != NULL)
		RegCloseKey(hKey);

	LSTATUS res = RegDeleteKeyEx(HKEY_CURRENT_USER, REGISTRY_KEY, KEY_ALL_ACCESS, 0);
	if (res == ERROR_SUCCESS) {
		_tprintf_s(_T("Chave apagada com sucesso\n"));
	}
}
