#include "players.h"
#include "utils.h"
#include "includes.h"
#include "game.h"
#include "threads.h"

extern Player players[MAX_PLAYERS];
extern int    nPlayers;
extern HANDLE mutex;
extern SharedData* pSharedData;

void broadcastMessage(Message msg) {
	for (int i = 0; i < nPlayers; i++) {
		if (players[i].hPipe == NULL || players[i].hPipe == INVALID_HANDLE_VALUE) {
			_tprintf(_T("Pipe inválido para o jogador %s.\n"), players[i].userName);
			continue;
		}
		sendMessageToPlayer(players[i].userName, msg, 0);
	}
}

int findUser(const TCHAR* userName) {
	for (int i = 0; i < nPlayers; i++) {
		if (_tcscmp(userName, players[i].userName) == 0) {
			return i;
		}
	}
	return -1;
}

BOOL isUsernameAvailable(const TCHAR* username) {
	for (int i = 0; i < nPlayers; i++) {
		if (_tcscmp(players[i].userName, username) == 0) {
			_tprintf(_T("Username %s encontrado.\n"), username);
			return FALSE;
		}
	}
	return TRUE;
}

float getPlayerScore(int pIndex) {
	if (pIndex < 0 || pIndex >= nPlayers) {
		return 0;
	}
	return players[pIndex].score;
}

void syncPlayersToSharedMemory() {
	WaitForSingleObject(mutex, INFINITE);

	memcpy(pSharedData->players, players, sizeof(Player) * nPlayers);
	pSharedData->playerCount = nPlayers;

	ReleaseMutex(mutex);
}

void increaseScore(int pIndex, float score) {
	players[pIndex].score += score;
}

int addPlayer(HANDLE handlePipe, const TCHAR* username, BOOL isBot) {
	if (nPlayers >= MAX_PLAYERS) {
		sendMessageToPlayer(NULL,
			(Message) {
			KICK_MSG, _T(""), _T("\nTá cheio. Tenta mais tarde...\n")
		}, handlePipe);
		return -1;
	}
	if (!isUsernameAvailable(username)) {
		sendMessageToPlayer(NULL,
			(Message) {
			KICK_MSG, _T(""), _T("\nO username já existe. Tenta outro nome.\n")
		}, handlePipe);
		return -1;
	}

	WaitForSingleObject(mutex, INFINITE);

	int pIndex = nPlayers;

	// Inicializar campos da estrutura player
	players[pIndex].hPipe = handlePipe;
	_tcscpy_s(players[pIndex].userName, MAX_USERNAME, username);
	players[pIndex].score = 0;
	players[pIndex].isBot = isBot;

	// Evento sem nome para sinalizar saída da thread
	players[pIndex].hExitEvent = CreateEvent(
		NULL,
		TRUE,
		FALSE,
		NULL
	);

	nPlayers++;
	ReleaseMutex(mutex);

	sendMessageToPlayer(username,
		(Message) {
		NORMAL_MSG, _T(""), _T("Bem-vindo ao jogo!\n")
	}, NULL);

	_tprintf(_T("O jogador %s juntou-se ao jogo. [%d/%d]\n"),
		username, nPlayers, MAX_PLAYERS);

	Message joinMessage;
	joinMessage.type = NORMAL_MSG;
	_stprintf_s(joinMessage.content, BUFFER, _T("O jogador %s juntou-se ao jogo. [%d/%d]\n"),
		username, nPlayers, MAX_PLAYERS);
	broadcastMessage(joinMessage);

	// Lancemos a thread para o novo jogador
	players[pIndex].hThread = CreateThread(
		NULL, 0,
		playerHandler,
		(LPVOID)pIndex,
		0, NULL);

	WaitForSingleObject(mutex, INFINITE);
	if (pIndex >= 0 && players[pIndex].isBot) {
		enviarDicionarioParaBot(players[pIndex].hPipe);
	}
	ReleaseMutex(mutex);

	// Deve o jogo começar?
	if (nPlayers >= 2 && !isGameRunning()) {
		broadcastMessage((Message) { START_MSG, _T(""), _T("\nQue os jogos começem...\n") });
		setGameRunning(TRUE);
		pSharedData->gameRunning = TRUE;
	}

	syncPlayersToSharedMemory();

	return pIndex;
}

void removePlayer(int pIndex) {
	if (pIndex < 0 || pIndex >= nPlayers) {
		_tprintf(_T("Índice inválido para remoção de jogador.\n"));
		return;
	}

	WaitForSingleObject(mutex, INFINITE);

	TCHAR username[MAX_USERNAME];
	_tcscpy_s(username, MAX_USERNAME, players[pIndex].userName);

	SetEvent((players[pIndex].hExitEvent));

	CloseHandle(players[pIndex].hExitEvent);

	for (int i = pIndex; i < nPlayers - 1; i++) {
		players[i] = players[i + 1];
	}
	memset(&players[nPlayers - 1], 0, sizeof(Player));
	nPlayers--;

	ReleaseMutex(mutex);

	syncPlayersToSharedMemory();

	_tprintf(_T("Jogador %s removido.\n"), username);
}

void handlePlayerMessage(Message msg, int pIndex) {
	HANDLE hPipe = players[pIndex].hPipe;
	DWORD bytesRead;

	if (msg.type == LIST_REQ) {

		Message response = { LIST_RESP };
		TCHAR listInfo[BUFFER] = _T("Jogadores ativos:\n");

		WaitForSingleObject(mutex, INFINITE);
		for (int i = 0; i < nPlayers; ++i) {
			TCHAR playerEntry[100];
			_stprintf_s(
				playerEntry, 100,
				_T("\t- %s (Score: %.2f, %s)\n"),
				players[i].userName,
				players[i].score,
				players[i].isBot ? _T("Bot") : _T("Humano")
			);
			_tcscat_s(listInfo, BUFFER, playerEntry);
		}
		ReleaseMutex(mutex);

		_tcscpy_s(response.content, BUFFER, listInfo);

		sendMessageToPlayer(_T(""), response, hPipe);

	}
	else if (msg.type == SCORE_REQ) {
		float score = getPlayerScore(pIndex);
		Message response = { 0 };
		response.type = SCORE_REQ;
		_stprintf_s(response.content, BUFFER, _T("%.2f"), score);
		_tprintf(_T("Enviando score %.2f para jogador %s\n"), score, players[pIndex].userName);
		_tprintf(_T("Mensagem content: %s\n"), response.content);
		sendMessageToPlayer(_T(""), response, hPipe);

	}
	else if (msg.type == EXIT_MSG) {
		_tprintf(_T("Cliente %s saiu.\n"), players[pIndex].userName);
		Message msg;
		msg.type = NORMAL_MSG;
		_stprintf_s(msg.content, BUFFER, _T("Jogador %s saiu do jogo.\n"), players[pIndex].userName);
		removePlayer(pIndex);
		broadcastMessage(msg);
		return;
	}
	else if (msg.type == WORD_MSG) {
		processWord(pIndex, msg.content, hPipe);
	}
}
