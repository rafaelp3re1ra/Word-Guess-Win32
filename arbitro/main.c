#include "utils.h"
#include "includes.h"
#include "threads.h"
#include "players.h"
#include "game.h"

int _tmain(int argc, TCHAR* argv[]) {

#ifdef UNICODE 
	(void)_setmode(_fileno(stdin), _O_WTEXT);
	(void)_setmode(_fileno(stdout), _O_WTEXT);
	(void)_setmode(_fileno(stderr), _O_WTEXT);
#endif

	managerHandles handles = initializeManager();
	if (handles.hShutdownEvent == INVALID_HANDLE_VALUE) {
		_tprintf(_T("\nErro ao iniciar o manager.\n"));
		ExitProcess(1);
	}

	writeToRegistry(REGISTRY_MAX_LETTERS, 3);

	HANDLE gameThreadHandle;
	gameThreadHandle = CreateThread(
		NULL,
		0,
		gameThread,
		handles.hGameStartEvent,
		0,
		NULL
	);

	while (WaitForSingleObject(handles.hShutdownEvent, 0) != WAIT_OBJECT_0) {
		handleCommand(handles.hShutdownEvent);
	}

	WaitForSingleObject(handles.hConnectionThread, 0);
	WaitForSingleObject(gameThreadHandle, 0);

	cleanupManager(handles);

	ExitProcess(0);
}
