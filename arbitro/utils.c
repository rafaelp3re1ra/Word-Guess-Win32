#include "utils.h"
#include "players.h"
#include "threads.h"
#include "game.h"

Player players[MAX_PLAYERS];
int nPlayers = 0;
HANDLE mutex;
SharedData* pSharedData = NULL;

void sendMessageToPlayer(const TCHAR* userName, Message msg, HANDLE hPipe) {
	HANDLE targetPipe = hPipe;

	if (targetPipe == NULL) {
		int index = findUser(userName);
		if (index == -1) {
			_tprintf(_T("Utilizador %s não encontrado para enviar mensagem.\n"), userName);
			return;
		}
		targetPipe = players[index].hPipe;
	}

	if (!WriteFile(targetPipe, &msg, sizeof(Message), NULL, NULL)) {
		_tprintf(_T("Falha ao enviar mensagem para %s. Código: %d\n"), userName, GetLastError());
		return;
	}

}

void handleCommand(HANDLE hShutdownEvent) {
	TCHAR input[100];
	TCHAR cmd[50], arg1[100], arg2[100];

	_tprintf(_T("Bota aí um comando ('help'): "));

	if (_fgetts(input, 100, stdin) == NULL) {
		_tprintf(_T("Erro ao ler a entrada.\n"));
		return;
	}

	size_t len = _tcslen(input);
	if (len > 0 && input[len - 1] == _T('\n')) {
		input[len - 1] = _T('\0');
	}

	if (_tcslen(input) == 0) {
		_tprintf(_T("Entrada vazia, por favor insira um comando.\n"));
		return;
	}

	int numArgs = _stscanf_s(input, _T("%49s %99s %99s"),
		cmd, (unsigned)_countof(cmd),
		arg1, (unsigned)_countof(arg1),
		arg2, (unsigned)_countof(arg2));

	if (_tcscmp(cmd, _T("help")) == 0) {
		_tprintf(_T("Comandos disponíveis:\n"));
		_tprintf(_T(" - listar (lista todos os utilizadores)\n"));
		_tprintf(_T(" - excluir <userName> (remove um utilizador)\n"));
		_tprintf(_T(" - iniciabot <userName> <dificuldade> (adiciona um bot ao jogo)\n"));
		_tprintf(_T("   Dificuldades: 0=Fácil, 1=Médio, 2=Difícil\n"));
		_tprintf(_T(" - acelerar (aumenta o ritmo (diminui 1 segundo))\n"));
		_tprintf(_T(" - travar (diminui o ritmo (aumenta 1 segundo))\n"));
		_tprintf(_T(" - encerrar (encerra o manager)\n"));
	}
	else if (_tcscmp(cmd, _T("listar")) == 0) {
		listPlayers();
	}
	else if (_tcscmp(cmd, _T("excluir")) == 0) {
		if (numArgs < 2) {
			_tprintf(_T("Erro: Nome de utilizador não fornecido para o comando 'excluir'.\n"));
		}
		else {
			WaitForSingleObject(mutex, INFINITE);

			int pIndex = findUser(arg1);
			if (pIndex >= 0) {
				if (players[pIndex].hPipe == NULL || players[pIndex].hPipe == INVALID_HANDLE_VALUE) {
					_tprintf(_T("Erro: Pipe inválido para o jogador '%s'.\n"), players[pIndex].userName);
				}
				else {
					sendMessageToPlayer(NULL, (Message) { KICK_MSG, _T(""), _T("Foste expulso ahahah.") }, players[pIndex].hPipe);

					removePlayer(pIndex);

					Message msg;
					msg.type = NORMAL_MSG;
					_stprintf_s(msg.content, BUFFER, _T("Jogador %s foi removido do jogo.\n"), arg1);
					broadcastMessage(msg);
				}
			}
			else {
				_tprintf(_T("Erro: Jogador com o nome '%s' não encontrado.\n"), arg1);
			}

			ReleaseMutex(mutex);
		}
	}
	else if (_tcscmp(cmd, _T("iniciabot")) == 0) {
		if (numArgs < 3) {
			_tprintf(_T("Erro: Uso: iniciabot <nome> <dificuldade>\n"));
			_tprintf(_T("Dificuldades: 0=Fácil, 1=Médio, 2=Difícil\n"));
		}
		else {
			// Converter dificuldade para número
			int dificuldade = _ttoi(arg2);

			// Verificar se é número válido
			if (dificuldade < 0 || dificuldade > 2) {
				_tprintf(_T("Dificuldade inválida. Use: 0, 1 ou 2\n"));
			}
			else {
				STARTUPINFO si;
				PROCESS_INFORMATION pi;
				ZeroMemory(&si, sizeof(si));
				si.cb = sizeof(si);
				ZeroMemory(&pi, sizeof(pi));

				TCHAR commandLine[200];
				_stprintf_s(commandLine, 200, _T("bot.exe %s %d"), arg1, dificuldade);

				_tprintf(_T("Linha de comando: %s\n"), commandLine);
				if (CreateProcess(NULL, commandLine, NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi)) {
					_tprintf(_T("Bot iniciado com sucesso para '%s' (Dificuldade: %d).\n"), arg1, dificuldade);
					CloseHandle(pi.hProcess);
					CloseHandle(pi.hThread);
				}
				else {
					_tprintf(_T("Erro ao iniciar o bot para '%s'. Código: %d\n"), arg1, GetLastError());
				}
			}
		}
	}
	else if (_tcscmp(cmd, _T("acelerar")) == 0) {
		if (isGameRunning()) {
			_tprintf(_T("Velocidade mudada a %d segundos\n"), changeVelocity(REGISTRY_FASTER));
		}
		else {
			_tprintf(_T("Jogo ainda não iniciado\n"));
		}
	}
	else if (_tcscmp(cmd, _T("travar")) == 0) {
		if (isGameRunning()) {
			_tprintf(_T("Velocidade mudada a %d segundos\n"), changeVelocity(REGISTRY_SLOWER));
		}
		else {
			_tprintf(_T("Jogo ainda não iniciado\n"));
		}
	}
	else if (_tcscmp(cmd, _T("encerrar")) == 0) {
		shutdownManager(hShutdownEvent);
	}
	else {
		_tprintf(_T("Comando desconhecido: %s\n"), cmd);
	}
}

void listPlayers() {
	if (nPlayers == 0) {
		_tprintf(_T("Não existem jogadores ativos!\n"));
		return;
	}

	_tprintf(_T("Lista de jogadores (%d):\n"), nPlayers);

	for (int i = 0; i < nPlayers; i++) {
		_tprintf(_T(" - Nome: %s \t Pontuação: %.2f \t Tipo: %s\n"),
			players[i].userName,
			players[i].score,
			players[i].isBot == 0 ? _T("humano") : _T("bot"));
	}
}

managerHandles initializeManager() {
	managerHandles handles = { NULL, NULL, NULL, NULL };

	handles.hShutdownEvent = CreateEvent(NULL, TRUE, FALSE, SHUTDOWN_EV);
	if (handles.hShutdownEvent == NULL) {
		_tprintf(_T("Erro ao criar evento de shutdown. Erro: %d\n"), GetLastError());
		return handles;
	}

	handles.hGameStartEvent = CreateEvent(NULL, TRUE, FALSE, GAMESTART_EV);
	if (handles.hGameStartEvent == NULL) {
		_tprintf(_T("Erro ao criar o evento game start. Erro: %d\n"), GetLastError());
		shutdownManager(handles.hShutdownEvent);
		return handles;
	}

	handles.hConnectionThread = CreateThread(
		NULL, 0, connectionListener, handles.hShutdownEvent, 0, NULL
	);
	if (handles.hConnectionThread == NULL) {
		_tprintf(_T("Erro ao criar thread de conexão. Erro: %d\n"), GetLastError());
		shutdownManager(handles.hShutdownEvent);
		return handles;
	}

	// Provavelmente fazer uma função para inicializar isto tudo?
	handles.hSharedMemory = CreateFileMapping(
		INVALID_HANDLE_VALUE,
		NULL,
		PAGE_READWRITE,
		0,
		sizeof(SharedData),
		SHARED_MEM
	);
	if (handles.hSharedMemory == NULL) {
		_tprintf(_T("Erro ao criar a memória partilhada. Erro. %d"), GetLastError());
		shutdownManager(handles.hShutdownEvent);
		return handles;
	}

	pSharedData = (SharedData*)MapViewOfFile(
		handles.hSharedMemory,
		FILE_MAP_ALL_ACCESS,
		0, 0,
		sizeof(SharedData)
	);
	if (!pSharedData) {
		CloseHandle(handles.hSharedMemory);
		return handles;
	}

	// Inicializa tudo a zero: playerCount = 0 e limpa os slots
	memset(pSharedData, 0, sizeof(SharedData));

	return handles;
}

void cleanupManager(managerHandles h) {
	if (h.hShutdownEvent != NULL) {
		if (!CloseHandle(h.hShutdownEvent)) {
			_tprintf(_T("Erro ao fechar o handle de shutdown. Erro: %d\n"), GetLastError());
		}
		h.hShutdownEvent = NULL;
	}

	if (h.hGameStartEvent != NULL) {
		if (!CloseHandle(h.hGameStartEvent)) {
			_tprintf(_T("Erro ao fechar o handle de game start. Erro: %d\n"), GetLastError());
		}
		h.hGameStartEvent = NULL;
	}

	if (h.hConnectionThread != NULL) {
		if (!CloseHandle(h.hConnectionThread)) {
			_tprintf(_T("Erro ao fechar o handle da thread de conexão. Erro: %d\n"), GetLastError());
		}
		h.hConnectionThread = NULL;
	}
	if (h.hSharedMemory != NULL) {
		if (!CloseHandle(h.hSharedMemory)) {
			_tprintf(_T("Erro ao fechar o handle da shared memory. Erro: %d\n"), GetLastError());
		}
		h.hSharedMemory = NULL;
	}
	if (mutex != NULL) {
		if (!CloseHandle(mutex)) {
			_tprintf(_T("Erro ao fechar o mutex. Erro: %d\n"), GetLastError());
		}
		mutex = NULL;
	}
	if (pSharedData) {
		UnmapViewOfFile(pSharedData);
		pSharedData = NULL;
	}
	clearRegistry();
}

void shutdownManager(HANDLE hShutdownEvent) {
	broadcastMessage((Message) { MANAGER_SHUTDOWN, _T(""), _T("O árbitro encerrou.") });

	if (!SetEvent(hShutdownEvent)) {
		_tprintf(_T("Erro ao sinalizar o evento de encerramento. Erro: %d\n"), GetLastError());
	}
}

HANDLE openEvent(const TCHAR* eventName) {
	HANDLE hEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, eventName);
	if (hEvent == NULL) {
		_tprintf(_T("Erro ao abrir evento '%s'. Erro: %d\n"), eventName, GetLastError());
	}
	return hEvent;
}
