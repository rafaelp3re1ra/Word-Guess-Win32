#include "utils.h"
#include "../data.h"

void getAndPrintLetrasFromSharedMemory() {
	HANDLE hMapFile = OpenFileMapping(
		FILE_MAP_ALL_ACCESS,
		FALSE,
		SHARED_MEM);

	if (hMapFile == NULL) {
		_tprintf(_T("Failed to open file mapping (%d).\n"), GetLastError());
		return FALSE;
	}

	SharedData* pShared = (SharedData*)MapViewOfFile(
		hMapFile,
		FILE_MAP_READ,
		0, 0, sizeof(SharedData));

	if (pShared == NULL) {
		printf("Failed to map view of file (%d).\n", GetLastError());
		CloseHandle(hMapFile);
		return FALSE;
	}
	_tprintf(_T("Letras: %s\n"), pShared->letters);
	UnmapViewOfFile(pShared);
	CloseHandle(hMapFile);

	return TRUE;
}

void fecharPipe(HANDLE hPipe) {
	CloseHandle(hPipe);
}