#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include <time.h>
#include <stdlib.h>
#include "../data.h"

// Níveis de dificuldade
#define EASY 0
#define MEDIUM 1
#define HARD 2

// Variáveis globais para o bot
HANDLE hPipeMutex;
TCHAR** dictionary = NULL;
int dictionarySize = 0;
int botDifficulty = MEDIUM; // Padrão: médio
HANDLE shutdownEv;
TCHAR playerName[MAX_USERNAME];
HANDLE hMapFile = NULL;
SharedData* pShared = NULL;

DWORD WINAPI receiveMessages(LPVOID lpParam);
DWORD WINAPI botAutoPlay(LPVOID lpParam);
void parseDictionary(const TCHAR* dictString);
void cleanupResources();
BOOL isGameRunning();
BOOL openSharedMemory();

void getAndPrintLetrasFromSharedMemory() {
	if (!pShared) {
		if (!openSharedMemory()) {
			return;
		}
	}
	_tprintf(_T("Letras: %s\n"), pShared->letters);
}

void fecharPipe(HANDLE hPipe) {
	CloseHandle(hPipe);
}

// Função para abrir a memória compartilhada
BOOL openSharedMemory() {
	if (hMapFile && pShared) return TRUE; // Já aberta

	hMapFile = OpenFileMapping(
		FILE_MAP_READ,
		FALSE,
		SHARED_MEM);

	if (hMapFile == NULL) {
		_tprintf(_T("Failed to open file mapping (%d).\n"), GetLastError());
		return FALSE;
	}

	pShared = (SharedData*)MapViewOfFile(
		hMapFile,
		FILE_MAP_READ,
		0, 0, sizeof(SharedData));

	if (pShared == NULL) {
		_tprintf(_T("Failed to map view of file (%d).\n"), GetLastError());
		CloseHandle(hMapFile);
		hMapFile = NULL;
		return FALSE;
	}

	return TRUE;
}

BOOL isGameRunning() {
	if (!pShared) {
		if (!openSharedMemory()) {
			return FALSE;
		}
	}
	return pShared->gameRunning;
}

int _tmain(int argc, TCHAR* argv[]) {
#ifdef UNICODE
	(void)_setmode(_fileno(stdin), _O_WTEXT);
	(void)_setmode(_fileno(stdout), _O_WTEXT);
	(void)_setmode(_fileno(stderr), _O_WTEXT);
#endif

	// Verificar argumentos
	if (argc < 2 || argc > 3) {
		_tprintf(_T("Uso: %s [nome_do_jogador] [dificuldade]\n"), argv[0]);
		_tprintf(_T("Dificuldades: 0=Fácil, 1=Médio, 2=Difícil\n"));
		return 1;
	}

	_tcscpy_s(playerName, MAX_USERNAME, argv[1]);

	// Processar dificuldade se fornecida
	if (argc == 3) {
		botDifficulty = _ttoi(argv[2]);
		if (botDifficulty < 0 || botDifficulty > 2) {
			_tprintf(_T("Dificuldade inválida. Usando padrão (Médio).\n"));
			botDifficulty = MEDIUM;
		}
	}

	HANDLE hPipe = INVALID_HANDLE_VALUE;

	// Conectar ao pipe do árbitro
	while (1) {
		hPipe = CreateFile(
			PIPE_NAME,
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL,
			OPEN_EXISTING,
			FILE_FLAG_OVERLAPPED,
			NULL
		);

		if (hPipe != INVALID_HANDLE_VALUE) break;

		DWORD err = GetLastError();
		if (err == ERROR_PIPE_BUSY) {
			if (!WaitNamedPipe(PIPE_NAME, 5000)) {
				_tprintf(_T("Tempo de espera excedido.\n"));
				ExitProcess(1);
			}
		}
		else {
			if (err == 2) {
				_tprintf(_T("Não se existe nenhum jogo a decorrer. Tenta novamente mais tarde.\n"));
				ExitProcess(1);
			}
			else {
				_tprintf(_T("Erro ao conectar (%d)\n"), err);
				ExitProcess(1);
			}
		}
	}

	// Configurar modo de leitura do pipe
	DWORD mode = PIPE_READMODE_MESSAGE;
	if (!SetNamedPipeHandleState(hPipe, &mode, NULL, NULL)) {
		_tprintf(_T("Erro ao configurar o pipe (%d)\n"), GetLastError());
		CloseHandle(hPipe);
		ExitProcess(1);
	}

	// Enviar mensagem de JOIN
	Message joinMsg = { JOIN_MSG_B };
	_tcscpy_s(joinMsg.userName, MAX_USERNAME, playerName);
	DWORD bytesWritten;
	if (!WriteFile(hPipe, &joinMsg, sizeof(Message), &bytesWritten, NULL)) {
		_tprintf(_T("Erro ao registrar jogador (%d)\n"), GetLastError());
		CloseHandle(hPipe);
		ExitProcess(1);
	}

	// Criar mutex para acesso seguro ao pipe
	hPipeMutex = CreateMutex(NULL, FALSE, NULL);
	if (hPipeMutex == NULL) {
		_tprintf(_T("Erro ao criar mutex. Erro: %d\n"), GetLastError());
		ExitProcess(1);
	}

	// Abrir memória compartilhada
	if (!openSharedMemory()) {
		_tprintf(_T("Falha ao acessar memória compartilhada. Encerrando.\n"));
		ExitProcess(1);
	}

	// Iniciar thread de receção
	HANDLE hThread = CreateThread(NULL, 0, receiveMessages, hPipe, 0, NULL);
	if (hThread == NULL) {
		_tprintf(_T("Erro ao criar thread (%d)\n"), GetLastError());
		CloseHandle(hPipe);
		ExitProcess(1);
	}

	shutdownEv = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (shutdownEv == NULL) {
		_tprintf(_T("Erro ao criar evento de shutdown. Erro: %d\n"), GetLastError());
		ExitProcess(1);
	}

	// Iniciar thread de autoplay
	HANDLE hBotThread = CreateThread(NULL, 0, botAutoPlay, hPipe, 0, NULL);
	if (hBotThread == NULL) {
		_tprintf(_T("Erro ao criar thread do bot. Erro: %d\n"), GetLastError());
		CloseHandle(hPipe);
		ExitProcess(1);
	}

	// Loop principal apenas para comandos especiais
	TCHAR input[BUFFER];
	while (1) {
		if (WaitForSingleObject(shutdownEv, 1) == WAIT_OBJECT_0) {
			_tprintf(_T("A encerrar...\n"));
			break;
		}

		_tprintf(_T("\n> "));
		if (_fgetts(input, BUFFER, stdin) == NULL) {
			_tprintf(_T("Erro ao ler entrada.\n"));
			continue;
		}

		// Processar apenas comandos especiais
		input[_tcscspn(input, _T("\n"))] = '\0';
		_tcslwr_s(input, BUFFER);

		if (input[0] == _T(':')) {
			Message msg = { 0 };
			_tcscpy_s(msg.userName, MAX_USERNAME, playerName);

			if (_tcscmp(input, _T(":pont")) == 0) {
				msg.type = SCORE_REQ;
			}
			else if (_tcscmp(input, _T(":jogs")) == 0) {
				msg.type = LIST_REQ;
			}
			else if (_tcscmp(input, _T(":sair")) == 0) {
				msg.type = EXIT_MSG;
			}
			else {
				_tprintf(_T("Comando desconhecido: %s\n"), input);
				continue;
			}

			// Enviar mensagem
			WaitForSingleObject(hPipeMutex, INFINITE);
			DWORD bytesWritten;
			if (!WriteFile(hPipe, &msg, sizeof(Message), &bytesWritten, NULL)) {
				_tprintf(_T("Erro ao enviar mensagem. Erro: %d\n"), GetLastError());
			}
			ReleaseMutex(hPipeMutex);

			if (msg.type == EXIT_MSG) {
				SetEvent(shutdownEv);
			}
		}
		else if (_tcscmp(input, _T("help")) == 0) {
			_tprintf(_T("Comandos disponíveis:\n\t- :jogs\n\t- :pont\n\t- :sair\n"));
		}
		else {
			_tprintf(_T("(Bot em modo automático - use comandos com ':')\n"));
		}
	}

	// Limpeza
	WaitForSingleObject(hThread, INFINITE);
	WaitForSingleObject(hBotThread, INFINITE);
	CloseHandle(hThread);
	CloseHandle(hBotThread);
	CloseHandle(hPipe);
	CloseHandle(hPipeMutex);
	CloseHandle(shutdownEv);

	cleanupResources();

	if (pShared) {
		UnmapViewOfFile(pShared);
		pShared = NULL;
	}
	if (hMapFile) {
		CloseHandle(hMapFile);
		hMapFile = NULL;
	}

	ExitProcess(0);
}

// Thread de autoplay do bot
DWORD WINAPI botAutoPlay(LPVOID lpParam) {
	HANDLE hPipe = (HANDLE)lpParam;
	srand((unsigned int)time(NULL));

	while (1) {
		DWORD waitTime = 1000 + (rand() % 9000);
		if (WaitForSingleObject(shutdownEv, waitTime) == WAIT_OBJECT_0) {
			break;
		}

		// Verificar se o jogo está em execução
		if (!isGameRunning()) {
			_tprintf(_T("Jogo não está em execução...\n"));
			continue;
		}

		// Só começa a jogar quando tiver dicionário
		if (dictionarySize == 0) {
			_tprintf(_T("Dicionário vazio...\n"));
			continue;
		}

		TCHAR wordToSend[BUFFER];
		int randVal = rand() % 100;
		int probability = 0;

		// Definir probabilidade baseada na dificuldade
		switch (botDifficulty) {
		case EASY: probability = 30; break;
		case MEDIUM: probability = 60; break;
		case HARD: probability = 90; break;
		}

		if (randVal < probability) {
			// Escolher palavra aleatória do dicionário
			int wordIndex = rand() % dictionarySize;
			_tcscpy_s(wordToSend, BUFFER, dictionary[wordIndex]);
		}
		else {
			// Gerar palavra aleatória (3-7 letras)
			int len = 3 + (rand() % 5);
			for (int i = 0; i < len; i++) {
				wordToSend[i] = _T('a') + (rand() % 26);
			}
			wordToSend[len] = _T('\0');
		}

		Message msg = { WORD_MSG };
		_tcscpy_s(msg.userName, MAX_USERNAME, playerName);
		_tcscpy_s(msg.content, BUFFER, wordToSend);

		WaitForSingleObject(hPipeMutex, INFINITE);
		DWORD bytesWritten;
		if (!WriteFile(hPipe, &msg, sizeof(Message), &bytesWritten, NULL)) {
			_tprintf(_T("Erro ao enviar palavra. Erro: %d\n"), GetLastError());
		}
		else {
			_tprintf(_T("Bot enviou: %s\n"), wordToSend);
		}
		ReleaseMutex(hPipeMutex);
	}
	return 0;
}

void parseDictionary(const TCHAR* dictString) {
	if (dictString == NULL || _tcslen(dictString) == 0) {
		_tprintf(_T("String do dicionário vazia\n"));
		return;
	}

	TCHAR* buffer = _tcsdup(dictString);
	if (!buffer) {
		_tprintf(_T("Erro ao alocar memória para parsing do dicionário\n"));
		return;
	}

	// Contar palavras
	TCHAR* context = NULL;
	TCHAR* token = _tcstok_s(buffer, _T("\n"), &context);
	int count = 0;

	while (token != NULL) {
		count++;
		token = _tcstok_s(NULL, _T("\n"), &context);
	}

	// Alocar memória para array de palavras
	dictionary = (TCHAR**)malloc(count * sizeof(TCHAR*));
	if (!dictionary) {
		free(buffer);
		return;
	}

	_tcscpy_s(buffer, _tcslen(dictString) + 1, dictString);
	context = NULL;
	token = _tcstok_s(buffer, _T("\n"), &context);
	int index = 0;

	while (token != NULL && index < count) {
		dictionary[index] = _tcsdup(token);
		if (!dictionary[index]) {
			for (int i = 0; i < index; i++) free(dictionary[i]);
			free(dictionary);
			free(buffer);
			dictionary = NULL;
			dictionarySize = 0;
			return;
		}
		index++;
		token = _tcstok_s(NULL, _T("\n"), &context);
	}

	free(buffer);
	dictionarySize = count;
	_tprintf(_T("Dicionário carregado com %d palavras\n"), dictionarySize);
}

void cleanupResources() {
	if (dictionary != NULL) {
		for (int i = 0; i < dictionarySize; i++) {
			if (dictionary[i] != NULL) {
				free(dictionary[i]);
			}
		}
		free(dictionary);
		dictionary = NULL;
		dictionarySize = 0;
	}
}

DWORD WINAPI receiveMessages(LPVOID lpParam) {
	HANDLE hPipe = (HANDLE)lpParam;
	Message msg;
	DWORD bytesRead = 0;
	OVERLAPPED ol = { 0 };
	HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (!hEvent) {
		_tprintf(_T("Erro ao criar evento OVERLAPPED: %d\n"), GetLastError());
		return 1;
	}
	ol.hEvent = hEvent;

	while (1) {
		if (WaitForSingleObject(shutdownEv, 0) == WAIT_OBJECT_0)
			break;

		ResetEvent(hEvent);

		BOOL result = ReadFile(hPipe, &msg, sizeof(Message), NULL, &ol);
		if (!result) {
			DWORD err = GetLastError();
			if (err == ERROR_IO_PENDING) {
				DWORD wait = WaitForSingleObject(hEvent, INFINITE);
				if (wait != WAIT_OBJECT_0) {
					_tprintf(_T("Erro na espera do ReadFile OVERLAPPED\n"));
					break;
				}
				if (!GetOverlappedResult(hPipe, &ol, &bytesRead, FALSE)) {
					if (GetLastError() == ERROR_BROKEN_PIPE) {
						_tprintf(_T("\nConexão com o árbitro encerrada. Provavelmente encerrou inesperadamente!\n"));
						SetEvent(shutdownEv);
						break;
					}
					_tprintf(_T("Erro no GetOverlappedResult (%d)\n"), GetLastError());
					break;
				}
			}
			else if (err == ERROR_BROKEN_PIPE) {
				_tprintf(_T("\nConexão com o árbitro encerrada. Provavelmente encerrou inesperadamente!\n"));
				SetEvent(shutdownEv);
				break;
			}
			else {
				_tprintf(_T("Erro no ReadFile (%d)\n"), err);
				break;
			}
		}
		else {
			if (!GetOverlappedResult(hPipe, &ol, &bytesRead, FALSE)) {
				_tprintf(_T("Erro no GetOverlappedResult (imediato) (%d)\n"), GetLastError());
				break;
			}
		}

		if (bytesRead != sizeof(Message)) {
			_tprintf(_T("Mensagem incompleta recebida (%d bytes)!\n"), bytesRead);
			continue;
		}

		// Processa mensagem
		switch (msg.type) {
		case LIST_RESP:
			_tprintf(_T("\n=== LISTA DE JOGADORES ===\n%s"), msg.content);
			break;
		case SCORE_REQ:
			_tprintf(_T("\nPontuação atual: %s\n"), msg.content);
			break;
		case NORMAL_MSG:
			_tprintf(_T("\n[NOTIFICAÇÃO] %s\n"), msg.content);
			break;
		case ERROR_MSG:
			_tprintf(_T("\n[ERRO] %s\n"), msg.content);
			break;
		case KICK_MSG:
			_tprintf(_T("\n[EXPULSÃO] %s\n"), msg.content);
			SetEvent(shutdownEv);
			break;
		case START_MSG:
			_tprintf(_T("\n=== QUE OS JOGOS COMECEM ===\n"));
			break;
		case MANAGER_SHUTDOWN:
			_tprintf(_T("\n[ÁRBITRO] %s\n"), msg.content);
			SetEvent(shutdownEv);
			break;
		case REFRESH_LETTERS:
			getAndPrintLetrasFromSharedMemory();
			break;
		case DICTIONARY:
			parseDictionary(msg.content);
			break;
		default:
			_tprintf(_T("\nMensagem desconhecida (Tipo %d)\n"), msg.type);
		}
	}

	CloseHandle(hEvent);
	return 0;
}