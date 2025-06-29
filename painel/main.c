#include <windows.h>
#include <tchar.h>
#include "../data.h"

HANDLE hMapFile = NULL;
SharedData* pSharedData = NULL;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
	case WM_PAINT: {
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hwnd, &ps);

		RECT clientRect;
		GetClientRect(hwnd, &clientRect);

		// Divide a janela em duas áreas: jogadores (70%) e letras (30%)
		RECT playersRect = clientRect;
		playersRect.right = clientRect.left + (clientRect.right - clientRect.left) * 7 / 10;
		RECT lettersRect = clientRect;
		lettersRect.left = playersRect.right + 10;

		// Criar fontes
		HFONT hFontTitle = CreateFont(28, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
			OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
			VARIABLE_PITCH, TEXT("Segoe UI"));
		HFONT hFontNormal = CreateFont(20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
			OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
			VARIABLE_PITCH, TEXT("Segoe UI"));

		// Fundo branco e texto preto
		SetBkColor(hdc, RGB(255, 255, 255));
		SetTextColor(hdc, RGB(0, 0, 0));
		ExtTextOut(hdc, 0, 0, ETO_OPAQUE, &clientRect, NULL, 0, NULL);

		// Título Jogadores
		SelectObject(hdc, hFontTitle);
		DrawText(hdc, _T("Jogadores Ativos"), -1, &playersRect, DT_LEFT | DT_TOP | DT_SINGLELINE);

		int titleHeight = 40; // Altura estimada do título
		playersRect.top += titleHeight + 10;

		// Lista jogadores
		SelectObject(hdc, hFontNormal);
		if (pSharedData == NULL || pSharedData->playerCount == 0) {
			DrawText(hdc, _T("Nenhum jogador ativo."), -1, &playersRect, DT_LEFT | DT_TOP | DT_WORDBREAK);
		}
		else {
			TCHAR buffer[2048] = { 0 };
			for (int i = 0; i < pSharedData->playerCount; i++) {
				_stprintf_s(buffer + _tcslen(buffer), 2048 - _tcslen(buffer),
					_T("%d. %s\n   Pontuação: %.2f\n   Tipo: %s\n\n"),
					i + 1,
					pSharedData->players[i].userName,
					pSharedData->players[i].score,
					pSharedData->players[i].isBot ? _T("Bot") : _T("Humano"));
			}
			DrawText(hdc, buffer, -1, &playersRect, DT_LEFT | DT_TOP | DT_WORDBREAK);
		}

		// Título Letras
		SelectObject(hdc, hFontTitle);
		DrawText(hdc, _T("Letras Ativas"), -1, &lettersRect, DT_LEFT | DT_TOP | DT_SINGLELINE);
		lettersRect.top += titleHeight + 10;

		// Letras numa linha, espaçadas
		SelectObject(hdc, hFontNormal);
		TCHAR lettersLine[128] = { 0 };
		int pos = 0;
		if (pSharedData != NULL && pSharedData->letterCount > 0) {
			for (int j = 0; j < pSharedData->letterCount; j++) {
				pos += _stprintf_s(lettersLine + pos, 128 - pos, _T("%c "), pSharedData->letters[j]);
			}
		}
		else {
			_tcscpy_s(lettersLine, 128, _T("Nenhuma letra ativa."));
		}
		DrawText(hdc, lettersLine, -1, &lettersRect, DT_LEFT | DT_TOP | DT_SINGLELINE);

		DeleteObject(hFontTitle);
		DeleteObject(hFontNormal);

		EndPaint(hwnd, &ps);
		return 0;
	}

	case WM_TIMER:
		InvalidateRect(hwnd, NULL, TRUE);
		return 0;

	case WM_SIZE:
		InvalidateRect(hwnd, NULL, TRUE);
		return 0;

	case WM_DESTROY:
		KillTimer(hwnd, 1);
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
	const TCHAR CLASS_NAME[] = _T("SampleWindowClass");

	WNDCLASS wc = { };
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = CLASS_NAME;

	RegisterClass(&wc);

	HWND hwnd = CreateWindowEx(
		0,
		CLASS_NAME,
		_T("Painel de Jogadores"),
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, 700, 450,
		NULL,
		NULL,
		hInstance,
		NULL
	);

	if (hwnd == NULL) {
		return 0;
	}

	// Abrir e mapear memória partilhada
	hMapFile = OpenFileMapping(FILE_MAP_READ, FALSE, SHARED_MEM);
	if (hMapFile == NULL) {
		MessageBox(NULL, _T("Falha ao abrir memória partilhada!"), _T("Erro"), MB_OK | MB_ICONERROR);
	}
	else {
		pSharedData = (SharedData*)MapViewOfFile(hMapFile, FILE_MAP_READ, 0, 0, sizeof(SharedData));
		if (pSharedData == NULL) {
			MessageBox(NULL, _T("Falha ao mapear a memória partilhada!"), _T("Erro"), MB_OK | MB_ICONERROR);
		}
	}

	ShowWindow(hwnd, nCmdShow);
	UpdateWindow(hwnd);

	// Timer para atualizar a tela a cada 1 segundo
	SetTimer(hwnd, 1, 1000, NULL);

	MSG msg = { };
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	// Limpeza
	if (pSharedData != NULL) {
		UnmapViewOfFile(pSharedData);
	}
	if (hMapFile != NULL) {
		CloseHandle(hMapFile);
	}

	return 0;
}
