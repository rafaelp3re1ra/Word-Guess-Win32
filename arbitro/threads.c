#include "includes.h"
#include "players.h"
#include "threads.h"
#include "utils.h"

DWORD WINAPI connectionListener(LPVOID param) {
	HANDLE hShutdownEvent = (HANDLE)param;

	while (WaitForSingleObject(hShutdownEvent, 0) != WAIT_OBJECT_0) {
		HANDLE hPipe = CreateNamedPipe(
			PIPE_NAME,
			PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
			PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
			MAX_PLAYERS,
			BUFFER, BUFFER,
			0, NULL
		);
		if (hPipe == INVALID_HANDLE_VALUE) {
			// falhou, tenta de novo
			continue;
		}

		OVERLAPPED ov = { 0 };
		ov.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		if (ov.hEvent == NULL) {
			CloseHandle(hPipe);
			continue;
		}

		BOOL ok = ConnectNamedPipe(hPipe, &ov);
		DWORD err = ok ? 0 : GetLastError();

		if (!ok && err == ERROR_IO_PENDING) {
			HANDLE waits[2] = { ov.hEvent, hShutdownEvent };
			DWORD r = WaitForMultipleObjects(2, waits, FALSE, INFINITE);

			if (r == WAIT_OBJECT_0 + 1) {
				CancelIo(hPipe);
				CloseHandle(ov.hEvent);
				CloseHandle(hPipe);
				break;
			}
		}
		else if (!ok && err != ERROR_PIPE_CONNECTED) {
			// falha irreversível no connect
			CloseHandle(ov.hEvent);
			CloseHandle(hPipe);
			continue;
		}

		// connect bem-sucedido!
		CloseHandle(ov.hEvent);

		// handshake do JOIN_MSG
		Message joinMsg;
		DWORD  bytesRead;
		if (!ReadFile(hPipe, &joinMsg, sizeof(joinMsg), &bytesRead, NULL) ||
			bytesRead != sizeof(joinMsg) ||
			(joinMsg.type != JOIN_MSG && joinMsg.type != JOIN_MSG_B)) {
			DisconnectNamedPipe(hPipe);
			CloseHandle(hPipe);
			continue;
		}

		int pIndex = 0;
		if (joinMsg.type == JOIN_MSG_B) {
			pIndex = addPlayer(hPipe, joinMsg.userName, TRUE);
		}
		else {
			pIndex = addPlayer(hPipe, joinMsg.userName, FALSE);
		}

		if (pIndex < 0) {
			DisconnectNamedPipe(hPipe);
			CloseHandle(hPipe);
		}
		// se tudo OK, addPlayer já lançou o playerHandler…
	}

	for (int i = 0; i < nPlayers; i++) {
		WaitForSingleObject(players[i].hThread, INFINITE);
	}

	_tprintf(_T("Thread de conexão encerrada :D.\n"));
	return 0;
}

DWORD WINAPI playerHandler(LPVOID index) {
	int pIndex = (int)index;

	TCHAR userName[MAX_USERNAME];
	_tcscpy_s(userName, MAX_USERNAME, players[pIndex].userName);

	HANDLE hPipe = players[pIndex].hPipe;
	HANDLE hShutdownEvent = openEvent(SHUTDOWN_EV);

	HANDLE hExitEvent = players[pIndex].hExitEvent;

	Message msg;
	DWORD bytesRead;
	OVERLAPPED overlapped = { 0 };
	overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	if (!overlapped.hEvent) {
		_tprintf(_T("Erro ao criar evento OVERLAPPED para %s\n"), userName);
		ExitThread(1);
	}

	HANDLE events[3] = { hShutdownEvent, hExitEvent, overlapped.hEvent };

	_tprintf(_T("THREAD do jogador %s iniciada (index=%d)\n"), userName, pIndex);

	while (TRUE) {

		pIndex = findUser(userName);

		ResetEvent(overlapped.hEvent);

		BOOL readResult = ReadFile(hPipe, &msg, sizeof(Message), NULL, &overlapped);
		if (!readResult) {
			DWORD error = GetLastError();

			if (error == ERROR_IO_PENDING) {
				DWORD waitResult = WaitForMultipleObjects(3, events, FALSE, INFINITE);

				if (waitResult == WAIT_OBJECT_0 || waitResult == WAIT_OBJECT_0 + 1) {
					// Cancelar a leitura pendente e encerrar a thread
					CancelIo(hPipe);
					_tprintf(_T("Evento de saída recebido para %s. Encerrando a thread.\n"), userName);
					break;
				}
				else if (waitResult == WAIT_OBJECT_0 + 2) {
					// Verificar a leitura após a conclusão da operação assíncrona
					if (GetOverlappedResult(hPipe, &overlapped, &bytesRead, FALSE) && bytesRead == sizeof(Message)) {
						pIndex = findUser(userName);
						handlePlayerMessage(msg, pIndex); // Passa a mensagem lida para a função
					}
					else {
						//_tprintf(_T("Erro ao obter resultado da leitura para %s. Encerrando.\n"), userName);
						break;
					}
				}
				else {
					break;
				}
			}
			else {
				_tprintf(_T("Erro ao ler do pipe para %s. Erro: %d\n"), userName, error);
				break;
			}
		}
		else {
			// Leitura imediata (pouco comum)
			if (bytesRead == sizeof(Message)) {
				pIndex = findUser(userName);
				handlePlayerMessage(msg, pIndex); // Passa a mensagem lida para a função
			}
		}
	}

	DisconnectNamedPipe(hPipe);
	CloseHandle(hPipe);
	CloseHandle(overlapped.hEvent);

	ExitThread(0);
}
