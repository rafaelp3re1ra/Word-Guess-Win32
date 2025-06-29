#include "utils.h"

DWORD WINAPI receiveMessages(LPVOID lpParam);

HANDLE shutdownEv;

int _tmain(int argc, TCHAR* argv[]) {
#ifdef UNICODE
	(void)_setmode(_fileno(stdin), _O_WTEXT);
	(void)_setmode(_fileno(stdout), _O_WTEXT);
	(void)_setmode(_fileno(stderr), _O_WTEXT);
#endif

	if (argc != 2) {
		_tprintf(_T("Uso: %s [nome_do_jogador]\n"), argv[0]);
		return 1;
	}

	const TCHAR* playerName = argv[1];
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
			if (err == 2)
			{
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
	Message joinMsg = { JOIN_MSG };
	_tcscpy_s(joinMsg.userName, MAX_USERNAME, playerName);
	DWORD bytesWritten;
	if (!WriteFile(hPipe, &joinMsg, sizeof(Message), &bytesWritten, NULL)) {
		_tprintf(_T("Erro ao registrar jogador (%d)\n"), GetLastError());
		CloseHandle(hPipe);
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

	// Loop principal de comandos
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

		// Remover newline e espaços em branco
		input[_tcscspn(input, _T("\n"))] = '\0';
		_tcslwr_s(input, BUFFER); // Padronizar para minúsculas

		if (_tcslen(input) == 0) continue;

		Message msg = { 0 };
		_tcscpy_s(msg.userName, MAX_USERNAME, playerName);

		if (input[0] == _T(':')) { // Comando especial
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
				continue; // Não envia mensagem inválida
			}
		}
		else if (_tcscmp(input, _T("help")) == 0) {
			_tprintf(_T("Comandos disponíveis:\n\t - :jogs\n\t - :pont\n\t- :sair "));
		}
		else { // Tentativa de palavra
			msg.type = WORD_MSG;
			_tcsncpy_s(msg.content, BUFFER, input, _TRUNCATE);
			_tprintf(_T("Palavra enviada: %s"), msg.content);
		}

		// Enviar mensagem
		if (!WriteFile(hPipe, &msg, sizeof(Message), &bytesWritten, NULL)) {
			_tprintf(_T("Erro ao enviar mensagem. Erro: %d\n"), GetLastError());
			break;
		}

		if (msg.type == EXIT_MSG) {
			_tprintf(_T("A sair do jogo...\n"));
			SetEvent(shutdownEv);
		}
	}

	// Limpeza
	WaitForSingleObject(hThread, INFINITE);
	CloseHandle(hThread);
	CloseHandle(hPipe);
	ExitProcess(0);
}

DWORD WINAPI receiveMessages(LPVOID lpParam) {
	HANDLE hPipe = (HANDLE)lpParam;
	Message msg;
	DWORD bytesRead = 0;  // Inicializa aqui
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

		ResetEvent(hEvent); // limpar o evento

		BOOL result = ReadFile(hPipe, &msg, sizeof(Message), NULL, &ol);
		if (!result) {
			DWORD err = GetLastError();
			if (err == ERROR_IO_PENDING) {
				// espera evento
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
					//_tprintf(_T("Erro no GetOverlappedResult (%d)\n"), GetLastError());
					break;
				}
			}
			else if (err == ERROR_BROKEN_PIPE) {
				_tprintf(_T("\nConexão com o árbitro encerrada. Provavelmente encerrou inesperadamente!\n"));
				SetEvent(shutdownEv);
				break;
			}
			else {
				//_tprintf(_T("Erro no ReadFile (%d)\n"), err);
				break;
			}
		}
		else {
			// Se ReadFile retornou TRUE imediatamente, devemos chamar GetOverlappedResult para obter bytesRead
			if (!GetOverlappedResult(hPipe, &ol, &bytesRead, FALSE)) {
				//_tprintf(_T("Erro no GetOverlappedResult (imediato) (%d)\n"), GetLastError());
				break;
			}
		}

		if (bytesRead != sizeof(Message)) {
			_tprintf(_T("Mensagem incompleta recebida (%d bytes)!\n"), bytesRead);
			continue;
		}

		// Processa mensagem normalmente
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
		default:
			_tprintf(_T("\nMensagem desconhecida (Tipo %d)\n"), msg.type);
		}
	}

	CloseHandle(hEvent);
	return 0;
}
